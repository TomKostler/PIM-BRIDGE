#ifndef PIM_VECTORS_H
#define PIM_VECTORS_H

int trigger_read_vector(void __iomem *address);

int trigger_write_vector(void __iomem *address);

/*
    Initializes the vectors needed in the PIM_DATA_REGION
*/
void __iomem *init_vector(uint16_t *arr, size_t length);

/*
    Initializes a vector filled with zeros so that the result can be written in
   it
*/
void __iomem *init_vector_result(size_t length);

void __iomem *init_vector_interleaved(uint16_t *logical_elements_data,
                                      size_t num_logical_elements,
                                      int num_physical_banks);

#endif