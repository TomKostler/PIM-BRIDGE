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
        return 0x7C00;
    }

    if (biased_exponent > 0) {
        shift = msb_pos - 10;
        mantissa = (shift >= 0) ? (fixed_val >> shift) & 0x3FF
                                : (fixed_val << -shift) & 0x3FF;
        return (biased_exponent << 10) | mantissa;

    } else {
        shift = 1 - biased_exponent;

        if (shift > msb_pos + 1) {
            return 0;
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

    binary_fractional_part = ((uint64_t)fractional_part * 1024) / 1000;

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

    if (exponent == 0x1f) {
        if (mantissa == 0) {
            return sign ? INT_MIN : INT_MAX;
        }
        return 0;
    }

    if (exponent == 0) {
        if (mantissa == 0) {
            return 0;
        }

        denorm_val = mantissa;
        denorm_val = denorm_val >> (14 - FIX_POINT_SHIFT);
        return sign ? -denorm_val : denorm_val;
    }

    value = (1 << FIX_POINT_SHIFT) | mantissa;

    shift = exponent - 15;

    if (shift > 0) {
        if (shift > 20) {
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
accumulate_result_vector(int32_t *result_integer_part,
                         int32_t *result_fractional_part,
                         uint16_t __iomem *output_partial_sum_vector) {
    for (int i = 0; i < MATRIX_ROWS; i++) {
        int64_t accumulated_row = 0;
        for (int j = 0; j < ELEMENT_COUNT_SUBMATRIX; j++) {

            int32_t val =
                f16_to_fixed_point(ioread16(output_partial_sum_vector));
            accumulated_row += val;

            output_partial_sum_vector++;
        }
        result_integer_part[i] = (accumulated_row >> FIX_POINT_SHIFT);
        result_fractional_part[i] =
            (((abs(accumulated_row) & FIX_POINT_MASK) * 1000) >>
             FIX_POINT_SHIFT);
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
    const size_t STRIPE_STRIDE = (size_t)MATRIX_ROWS * ELEMENT_COUNT_SUBMATRIX;
    const size_t BLOCK_IN_STRIPE_STRIDE =
        ELEMENT_COUNT_SUBMATRIX * ELEMENT_COUNT_SUBMATRIX;

    // === 1. Read input vector ===
    for (int i = 0; i < X16_COLUMNS; ++i) {
        trigger_read(input_vector_base_addr + i * INPUT_VECTOR_BLOCK_STRIDE);
    }

    // === 2. Execute Multiply and Accumulates ===
    for (int i = 0; i < X16_ROWS; ++i) {
        for (int j = 0; j < X16_COLUMNS; ++j) {

            size_t stripe_offset = (size_t)i * STRIPE_STRIDE;
            size_t block_offset = (size_t)j * BLOCK_IN_STRIPE_STRIDE;

            uint16_t __iomem *block_addr =
                matrix_base_addr + stripe_offset + block_offset;
            trigger_read(block_addr);
        }
    }

    dsb(sy);

    // === Execute FILL's for writing the output vector ===
    for (int i = 0; i < X16_COLUMNS; ++i) {
        trigger_write(output_vector_base_addr + i * BLOCK_IN_STRIPE_STRIDE);
    }

    // === Trigger EXIT ===
    trigger_read(dummy_region_address);
}

static void print_pim_memory_layout(const char *title,
                                    const volatile void __iomem *base_addr,
                                    size_t num_elements) {
    const volatile uint16_t __iomem *ptr = base_addr;
    uint16_t val;
    size_t i;

    pr_err("--- %s ---\n", title);

    for (i = 0; i < num_elements; ++i) {
        val = ioread16(ptr + i);
        pr_err("%04zu: (0x%04X)\n", i, val);

        if ((i + 1) % 16 == 0) {
            pr_err("\n");
        }
    }
    pr_err("--- End of %s ---\n", title);
}

/**
 * Executes a general matrix-vector multiplication (GEMV) using predefined test
 * data in kernel space.
 */
void gemv_driver_code(void) {

    uint16_t *input_vector_data = NULL;
    uint16_t *matrix_data = NULL;
    uint16_t *transformed_matrix_data = NULL;
    int32_t *result_integer_part = NULL;
    int32_t *result_fractional_part = NULL;

    uint16_t __iomem *input_vector_address = NULL;
    uint16_t __iomem *init_matrix_address = NULL;
    uint16_t __iomem *dummy_region_address = NULL;
    uint16_t __iomem *output_partial_sum_vector = NULL;

    int ret = 0;

    // Overwriting the default value
    MATRIX_ROWS = 128;
    MATRIX_COLS = 128;

    X16_ROWS = (MATRIX_ROWS / 16);
    X16_COLUMNS = (MATRIX_COLS / 16);
    ELEMENT_COUNT_SUBMATRIX = 16;

    input_vector_data =
        kmalloc_array(MATRIX_ROWS, sizeof(uint16_t), GFP_KERNEL);
    if (!input_vector_data) {
        ret = -ENOMEM;
    }

    matrix_data =
        kmalloc_array(MATRIX_ROWS, MATRIX_COLS * sizeof(uint16_t), GFP_KERNEL);
    if (!matrix_data) {
        ret = -ENOMEM;
    }

    transformed_matrix_data =
        kmalloc_array(MATRIX_ROWS, MATRIX_COLS * sizeof(uint16_t), GFP_KERNEL);
    if (!transformed_matrix_data) {
        ret = -ENOMEM;
    }

    result_integer_part =
        kmalloc_array(MATRIX_ROWS, sizeof(int32_t), GFP_KERNEL);
    if (!result_integer_part) {
        ret = -ENOMEM;
    }

    result_fractional_part =
        kmalloc_array(MATRIX_ROWS, sizeof(int32_t), GFP_KERNEL);
    if (!result_fractional_part) {
        ret = -ENOMEM;
    }

    // Set Microkernel to vadd
    set_kernel(build_kernel_gemv);

    // ------------- Init the Input Vector -------------
    // in an interleaved manner for faster access for the PIM-Units
    for (int i = 0; i < MATRIX_ROWS; ++i) {
        input_vector_data[i] = F16_ONE;
    }
    input_vector_address =
        init_vector_interleaved(input_vector_data, MATRIX_ROWS / X16_ROWS);

    pr_err("--- Validating PIM Vector Layout in C-Kernel ---\n");
    print_pim_memory_layout("Interleaved Vector Memory Layout (C Kernel)",
                              input_vector_address,
                              (size_t)X16_COLUMNS * NUM_BANKS *
                              ELEMENT_COUNT_SUBMATRIX);

    // -------------------------------------------

    // ------------- Init the Matrix -------------
    memset(matrix_data, 0, MATRIX_ROWS * MATRIX_COLS * sizeof(uint16_t));

    for (int r = 0; r < MATRIX_ROWS; ++r) {
        for (int c = 0; c < MATRIX_COLS; ++c) {
            if (r >= c) {
                size_t idx = (size_t)r * MATRIX_COLS + c;
                matrix_data[idx] = F16_ONE;
            }
        }
    }

    transform_matrix(matrix_data, transformed_matrix_data);
    init_matrix_address =
        init_matrix_flat(transformed_matrix_data, MATRIX_ROWS * MATRIX_COLS);

    // Init result vector
    output_partial_sum_vector = init_vector_result(MATRIX_ROWS * X16_ROWS);

    // Init dummy memory region for syncing operations that don't need data from
    // memory
    dummy_region_address = init_dummy_memory_region();

    // Guarantee that the vector, matrix and PIM_DATA_REGION is correctly
    // initialized
    dsb(SY);

    // Switch to PIM_ALL_BANK to notifiy DRAM-Controller that reads/writes to
    // PIM Regions should be forwarded to PIM-VM
    set_bank_mode(PIM_ALL_BANK);

    for (int i = 0; i < REPETITIONS; ++i) {
        gemv_execute(init_matrix_address, input_vector_address,
                     output_partial_sum_vector, dummy_region_address);
        dsb(sy);
    }

    // Revert to Single Bank Mode to read results directly from memory without
    // invoking the PIM Microkernel.
    set_bank_mode(SINGLE_BANK);

    // Accumulate the result vector out of the partial sums
    accumulate_result_vector(result_integer_part, result_fractional_part,
                             output_partial_sum_vector);

    pr_err("--------- RESULT-VECTOR IS ---------:");
    pr_err("MATRIX ROWS: %d\n", MATRIX_ROWS);
    for (int i = 0; i < MATRIX_ROWS; i++) {
        pr_err("VAL %d: %d.%03u, ", i, result_integer_part[i],
               result_fractional_part[i]);
    }
    pr_err("Done.");
}

/**
 * Executes a general matrix-vector multiplication (GEMV) using input data from
 * user space. Initializes PIM memory regions, performs the GEMV operation, and
 * copies the result back to user space.
 */
int gemv_from_userspace(__u64 result_addr, uint16_t *kernel_input_vector,
                        uint16_t *kernel_matrix, uint32_t len_input_vector,
                        uint32_t matrix_dim1, uint32_t matrix_dim2) {

    int32_t *result_integer_part = NULL;
    int32_t *result_fractional_part = NULL;
    uint16_t *transformed_matrix_data = NULL;
    uint16_t *polished_result = NULL;

    uint16_t __iomem *input_vector_address = NULL;
    uint16_t __iomem *init_matrix_address = NULL;
    uint16_t __iomem *dummy_region_address = NULL;
    uint16_t __iomem *output_partial_sum_vector = NULL;

    int ret = 0;

    // Overwriting the default value
    MATRIX_ROWS = 128;
    MATRIX_COLS = 128;

    X16_ROWS = (MATRIX_ROWS / 16);
    X16_COLUMNS = (MATRIX_COLS / 16);
    ELEMENT_COUNT_SUBMATRIX = 16;

    if (matrix_dim1 == 0 || matrix_dim2 == 0 || (matrix_dim1 % 16) != 0 ||
        (matrix_dim2 % 16) != 0) {
        pr_err("PIM_GEMV: Invalid matrix dimensions (%u x %u). Must be "
               "non-zero and a multiple of 16.\n",
               matrix_dim1, matrix_dim2);
        return -EINVAL;
    }

    result_integer_part = kmalloc(MATRIX_ROWS * sizeof(int32_t), GFP_KERNEL);
    result_fractional_part = kmalloc(MATRIX_ROWS * sizeof(int32_t), GFP_KERNEL);
    transformed_matrix_data =
        kmalloc(MATRIX_ROWS * MATRIX_COLS * sizeof(uint16_t), GFP_KERNEL);
    polished_result = kmalloc(MATRIX_ROWS * sizeof(uint16_t), GFP_KERNEL);

    // Test if allocation was successfull
    if (!result_integer_part || !result_fractional_part ||
        !transformed_matrix_data || !polished_result) {
        pr_err("PIM_GEMV: Failed to allocate memory on the heap.\n");
        ret = -ENOMEM;
        goto cleanup;
    }

    set_kernel(build_kernel_gemv);

    input_vector_address =
        init_vector_interleaved(kernel_input_vector, MATRIX_ROWS / X16_ROWS);
    if (!input_vector_address) {
        ret = -ENOMEM;
        goto cleanup;
    }

    transform_matrix(kernel_matrix, transformed_matrix_data);
    init_matrix_address =
        init_matrix_flat(transformed_matrix_data, MATRIX_ROWS * MATRIX_COLS);
    if (!init_matrix_address) {
        ret = -ENOMEM;
        goto cleanup;
    }

    output_partial_sum_vector = init_vector_result(MATRIX_ROWS * X16_ROWS);
    if (!output_partial_sum_vector) {
        ret = -ENOMEM;
        goto cleanup;
    }

    dummy_region_address = init_dummy_memory_region();
    if (!dummy_region_address) {
        ret = -ENOMEM;
        goto cleanup;
    }

    dsb(SY);
    set_bank_mode(PIM_ALL_BANK);

    for (int i = 0; i < REPETITIONS; ++i) {
        gemv_execute(init_matrix_address, input_vector_address,
                     output_partial_sum_vector, dummy_region_address);
        dsb(sy);
    }

    set_bank_mode(SINGLE_BANK);

    accumulate_result_vector(result_integer_part, result_fractional_part,
                             output_partial_sum_vector);

    // Convert the result back to IEEE 754 16-Bit representation
    for (int i = 0; i < MATRIX_ROWS; i++) {
        polished_result[i] = kernel_parts_to_f16(result_integer_part[i],
                                                 result_fractional_part[i]);
    }

    if (copy_to_user((void __user *)result_addr, polished_result,
                     MATRIX_ROWS * sizeof(uint16_t))) {
        pr_err("PIM: Failed to copy result vector to user\n");
        ret = -EFAULT;
    } else {
        pr_info("PIM: Successfully copied result to user space.\n");
    }

cleanup:
    kfree(transformed_matrix_data);
    kfree(result_fractional_part);
    kfree(result_integer_part);
    kfree(polished_result);

    return ret;
}
