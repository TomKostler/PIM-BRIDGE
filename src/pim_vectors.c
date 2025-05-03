#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>

#include "../include/pim_vectors.h"
#include "../include/pim_memory_region.h"
#include "../include/pim_data_allocator.h"


uint32_t __iomem* init_vector(uint16_t *arr, size_t length) {

    // Allocate enough physical memory based off MMIO-Mapping for the vector
    uint32_t __iomem *vector_addr = pim_data_region_alloc(length * sizeof(uint16_t), PIM_VECTOR_ALIGNMENT);
    pr_info("Vector start is 1024-Bytes aligned? %d\n", IS_ALIGNED((uintptr_t)vector_addr, 1024));


    // Place the Vector in the memory
    for (int i = 0; i < length; i++) {
        iowrite16(arr[i], vector_addr);
        vector_addr += 2;
        dsb(SY);
    }
    

    // For testing purposes
    pr_info("--------- Vector is ---------:");
    vector_addr -= 2*length;
    for (int i = 0; i < length; i++) {
        uint16_t val = ioread16(vector_addr);
        pr_info("VAL: (hex): 0x%x\n", val);
        vector_addr += 2;
    }

    wmb();
    return vector_addr;
}




uint32_t __iomem* init_vector_result(size_t length) {
    uint16_t arr[length];
    memset(arr, 0, length * sizeof(uint16_t));

    return init_vector(arr, length);
}


void half_to_string(uint16_t half, char *str) {
    uint16_t sign   = (half >> 15) & 0x1;
    uint16_t exp    = (half >> 10) & 0x1F;
    uint16_t frac   = half & 0x3FF;
    
    if(exp == 0x1F) {
        if(frac) {
            strcpy(str, "NaN");
        } else {
            strcpy(str, sign ? "-inf" : "inf");
        }
        return;
    }
    
    int integer_part = 0;
    int frac_part = 0;
    
    if(exp != 0) {
        int e = exp - 15;
        
        uint32_t significand = (1 << 10) | frac;
        
        
        integer_part = significand >> 10;
        frac_part = significand & ((1 << 10) - 1);
    }
    
    if(sign)
        sprintf(str, "-%d.%d", integer_part, frac_part);
    else
        sprintf(str, "%d.%d", integer_part, frac_part);
}