#ifndef PIM_MATRICES_H
#define PIM_MATRICES_H

#include <linux/types.h>

extern const int MATRIX_ROWS;
extern const int MATRIX_COLS;

extern const int X16_ROWS;
extern const int X16_COLUMNS;
extern const int ELEMENT_COUNT_SUBMATRIX;

/**
 * Rearranges a flat, row-major matrix into a tiled layout for efficient PIM
 * access. The resulting 'row-major of blocks' format improves data locality for
 * the SIMD architecture.
 */
void transform_matrix(const uint16_t *original_matrix,
                      uint16_t *transformed_matrix);

/**
 * Allocates a PIM memory region and writes the transformed matrix data into it.
 * This function handles the transfer from a standard kernel buffer to the
 * memory-mapped PIM device space.
 */
void __iomem *init_matrix_flat(uint16_t *flat_data_arr,
                               size_t matrix_logical_size);

#endif