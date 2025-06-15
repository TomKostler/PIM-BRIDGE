#include <linux/types.h>

#ifndef PIM_DATA_ALLOCATOR_H
#define PIM_DATA_ALLOCATOR_H

/*
	Custom Allocator to make sure the PIM_DATA is correctly aligned for PIM_VM
*/ 

void __iomem *init_dummy_memory_region(void);

void __iomem *pim_data_region_alloc(size_t size, size_t alignment);

#endif