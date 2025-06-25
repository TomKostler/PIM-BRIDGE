#ifndef PIM_MEMORY_REGION_H
#define PIM_MEMORY_REGION_H

#include <linux/io.h> //ioremap

#define PIM_CONFIG_MEMORY_REGION_BASE 0xC0000000UL
#define PIM_CONFIG_MEMORY_REGION_SIZE 0x4000UL

#define PIM_DATA_MEMORY_REGION_BASE 0xC0004000UL
#define PIM_DATA_MEMORY_REGION_SIZE 0x3FFFC000UL

// per pCh 256 Bytes can be calculated, for both of them that would be 512 Bytes
// => 512 Bytes Alignment would be enough => 1024 for safety
#define PIM_VECTOR_ALIGNMENT 1024

#define PIM_MATRIX_ALIGNMENT 65536

extern volatile u32 __iomem *pim_data_virt_addr;
extern volatile u8 __iomem *pim_config_virt_addr;

#endif