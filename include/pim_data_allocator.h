#include <linux/types.h>

#ifndef PIM_DATA_ALLOCATOR_H
#define PIM_DATA_ALLOCATOR_H


/**
 * Custom Allocator that allocates an aligned memory block from the static PIM data region using a bump-pointer scheme.
 * It tracks the next available address with a static offset, ensuring each sequential allocation is correctly aligned.
 */
void __iomem *pim_data_region_alloc(size_t size, size_t alignment);


/**
 * Allocates and zeroes out a small dummy memory region in PIM space.
 */
void __iomem *init_dummy_memory_region(void);



#endif