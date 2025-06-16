#include "../include/read_write_triggers.h"
#include <linux/io.h>


int trigger_write(void __iomem *address) {
    iowrite8(0, address);
    dsb(SY);
    return 0;
}

int trigger_read(void __iomem *address) {
    volatile uint8_t val = ioread8(address);
    (void)val;
    dsb(SY);
    return 0;
}