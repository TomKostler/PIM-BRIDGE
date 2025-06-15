#ifndef PIM_MATRICES_H
#define PIM_MATRICES_H

int trigger_read_matrix(void __iomem *address);

int trigger_write_matrix(void __iomem *address);

/*
    Initializes the matrices needed in the PIM_DATA_REGION
*/
void transform_matrix(const uint16_t *original_matrix,
                      uint16_t *transformed_matrix, int rows, int cols);

void __iomem *init_matrix_flat(uint16_t *flat_data_arr,
                               size_t total_uint16_elements);

#endif