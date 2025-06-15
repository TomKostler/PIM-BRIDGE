#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#include "../include/pim_data_allocator.h"
#include "../include/pim_matrices.h"
#include "../include/pim_memory_region.h"

/*
 * Transform the Matrix into block-based column major
 */
void transform_matrix(const uint16_t *original_matrix,
                      uint16_t *transformed_matrix, int rows, int cols) {
    const int X16R = rows / 16;
    const int X16C = cols / 16;
    const int NUM_ELEMENTS_IN_BLOCK =
        16 * X16C * 16; // 16 rows * 8 cols * 16 elements = 2048
    const int NUM_ELEMENTS_IN_COL =
        16 * 16; // 16 F16x16 vectors per column = 256

    for (int i = 0; i < X16R; ++i) {
        for (int c = 0; c < X16C; ++c) {
            for (int r = 0; r < 16; ++r) {
                for (int k = 0; k < 16; ++k) {

                    int src_row = i * 16 + r;
                    int src_col = c * 16 + k;
                    int src_index = src_row * cols + src_col;

                    int dest_index = (i * NUM_ELEMENTS_IN_BLOCK) +
                                     (c * NUM_ELEMENTS_IN_COL) + (r * 16) + k;

                    transformed_matrix[dest_index] = original_matrix[src_index];
                }
            }
        }
    }
}


void __iomem *init_matrix_flat(uint16_t *flat_data_arr,
                               size_t total_uint16_elements) {
    size_t total_size_bytes = total_uint16_elements * sizeof(uint16_t);
    uint16_t __iomem *matrix_start_addr =
        pim_data_region_alloc(total_size_bytes, PIM_MATRIX_ALIGNMENT);

    if (!matrix_start_addr) {
        pr_err("PIM allocator failed in init_matrix_flat\n");
        return NULL;
    }

    uint16_t __iomem *current_addr = matrix_start_addr;

    pr_info("Initializing matrix with %zu uint16_t elements.\n",
            total_uint16_elements);

    // Write elements sequentially
    for (size_t i = 0; i < total_uint16_elements; i++) {
        iowrite16(flat_data_arr[i], current_addr);
        current_addr++;
    }
    dsb(SY);

    return matrix_start_addr;
}
