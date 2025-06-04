#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#include "../include/pim_data_allocator.h"
#include "../include/pim_memory_region.h"
#include "../include/pim_vectors.h"




int trigger_write_vector(void __iomem *address) {
    iowrite16(0, address);
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

void half_to_string(uint16_t half, char *str) {
    uint16_t sign = (half >> 15) & 0x1;
    uint16_t exp = (half >> 10) & 0x1F;
    uint16_t frac = half & 0x3FF;

    if (exp == 0x1F) {
        if (frac) {
            strcpy(str, "NaN");
        } else {
            strcpy(str, sign ? "-inf" : "inf");
        }
        return;
    }

    int integer_part = 0;
    int frac_part = 0;

    if (exp != 0) {
        int e = exp - 15;

        uint32_t significand = (1 << 10) | frac;

        integer_part = significand >> 10;
        frac_part = significand & ((1 << 10) - 1);
    }

    if (sign)
        sprintf(str, "-%d.%d", integer_part, frac_part);
    else
        sprintf(str, "%d.%d", integer_part, frac_part);
}