#ifndef PIM_DATA_MEMORY_REGION_H
#define PIM_DATA_MEMORY_REGION_H


#include <linux/io.h> //ioremap

#define PIM_DATA_MEMORY_REGION_BASE 0xc0004000
#define PIM_DATA_MEMORY_REGION_SIZE 0x3FFFC000
#define PIM_VECTOR_ALIGNMENT 1024 // Equals Burst Rate of 1024 Bytes of HBM Memory Controller


extern volatile u32 __iomem *pim_data_virt_addr;

#endif