#ifndef PIM_VECTORS_H
#define PIM_VECTORS_H

#include <linux/types.h>

/**
 * Initializes the vectors needed in the PIM_DATA_REGION by copying a simple
 * vector into it element by element.
 */
void __iomem *init_vector(uint16_t *arr, size_t length);

/**
 * Initializes a vector filled with zeros so that the result can be written in
 * it
 */
void __iomem *init_vector_result(size_t length);

/**
 * Initializes the PIM memory for the input vector with a non-contiguous layout
 * required by the execution kernel. It lays out each logical vector block at a
 * wide stride to create gaps in memory, repeating the data within each block
 * for all PIM banks.
 */
void __iomem *init_vector_interleaved(uint16_t *logical_elements_data,
                                      size_t num_logical_elements);

#endif