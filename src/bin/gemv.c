#include <linux/slab.h>
#include <linux/uaccess.h>

#include "../../include/microkernels/kernel_datastructures.h"
#include "../../include/microkernels/kernels.h"
#include "../../include/pim_configs.h"
#include "../../include/pim_data_allocator.h"
#include "../../include/pim_init_state.h"
#include "../../include/pim_matrices.h"
#include "../../include/pim_memory_region.h"
#include "../../include/pim_vectors.h"
#include "../../include/read_write_triggers.h"

#define REPETITIONS 8
#define F16_ONE 0x3C00

#define FIX_POINT_SHIFT 10
#define FIX_POINT_MASK ((1 << FIX_POINT_SHIFT) - 1)

/**
 * Context structure to encapsulate all state variables for an operation.
 * => Avoids global variables
 */
struct gemv_context {
    // Result pointers
    int32_t *result_integer_part;
    int32_t *result_fractional_part;
    uint16_t *polished_result;

    // Current indices for the result arrays
    int index_result_fractured;
    int index_result_polished;

    int repetitions;
};

/**
 * Converts a fixed-point value to a 16-bit IEEE 754 floating-point
 * representation (FP16).
 */
static uint16_t fixed_to_f16_engine(uint64_t fixed_val) {
    int msb_pos;
    int actual_exponent;
    int biased_exponent;
    int shift;
    uint16_t mantissa;
    uint64_t shifted_val;

    if (fixed_val == 0) {
        return 0;
    }

    msb_pos = 0;
    for (int i = 63; i >= 0; i--) {
        if ((fixed_val >> i) & 1) {
            msb_pos = i;
            break;
        }
    }

    actual_exponent = msb_pos - FIX_POINT_SHIFT;
    biased_exponent = actual_exponent + 15;

    if (biased_exponent >= 0x1F) {
        return 0x7C00; // Infinity
    }

    if (biased_exponent > 0) { // Normalized
        shift = msb_pos - 10;
        mantissa = (shift >= 0) ? (fixed_val >> shift) & 0x3FF
                                : (fixed_val << -shift) & 0x3FF;
        return (biased_exponent << 10) | mantissa;

    } else { // Denormalized
        shift = 1 - biased_exponent;

        if (shift > msb_pos + 1) {
            return 0; // Underflow
        }

        shifted_val = fixed_val >> (msb_pos - 10 + shift);
        return (uint16_t)(shifted_val & 0x3FF);
    }
}

/**
 * Converts separate integer and fractional parts into a single 16-bit FP16
 * value.
 */
static uint16_t kernel_parts_to_f16(int32_t integer_part,
                                    int32_t fractional_part) {
    uint16_t sign = 0;
    uint32_t binary_fractional_part;
    uint64_t fixed_val;
    uint16_t magnitude_f16;

    // Get sign
    if (integer_part < 0) {
        sign = 0x8000;
        integer_part = -integer_part;
    }
    if (fractional_part < 0) {
        fractional_part = -fractional_part;
    }

    binary_fractional_part = ((uint64_t)fractional_part * 64) / 1000;

    // Combining the two parts
    fixed_val = ((uint64_t)integer_part << FIX_POINT_SHIFT) |
                (binary_fractional_part & 0x3FF);

    magnitude_f16 = fixed_to_f16_engine(fixed_val);

    return sign | magnitude_f16;
}

/**
 * Converts a uint16_t representing an IEEE 754 half-precision float (f16) into
 * its 32-bit signed fixed-point integer equivalent, including handling of
 * special values for zero, denormals, and infinity.
 */
