#include "../include/pim_vectors.h"
#include "../include/pim_configs.h"
#include "../include/pim_data_allocator.h"
#include "../include/pim_matrices.h"
#include "../include/pim_memory_region.h"

void __iomem *init_vector(uint16_t *arr, size_t length) {
    size_t total_size = length * sizeof(uint16_t);
    uint16_t __iomem *vector_start_addr =
        pim_data_region_alloc(total_size, PIM_VECTOR_ALIGNMENT);

    if (!vector_start_addr) {
        pr_err("PIM allocator failed in init_vector\n");
        return NULL;
    }

    uint16_t __iomem *current_addr = vector_start_addr;

    for (int i = 0; i < length; i++) {
        iowrite16(arr[i], current_addr);

        current_addr++;
        dsb(SY);
    }
    return vector_start_addr;
}

void __iomem *init_vector_result(size_t length) {
    uint16_t arr[length];
    memset(arr, 0, length * sizeof(uint16_t));

    return init_vector(arr, length);
}

void __iomem *init_vector_interleaved(uint16_t *logical_elements_data,
                                      size_t num_logical_elements) {

    const size_t WRITE_STRIDE =
        (size_t)NUM_BANKS * ELEMENT_COUNT_SUBMATRIX; // 512
    const size_t READ_STRIDE = WRITE_STRIDE * 2;     // 1024

    size_t total_elements_in_pim =
        (num_logical_elements - 1) * READ_STRIDE + WRITE_STRIDE;
    size_t total_size_bytes = total_elements_in_pim * sizeof(uint16_t);

    uint16_t __iomem *vector_start_addr =
        pim_data_region_alloc(total_size_bytes, PIM_VECTOR_ALIGNMENT);

    uint16_t *current_logical_element_src_ptr = logical_elements_data;

    for (size_t i = 0; i < num_logical_elements; ++i) {
        uint16_t __iomem *current_block_start_addr =
            vector_start_addr + i * READ_STRIDE;

        uint16_t __iomem *write_ptr = current_block_start_addr;

        for (int bank_idx = 0; bank_idx < NUM_BANKS; ++bank_idx) {
            for (int val_idx = 0; val_idx < ELEMENT_COUNT_SUBMATRIX;
                 ++val_idx) {
                iowrite16(current_logical_element_src_ptr[val_idx], write_ptr);
                write_ptr++;
            }
        }
        current_logical_element_src_ptr += ELEMENT_COUNT_SUBMATRIX;
    }

    dsb(SY);
    return vector_start_addr;
}
