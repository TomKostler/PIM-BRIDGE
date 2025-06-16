#include "../include/pim_data_allocator.h"
#include "../include/pim_memory_region.h"

static size_t current_start_free_mem_offset = 0;

void __iomem *pim_data_region_alloc(size_t size, size_t alignment) {
    phys_addr_t phys_base_addr =
        PIM_DATA_MEMORY_REGION_BASE + current_start_free_mem_offset;

    phys_addr_t aligned_phys_addr =
        (phys_base_addr + alignment - 1) & ~(alignment - 1);

    unsigned long offset = aligned_phys_addr - phys_base_addr;

    if (offset + size > PIM_DATA_MEMORY_REGION_SIZE) {
        pr_err("PIM allocator out of memory\n");
        return NULL;
    }

    void __iomem *addr =
        (void __iomem *)((u8 __iomem *)pim_data_virt_addr +
                         current_start_free_mem_offset + offset);
    current_start_free_mem_offset += offset + size;
    return addr;
}

void __iomem *init_dummy_memory_region(void) {
    uint32_t __iomem *dummy_addr =
        pim_data_region_alloc(sizeof(uint16_t), PIM_VECTOR_ALIGNMENT);
    iowrite16(0x0000, dummy_addr);

    return dummy_addr;
}