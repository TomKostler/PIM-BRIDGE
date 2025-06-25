#include "../include/pim_matrices.h"
#include "../include/pim_data_allocator.h"
#include "../include/pim_memory_region.h"

// Default Sizes (Can be changed in the drivers)
int MATRIX_ROWS = 128;
int MATRIX_COLS = 128;

int X16_ROWS;
int X16_COLUMNS;
int ELEMENT_COUNT_SUBMATRIX;

void transform_matrix(const uint16_t *original_matrix,
                      uint16_t *transformed_matrix) {
    const int NUM_ELEMENTS_IN_BLOCK =
        ELEMENT_COUNT_SUBMATRIX * X16_COLUMNS * ELEMENT_COUNT_SUBMATRIX;
    const int NUM_ELEMENTS_IN_COL =
        ELEMENT_COUNT_SUBMATRIX * ELEMENT_COUNT_SUBMATRIX;

    for (int i = 0; i < X16_ROWS; ++i) {
        for (int c = 0; c < X16_COLUMNS; ++c) {
            for (int r = 0; r < ELEMENT_COUNT_SUBMATRIX; ++r) {
                for (int k = 0; k < ELEMENT_COUNT_SUBMATRIX; ++k) {

                    int src_row = i * ELEMENT_COUNT_SUBMATRIX + r;
                    int src_col = c * ELEMENT_COUNT_SUBMATRIX + k;
                    int src_index = src_row * MATRIX_COLS + src_col;

                    int dest_index = (i * NUM_ELEMENTS_IN_BLOCK) +
                                     (c * NUM_ELEMENTS_IN_COL) +
                                     (r * ELEMENT_COUNT_SUBMATRIX) + k;

                    transformed_matrix[dest_index] = original_matrix[src_index];
                }
            }
        }
    }
}

void __iomem *init_matrix_flat(uint16_t *flat_data_arr,
                               size_t matrix_logical_size) {
    uint16_t __iomem *current_addr;
    size_t total_size_bytes = matrix_logical_size * sizeof(uint16_t);
    uint16_t __iomem *matrix_start_addr =
        pim_data_region_alloc(total_size_bytes, PIM_MATRIX_ALIGNMENT);

    if (!matrix_start_addr) {
        pr_err("PIM allocator failed in init_matrix_flat\n");
        return NULL;
    }

    current_addr = matrix_start_addr;

    // Write elements sequentially
    for (size_t i = 0; i < matrix_logical_size; i++) {
        iowrite16(flat_data_arr[i], current_addr);
        current_addr++;
    }
    dsb(SY);

    return matrix_start_addr;
}