static inline int32_t f16_to_fixed_point(uint16_t f16_val) {
    int32_t sign = (f16_val >> 15) & 0x01;
    int32_t exponent = (f16_val >> 10) & 0x1f;
    int32_t mantissa = f16_val & 0x3ff;
    int32_t denorm_val;
    int32_t value;
    int32_t shift;

    if (exponent == 0x1f) {  // Infinity or NaN
        if (mantissa == 0) { // Infinity
            return sign ? INT_MIN : INT_MAX;
        }
        return 0; // NaN -> 0
    }

    if (exponent == 0) {     // Zero or Denormalized
        if (mantissa == 0) { // Zero
            return 0;
        }
        // Denormalized
        denorm_val = mantissa;
        denorm_val = denorm_val >> (14 - FIX_POINT_SHIFT);
        return sign ? -denorm_val : denorm_val;
    }

    // Normalized
    value = (1 << FIX_POINT_SHIFT) | mantissa;

    shift = exponent - 15;

    if (shift > 0) {
        if (shift > 20) { // Avoid overflow
            value = sign ? INT_MIN : INT_MAX;
        } else {
            value <<= shift;
        }
    } else {
        value >>= -shift;
    }

    if (sign) {
        value = -value;
    }

    return value;
}

/**
 * Accumulates the partial sum vectors from PIM memory to compute the final
 * scalar result for each output row. This reduction is performed on the CPU by
 * it first converting the f16 partial sums into a fixed-point representation
 * before summation.
 */
static void
accumulate_result_vector(struct gemv_context *ctx,
                         uint16_t __iomem *output_partial_sum_vector) {
    for (int i = 0; i < MATRIX_ROWS; i++) {
        int64_t accumulated_row = 0;
        for (int j = 0; j < ELEMENT_COUNT_SUBMATRIX; j++) {
            int32_t val =
                f16_to_fixed_point(ioread16(output_partial_sum_vector));
            accumulated_row += val;
            output_partial_sum_vector++;
        }
        ctx->result_integer_part[ctx->index_result_fractured] =
            (accumulated_row >> FIX_POINT_SHIFT);
        ctx->result_fractional_part[ctx->index_result_fractured] =
            (((abs(accumulated_row) & FIX_POINT_MASK) * 1000) >>
             FIX_POINT_SHIFT);
        ctx->index_result_fractured++;
    }
}

/**
 * Orchestrates a complete matrix-vector multiplication (GEMV) on the PIM units
 * by issuing a sequence of memory-mapped trigger reads and writes. This
 * sequence loads the input vector (MOV), processes the matrix data (MAC),
 * writes back the result vector (FILL), and finally resets the PIM units
 * (EXIT).
 */
static void gemv_execute(uint16_t __iomem *matrix_base_addr,
                         uint16_t __iomem *input_vector_base_addr,
                         uint16_t __iomem *output_vector_base_addr,
                         uint16_t __iomem *dummy_region_address) {
    const size_t INPUT_VECTOR_BLOCK_STRIDE =
        NUM_BANKS * ELEMENT_COUNT_SUBMATRIX;
    const size_t BLOCK_IN_STRIPE_STRIDE =
        ELEMENT_COUNT_SUBMATRIX * ELEMENT_COUNT_SUBMATRIX;
    const size_t NUM_ELEMENTS_IN_COL =
        ELEMENT_COUNT_SUBMATRIX * ELEMENT_COUNT_SUBMATRIX;
    const size_t NUM_ELEMENTS_IN_STRIPE = NUM_ELEMENTS_IN_COL * X16_COLUMNS;

    // === 1. Read input vector ===
    for (int i = 0; i < X16_COLUMNS; ++i) {
        trigger_read(input_vector_base_addr + i * INPUT_VECTOR_BLOCK_STRIDE);
    }

    // === 2. Execute Multiply and Accumulates ===
    for (int i = 0; i < X16_ROWS; ++i) {
        for (int j = 0; j < X16_COLUMNS; ++j) {
            size_t offset =
                (i * NUM_ELEMENTS_IN_STRIPE) + (j * NUM_ELEMENTS_IN_COL);
            uint16_t __iomem *block_addr = matrix_base_addr + offset;
            trigger_read(block_addr);
        }
    }

    dsb(sy);

    // === 3. Execute FILLs for writing the output vector ===
    for (int i = 0; i < X16_COLUMNS; ++i) {
        trigger_write(output_vector_base_addr + i * BLOCK_IN_STRIPE_STRIDE);
    }

    // === 4. Trigger EXIT ===
    trigger_read(dummy_region_address);
}

