#ifndef PIM_MEMORY_REGION_H
#define PIM_MEMORY_REGION_H


#include <linux/io.h> //ioremap


#define PIM_CONFIG_MEMORY_REGION_BASE 0xC0000000
#define PIM_CONFIG_MEMORY_REGION_SIZE 0x4000

#define PIM_DATA_MEMORY_REGION_BASE 0xc0004000
#define PIM_DATA_MEMORY_REGION_SIZE 0x3FFFC000


// per pCh 256 Bytes can be calculated, for both of them that would be 512 Bytes
// => 512 Bytes Alignment would be enough => 1024 for safety
#define PIM_VECTOR_ALIGNMENT 1024


extern volatile u32 __iomem *pim_data_virt_addr;
extern volatile u32 __iomem *pim_config_virt_addr;


#endif