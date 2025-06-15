#include "../../include/microkernels/kernel_datastructures.h"
#include "../../include/microkernels/kernels.h"
#include "../../include/pim_configs.h"
#include "../../include/pim_data_allocator.h"
#include "../../include/pim_init_state.h"
#include "../../include/pim_matrices.h"
#include "../../include/pim_memory_region.h"
#include "../../include/pim_vectors.h"

#define MATRIX_ROWS 128
#define MATRIX_COLS 128
#define REPETITIONS 8
#define F16_ONE 0x3C00

#define X16_ROWS (MATRIX_ROWS / 16)
#define X16_COLUMNS (MATRIX_COLS / 16)

#define FIXED_SHIFT 10
#define FIXED_MASK ((1 << FIXED_SHIFT) - 1)



static inline int32_t f16_to_fixed_point(uint16_t f16_val) {
    int32_t sign = (f16_val >> 15) & 0x01;
    int32_t exponent = (f16_val >> 10) & 0x1f;
    int32_t mantissa = f16_val & 0x3ff;

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

        int32_t denorm_val = mantissa;
        denorm_val = denorm_val >> (14 - FIXED_SHIFT);
        return sign ? -denorm_val : denorm_val;
    }

    int32_t value = (1 << FIXED_SHIFT) | mantissa;

    int32_t shift = exponent - 15;

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

static void
accumulate_result_vector(s32 result_front_part[MATRIX_ROWS],
                         s32 result_back_part[MATRIX_ROWS],
                         uint16_t __iomem *output_partial_sum_vector) {
    uint16_t partial_sums[128][16];
    for (int i = 0; i < 128; i++) {
        s64 accumulated_row = 0;
        for (int j = 0; j < 16; j++) {
            partial_sums[i][j] = ioread16(output_partial_sum_vector);

            s32 val = f16_to_fixed_point(partial_sums[i][j]);
            accumulated_row += val;

            output_partial_sum_vector++;
        }
        result_front_part[i] = (accumulated_row >> FIXED_SHIFT);
        result_back_part[i] =
            (((abs(accumulated_row) & FIXED_MASK) * 1000) >> FIXED_SHIFT);
    }
}



static void gemv_execute(uint16_t __iomem *matrix_base_addr,
                         uint16_t __iomem *input_vector_base_addr,
                         uint16_t __iomem *output_vector_base_addr,
                         uint16_t __iomem *dummy_region_address,
                         const int matrix_rows_param, // 128
                         const int X16_R,             // 8
                         const int X16_C,             // 8
                         const int num_pim_banks) {
    const size_t F16x16_SIZE_UINT16 = 16;

    const size_t INPUT_VECTOR_BLOCK_STRIDE_UINT16 =
        num_pim_banks * F16x16_SIZE_UINT16 * 2;
    const size_t STRIPE_STRIDE_UINT16 = (size_t)matrix_rows_param * 16;
    const size_t BLOCK_IN_STRIPE_STRIDE_UINT16 = 16 * 16;

    // === 1. Read input vector ===
    for (int i = 0; i < 8; ++i) {
        trigger_read_vector(input_vector_base_addr +
                            i * INPUT_VECTOR_BLOCK_STRIDE_UINT16);
    }

    // === 2. Execute Multiply and Accumulates ===
    for (int i = 0; i < X16_R; ++i) {
        for (int j = 0; j < X16_C; ++j) {

            size_t stripe_offset = (size_t)i * STRIPE_STRIDE_UINT16;
            size_t block_offset = (size_t)j * BLOCK_IN_STRIPE_STRIDE_UINT16;

            uint16_t __iomem *block_addr =
                matrix_base_addr + stripe_offset + block_offset;
            trigger_read_vector(block_addr);
        }
    }

    dsb(sy);

    // === Execute FILL's for writing the output vector ===
    const size_t OUTPUT_BLOCK_STRIDE_UINT16 = 16 * F16x16_SIZE_UINT16;
    for (int i = 0; i < 8; ++i) {
        trigger_write_vector(output_vector_base_addr +
                             i * OUTPUT_BLOCK_STRIDE_UINT16);
    }

    // === Trigger EXIT ===
    trigger_read_vector(dummy_region_address);
}



void gemv_driver_code(void) {

    // Set Microkernel to vadd
    int success_kernel = set_kernel(build_kernel_gemv);
    pr_err("Building kernel success: %d\n", success_kernel);

    // ------------- Init the Input Vector -------------
    // in an interleaved manner for faster access for the PIM-Units
    uint16_t input_vector_data[MATRIX_ROWS];
    for (int i = 0; i < MATRIX_ROWS; ++i) {
        input_vector_data[i] = F16_ONE;
    }
    uint16_t __iomem *input_vector_address = init_vector_interleaved(
        input_vector_data, MATRIX_ROWS / X16_ROWS, NUM_BANKS);
    // -------------------------------------------


    // ------------- Init the Matrix -------------
    static uint16_t matrix_data[MATRIX_ROWS * MATRIX_COLS];
    static uint16_t transformed_matrix_data[MATRIX_ROWS * MATRIX_COLS];

    pr_err(
        "PIM Debug: Filling the lower triangle of the matrix with ones...\n");
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

    transform_matrix(matrix_data, transformed_matrix_data, MATRIX_ROWS,
                     MATRIX_COLS);
    uint16_t __iomem *init_matrix_address =
        init_matrix_flat(transformed_matrix_data, MATRIX_ROWS * MATRIX_COLS);
    // -------------------------------------------


    // Init result vector
    uint16_t __iomem *output_partial_sum_vector =
        init_vector_result(MATRIX_ROWS * X16_ROWS);

    // Init dummy memory region for syncing operations that don't need data from
    // memory
    uint16_t __iomem *dummy_region_address = init_dummy_memory_region();

    // Guarantee that vectors and PIM_DATA_REGION is correctly initialized
    dsb(SY);

    // Switch to PIM_ALL_BANK to notifiy DRAM-Controller that reads/writes to
    // PIM Regions should be forwarded to PIM-VM
    set_bank_mode(PIM_ALL_BANK);

    for (int i = 0; i < REPETITIONS; ++i) {
        gemv_execute(init_matrix_address, input_vector_address,
                     output_partial_sum_vector, dummy_region_address,
                     MATRIX_ROWS, X16_ROWS, X16_COLUMNS, NUM_BANKS);
        dsb(sy);
    }

    set_bank_mode(SINGLE_BANK);

    // Accumulate the result vector out of the partial sums
    s32 result_front_part[MATRIX_ROWS];
    s32 result_back_part[MATRIX_ROWS];
    accumulate_result_vector(result_front_part, result_back_part,
                             output_partial_sum_vector);

    pr_err("--------- RESULT-VECTOR IS ---------:");
    pr_err("MATRIX ROWS: %d\n", MATRIX_ROWS);
    for (int i = 0; i < MATRIX_ROWS; i++) {
        pr_err("VAL %d: %d.%03u, ", i, result_front_part[i], result_back_part[i]);
    }
    pr_err("Done.");
}