/**
 * Partitions a large source matrix into multiple, newly allocated 64x128
 * sub-matrices. It returns a dynamically allocated array of pointers to these
 * chunks and writes the total number of chunks into the total_chunks_out
 * parameter.
 */
static uint16_t **divide_matrix_in_pim_chunks(uint16_t *matrix, uint32_t rows,
                                              uint32_t cols,
                                              uint32_t *total_chunks_out) {

    uint32_t num_row_chunks;
    uint32_t num_col_chunks;
    uint32_t total_chunks;

    uint16_t **chunk_pointers;
    const size_t chunk_size_bytes = 64 * 128 * sizeof(uint16_t);
    const size_t row_size_bytes = 128 * sizeof(uint16_t);

    if (rows % 64 != 0 || cols % 128 != 0) {
        pr_err("Matrix dimensions must be a multiple of 64x128.\n");
        return NULL;
    }

    // ---- Calculate chunk count and allocate pointer array ----
    num_row_chunks = rows / 64;
    num_col_chunks = cols / 128;
    total_chunks = num_row_chunks * num_col_chunks;

    chunk_pointers =
        kmalloc_array(total_chunks, sizeof(uint16_t *), GFP_KERNEL);
    if (!chunk_pointers) {
        return NULL;
    }

    // ---- Copy data ----
    for (uint32_t r_chunk = 0; r_chunk < num_row_chunks; ++r_chunk) {
        for (uint32_t c_chunk = 0; c_chunk < num_col_chunks; ++c_chunk) {
            uint32_t chunk_index = r_chunk * num_col_chunks + c_chunk;

            chunk_pointers[chunk_index] = kmalloc(chunk_size_bytes, GFP_KERNEL);
            if (!chunk_pointers[chunk_index]) {
                for (uint32_t i = 0; i < chunk_index; ++i)
                    kfree(chunk_pointers[i]);
                kfree(chunk_pointers);
                return NULL;
            }

            // Copy the data for the current chunk row by row
            for (uint32_t row_in_chunk = 0; row_in_chunk < 64; ++row_in_chunk) {
                uint32_t src_row = r_chunk * 64 + row_in_chunk;
                uint32_t src_col = c_chunk * 128;
                uint16_t *src_ptr = matrix + (src_row * cols + src_col);
                uint16_t *dest_ptr =
                    chunk_pointers[chunk_index] + (row_in_chunk * 128);
                memcpy(dest_ptr, src_ptr, row_size_bytes);
            }
        }
    }

    *total_chunks_out = num_row_chunks;
    return chunk_pointers;
}

/**
 * Deallocates all memory associated with a matrix previously partitioned by
 * divide_matrix_in_pim_chunks.
 */
static void free_matrix_chunks(uint16_t **chunks, uint32_t total_chunks) {
    if (!chunks) {
        return;
    }
    for (uint32_t i = 0; i < total_chunks; ++i) {
        kfree(chunks[i]);
    }
    kfree(chunks);
}

/**
 * Executes a GEMV Operation for a 64x128 Matrix by initializing the matrix and
 * the partial output vector and then calling gemv_execute to trigger the
 * PIM-VM. It then reads the partial result from the hardware and accumulates it
 * into the shared gemv_context struct for final processing.
 */
