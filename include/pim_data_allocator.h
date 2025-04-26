#ifndef PIM_DATA_ALLOCATOR
#define PIM_DATA_ALLOCATOR


void *pim_data_region_alloc(size_t size, uint32_t alignment);

int init_dummy_memory_region(void);



#endif