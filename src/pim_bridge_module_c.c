#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/types.h>

#include "../include/pim_memory_region.h"
#include "../include/vadd.h"



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tom Kostler");
MODULE_DESCRIPTION("Kernel-Module acting as a driver for PIM-Functionality");


volatile u32 __iomem *pim_data_virt_addr = NULL;
volatile u32 __iomem *pim_config_virt_addr = NULL;



// gem5 Magic Instruction
static void m5_exit(uint64_t code)
{
    asm volatile(
        ".inst 0xff000110 | (0x21 << 16)\n"
        :
        : "r"(code)
        : "memory"
    );
}



static int __init pim_bridge_init(void) {
    pr_info("Loading PIM-Bridge kernel module\n");

    pim_data_virt_addr = ioremap_wc(PIM_DATA_MEMORY_REGION_BASE, PIM_DATA_MEMORY_REGION_SIZE);
    pim_config_virt_addr = ioremap_wc(PIM_CONFIG_MEMORY_REGION_BASE, PIM_CONFIG_MEMORY_REGION_SIZE);
    if (!pim_data_virt_addr || !pim_config_virt_addr) {
        pr_err("ioremap failed\n");
        return -ENOMEM;
    }
    pr_info("Initialized PIM-Region starting at: %p\n", pim_data_virt_addr);

    vadd_driver_code();

    return 0;
}




static void __exit pim_bridge_exit(void) {

    if (pim_data_virt_addr)
        iounmap(pim_data_virt_addr);
    if (pim_config_virt_addr)
        iounmap(pim_config_virt_addr);

    pr_info("Unloading PIM-Bridge kernel module\n");
}

module_init(pim_bridge_init);
module_exit(pim_bridge_exit);