static int gemv_64x128_chunk(struct gemv_context *ctx, uint16_t *matrix_data,
                             uint16_t __iomem *input_vector_address,
                             uint16_t __iomem *dummy_region_address) {
    uint16_t *transformed_matrix_data = NULL;
    uint16_t __iomem *init_matrix_address = NULL;
    uint16_t __iomem *output_partial_sum_vector = NULL;
    int ret = 0;

    transformed_matrix_data = kmalloc(64 * 128 * sizeof(uint16_t), GFP_KERNEL);

    // Test if allocation was successful
    if (!ctx->result_integer_part || !ctx->result_fractional_part ||
        !transformed_matrix_data || !ctx->polished_result) {
        pr_err("PIM_GEMV: Failed to allocate memory on the heap.\n");
        ret = -ENOMEM;
        goto cleanup;
    }

    transform_matrix(matrix_data, transformed_matrix_data);
    init_matrix_address = init_matrix_flat(transformed_matrix_data, 64 * 128);
    if (!init_matrix_address) {
        ret = -ENOMEM;
        goto cleanup;
    }

    output_partial_sum_vector =
        init_vector_result(64 * ELEMENT_COUNT_SUBMATRIX);
    if (!output_partial_sum_vector) {
        ret = -ENOMEM;
        goto cleanup;
    }

    dsb(SY);
    set_bank_mode(PIM_ALL_BANK);

    for (int i = 0; i < ctx->repetitions;
         ++i) { // Repetitions var is there for simulating and evaluating
                // matrices with different columns sizes than 128
        gemv_execute(init_matrix_address, input_vector_address,
                     output_partial_sum_vector, dummy_region_address);
        dsb(sy);
    }

    set_bank_mode(SINGLE_BANK);

    accumulate_result_vector(ctx, output_partial_sum_vector);

    // Convert the result back to IEEE 754 16-Bit representation
    for (int i = 0; i < 64; i++) {
        int current_read_idx = ctx->index_result_polished;
        ctx->polished_result[current_read_idx] =
            kernel_parts_to_f16(ctx->result_integer_part[current_read_idx],
                                ctx->result_fractional_part[current_read_idx]);
        ctx->index_result_polished++;
    }

cleanup:
    kfree(transformed_matrix_data);
    return ret;
}

/**
 * Executes a general matrix-vector multiplication (GEMV) using predefined test
 * data in kernel space.
 */
void gemv_driver_code(void) {
    uint16_t *input_vector_data = NULL;
    uint16_t *matrix_data = NULL;
    uint16_t __iomem *input_vector_address = NULL;
    uint16_t __iomem *dummy_region_address = NULL;
    uint16_t **all_chunks_data = NULL;
    uint32_t num_chunks_data = 0;
    int ret = 0;

    // Create the context struct
    struct gemv_context ctx;
    memset(&ctx, 0, sizeof(struct gemv_context));

    input_vector_data = kmalloc_array(128, sizeof(uint16_t), GFP_KERNEL);
    if (!input_vector_data) {
        ret = -ENOMEM;
        goto cleanup;
    }

    matrix_data = kmalloc_array(1024, 128 * sizeof(uint16_t), GFP_KERNEL);
    if (!matrix_data) {
        ret = -ENOMEM;
        goto cleanup;
    }

    // ------------- Init the Input Vector -------------
    // in an interleaved manner for faster access for the PIM-Units
    for (int i = 0; i < 128; ++i) {
        input_vector_data[i] = F16_ONE;
    }

    // ------------- Init the Matrix -------------
    memset(matrix_data, 0, 1024 * 128 * sizeof(uint16_t));
    for (int r = 0; r < 1024; ++r) {
        for (int c = 0; c < 128; ++c) {
            if (r >= c) {
                size_t idx = (size_t)r * 128 + c;
                matrix_data[idx] = F16_ONE;
            }
        }
    }

    all_chunks_data =
        divide_matrix_in_pim_chunks(matrix_data, 1024, 128, &num_chunks_data);
    if (!all_chunks_data) {
        ret = -ENOMEM;
        goto cleanup;
    }
    ctx.repetitions = 512 / 128;

    ctx.result_integer_part = kmalloc(1024 * sizeof(int32_t), GFP_KERNEL);
    ctx.result_fractional_part = kmalloc(1024 * sizeof(int32_t), GFP_KERNEL);
    ctx.polished_result = kmalloc(1024 * sizeof(uint16_t), GFP_KERNEL);
    if (!ctx.result_integer_part || !ctx.result_fractional_part ||
        !ctx.polished_result) {
        ret = -ENOMEM;
        goto cleanup;
    }

    set_kernel(build_kernel_gemv);

    input_vector_address =
        init_vector_interleaved(input_vector_data, 64 / 8);
    if (!input_vector_address) {
        ret = -ENOMEM;
        goto cleanup;
    }

    dummy_region_address = init_dummy_memory_region();
    if (!dummy_region_address) {
        ret = -ENOMEM;
        goto cleanup;
    }
    for (int i = 0; i < num_chunks_data; ++i) {
        uint16_t *current_chunk_ptr = all_chunks_data[i];
        gemv_64x128_chunk(&ctx, current_chunk_ptr, input_vector_address,
                          dummy_region_address);
    }

    pr_err("--------- RESULT-VECTOR IS ---------:");
    pr_err("MATRIX ROWS: %d\n", 1024);
    for (int i = 0; i < 1024; i++) {
        pr_err("VAL %d: %d.%03u, ", i, ctx.result_integer_part[i],
               ctx.result_fractional_part[i]);
    }
    pr_err("Done.");

cleanup:
    // Free all allocated memory
    kfree(ctx.result_integer_part);
    kfree(ctx.result_fractional_part);
    kfree(ctx.polished_result);
    kfree(matrix_data);
    kfree(input_vector_data);
    free_matrix_chunks(all_chunks_data, num_chunks_data);

    if (ret != 0) {
        pr_err("gemv_driver_code failed with error %d\n", ret);
    }
}

