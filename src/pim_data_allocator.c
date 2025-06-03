#include "../include/pim_data_allocator.h"
#include "../include/pim_memory_region.h"

static size_t pim_data_offset = 0;

void __iomem *pim_data_region_alloc(size_t size, uint32_t alignment) {
    size_t aligned_offset;
    void __iomem *addr;

    // Align current offset
    aligned_offset = (pim_data_offset + (alignment - 1)) & ~(alignment - 1);

    if (aligned_offset + size > PIM_DATA_MEMORY_REGION_SIZE) {
        pr_err("PIM allocator out of memory\n");
        return NULL;
    }

    addr = pim_data_virt_addr + aligned_offset;
    pim_data_offset = aligned_offset + size;

    return addr;
}

void __iomem *init_dummy_memory_region(void) {
    uint32_t __iomem *dummy_addr =
        pim_data_region_alloc(sizeof(uint16_t), PIM_VECTOR_ALIGNMENT);
    iowrite16(0x0000, dummy_addr);

    return dummy_addr;
}