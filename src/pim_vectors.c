#include "../include/pim_vectors.h"
#include "../include/pim_configs.h"
#include "../include/pim_data_allocator.h"
#include "../include/pim_matrices.h"
#include "../include/pim_memory_region.h"

// Default Value for the drivers
int ROWS = 256;

void __iomem *init_vector(uint16_t *arr, size_t length) {
    uint16_t __iomem *current_addr;
    uint16_t __iomem *vector_start_addr;
    size_t total_size;

    total_size = length * sizeof(uint16_t);
    vector_start_addr = pim_data_region_alloc(total_size, PIM_VECTOR_ALIGNMENT);

    if (!vector_start_addr) {
        pr_err("PIM allocator failed in init_vector\n");
        return NULL;
    }

    current_addr = vector_start_addr;

    for (int i = 0; i < length; i++) {
        iowrite16(arr[i], current_addr);

        current_addr++;
        dsb(SY);
    }
    return vector_start_addr;
}

void __iomem *init_vector_result(size_t length) {
    size_t total_size = length * sizeof(uint16_t);
    uint16_t __iomem *vector_start_addr =
        pim_data_region_alloc(total_size, PIM_VECTOR_ALIGNMENT);

    if (!vector_start_addr) {
        pr_err("PIM allocator failed in init_vector_result\n");
        return NULL;
    }

    for (int i = 0; i < length; i++) {
        iowrite16(0, vector_start_addr + i);
        dsb(SY);
    }

    return vector_start_addr;
}

void __iomem *init_vector_interleaved(uint16_t *logical_elements_data,
                                      size_t num_logical_elements) {
    uint16_t __iomem *vector_start_addr;
    uint16_t __iomem *write_ptr;

    const size_t ELEMENTS_PER_BLOCK =
        (size_t)NUM_BANKS * ELEMENT_COUNT_SUBMATRIX; // 16 * 16 = 256

    size_t total_elements_in_pim = num_logical_elements * ELEMENTS_PER_BLOCK;
    size_t total_size_bytes = total_elements_in_pim * sizeof(uint16_t);

    vector_start_addr =
        pim_data_region_alloc(total_size_bytes, PIM_VECTOR_ALIGNMENT);
    if (!vector_start_addr) {
        pr_err("PIM: pim_data_region_alloc failed for interleaved vector\n");
        return NULL;
    }

    write_ptr = vector_start_addr;

    for (size_t i = 0; i < num_logical_elements; ++i) {
        uint16_t *current_logical_element_src_ptr =
            logical_elements_data + i * ELEMENT_COUNT_SUBMATRIX;

        for (int bank_idx = 0; bank_idx < NUM_BANKS; ++bank_idx) {
            for (int val_idx = 0; val_idx < ELEMENT_COUNT_SUBMATRIX;
                 ++val_idx) {
                iowrite16(current_logical_element_src_ptr[val_idx], write_ptr);
                write_ptr++;
            }
        }
    }

    dsb(SY);
    return vector_start_addr;
}