/**
 * Executes a general matrix-vector multiplication (GEMV) using input data from
 * user space. Initializes PIM memory regions, performs the GEMV operation, and
 * copies the result back to user space.
 */
int gemv_from_userspace(__u64 result_addr, uint16_t *input_vector_data,
                        uint16_t *matrix_data, uint32_t len_input_vector,
                        uint32_t matrix_rows, uint32_t matrix_cols) {

    uint16_t __iomem *input_vector_address = NULL;
    uint16_t __iomem *dummy_region_address = NULL;
    uint16_t **all_chunks_data = NULL;
    uint32_t num_chunks_data = 0;
    int ret = 0;

    // Create the context struct
    struct gemv_context ctx;
    memset(&ctx, 0, sizeof(struct gemv_context));

    all_chunks_data = divide_matrix_in_pim_chunks(matrix_data, matrix_rows, 128,
                                                  &num_chunks_data);
    if (!all_chunks_data) {
        ret = -ENOMEM;
        goto cleanup;
    }

    ctx.repetitions = matrix_cols / 128;

    ctx.result_integer_part =
        kmalloc(matrix_rows * sizeof(int32_t), GFP_KERNEL);
    ctx.result_fractional_part =
        kmalloc(matrix_rows * sizeof(int32_t), GFP_KERNEL);
    ctx.polished_result = kmalloc(matrix_rows * sizeof(uint16_t), GFP_KERNEL);
    if (!ctx.result_integer_part || !ctx.result_fractional_part ||
        !ctx.polished_result) {
        ret = -ENOMEM;
        goto cleanup;
    }

    set_kernel(build_kernel_gemv);

    input_vector_address = init_vector_interleaved(input_vector_data, 64 / 8);
    if (!input_vector_address) {
        ret = -ENOMEM;
        goto cleanup;
    }

    dummy_region_address = init_dummy_memory_region();
    if (!dummy_region_address) {
        ret = -ENOMEM;
        goto cleanup;
    }

    for (int i = 0; i < num_chunks_data; ++i) {
        uint16_t *current_chunk_ptr = all_chunks_data[i];
        gemv_64x128_chunk(&ctx, current_chunk_ptr, input_vector_address,
                          dummy_region_address);
    }

    if (copy_to_user((void __user *)result_addr, ctx.polished_result,
                     matrix_rows * sizeof(uint16_t))) {
        pr_err("PIM: Failed to copy result vector to user\n");
        ret = -EFAULT;
    } else {
        pr_info("PIM: Successfully copied result to user space.\n");
    }

cleanup:
    // Free all allocated memory
    kfree(ctx.result_integer_part);
    kfree(ctx.result_fractional_part);
    kfree(ctx.polished_result);
    free_matrix_chunks(all_chunks_data, num_chunks_data);

    if (ret != 0) {
        pr_err("gemv_driver_code failed with error %d\n", ret);
    }
    return ret;
}
