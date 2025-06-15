#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#include "../include/pim_configs.h"
#include "../include/pim_data_allocator.h"
#include "../include/pim_memory_region.h"
#include "../include/pim_vectors.h"

int trigger_write_vector(void __iomem *address) {
    iowrite8(0, address);
    dsb(SY);
    return 0;
}

int trigger_read_vector(void __iomem *address) {
    volatile uint8_t val = ioread8(address);
    (void)val;
    dsb(SY);
    return 0;
}

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

#define F16x16_NUM_UINT16 16
void __iomem *init_vector_interleaved(uint16_t *logical_elements_data,
                                      size_t num_logical_elements,
                                      int num_physical_banks) {

    const size_t WRITE_STRIDE_UINT16 =
        (size_t)num_physical_banks * F16x16_NUM_UINT16;        // 512
    const size_t READ_STRIDE_UINT16 = WRITE_STRIDE_UINT16 * 2; // 1024

    size_t total_uint16_elements_in_pim =
        (num_logical_elements - 1) * READ_STRIDE_UINT16 + WRITE_STRIDE_UINT16;
    size_t total_size_bytes = total_uint16_elements_in_pim * sizeof(uint16_t);

    uint16_t __iomem *vector_start_addr =
        pim_data_region_alloc(total_size_bytes, PIM_VECTOR_ALIGNMENT);

    uint16_t *current_logical_element_src_ptr = logical_elements_data;

    for (size_t i = 0; i < num_logical_elements; ++i) {
        uint16_t __iomem *current_block_start_addr =
            vector_start_addr + i * READ_STRIDE_UINT16;

        uint16_t __iomem *write_ptr = current_block_start_addr;

        for (int bank_idx = 0; bank_idx < num_physical_banks; ++bank_idx) {
            for (int val_idx = 0; val_idx < F16x16_NUM_UINT16; ++val_idx) {
                iowrite16(current_logical_element_src_ptr[val_idx], write_ptr);
                write_ptr++;
            }
        }
        current_logical_element_src_ptr += F16x16_NUM_UINT16;
    }

    dsb(SY);
    return vector_start_addr;
}
