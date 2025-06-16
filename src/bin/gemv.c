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
accumulate_result_vector(int32_t result_integer_part[MATRIX_ROWS],
                         int32_t result_fractional_part[MATRIX_ROWS],
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
        NUM_BANKS * ELEMENT_COUNT_SUBMATRIX * 2;
    const size_t STRIPE_STRIDE = (size_t)MATRIX_ROWS * ELEMENT_COUNT_SUBMATRIX;
    const size_t BLOCK_IN_STRIPE_STRIDE =
        ELEMENT_COUNT_SUBMATRIX * ELEMENT_COUNT_SUBMATRIX;

    // === 1. Read input vector ===
    for (int i = 0; i < X16_ROWS; ++i) {
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
    for (int i = 0; i < X16_ROWS; ++i) {
        trigger_write(output_vector_base_addr + i * BLOCK_IN_STRIPE_STRIDE);
    }

    // === Trigger EXIT ===
    trigger_read(dummy_region_address);
}

void gemv_driver_code(void) {

    uint16_t input_vector_data[MATRIX_ROWS];
    static uint16_t matrix_data[MATRIX_ROWS * MATRIX_COLS];
    static uint16_t transformed_matrix_data[MATRIX_ROWS * MATRIX_COLS];
    int32_t result_integer_part[MATRIX_ROWS];
    int32_t result_fractional_part[MATRIX_ROWS];

    uint16_t __iomem *input_vector_address;
    uint16_t __iomem *init_matrix_address;
    uint16_t __iomem *dummy_region_address;
    uint16_t __iomem *output_partial_sum_vector;

    // Set Microkernel to vadd
    int success_kernel = set_kernel(build_kernel_gemv);
    pr_err("Building kernel success: %d\n", success_kernel);

    // ------------- Init the Input Vector -------------
    // in an interleaved manner for faster access for the PIM-Units
    for (int i = 0; i < MATRIX_ROWS; ++i) {
        input_vector_data[i] = F16_ONE;
    }
    input_vector_address =
        init_vector_interleaved(input_vector_data, MATRIX_ROWS / X16_ROWS);
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

    // For testing purposes
    // for (size_t i = 0; i < MATRIX_ROWS * MATRIX_COLS; ++i) {
    //     matrix_data[i] = 0x3c00;
    // }

    transform_matrix(matrix_data, transformed_matrix_data);
    init_matrix_address =
        init_matrix_flat(transformed_matrix_data, MATRIX_ROWS * MATRIX_COLS);
    // -------------------------------------------

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
