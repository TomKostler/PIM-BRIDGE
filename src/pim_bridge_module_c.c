#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>

#include <linux/types.h>
#include <linux/ioport.h>


#include "../include/pim_memory_region.h"
#include "../include/bins.h"



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tom Kostler");
MODULE_DESCRIPTION("Kernel-Module acting as a driver for PIM-Functionality");

volatile u32 __iomem *pim_data_virt_addr = NULL;
volatile u32 __iomem *pim_config_virt_addr = NULL;



static int __init pim_bridge_init(void) {
    pr_warn("Loading PIM-Bridge kernel module\n");

    pim_config_virt_addr = ioremap(PIM_CONFIG_MEMORY_REGION_BASE, PIM_CONFIG_MEMORY_REGION_SIZE);
    pim_data_virt_addr = ioremap(PIM_DATA_MEMORY_REGION_BASE, PIM_DATA_MEMORY_REGION_SIZE);

    if (!pim_config_virt_addr) {
        pr_err("ioremap config failed\n");
        return -ENOMEM;
    }

    if (!pim_data_virt_addr) {
        pr_err("ioremap data failed\n");
        return -ENOMEM;
    }

    pr_info("Initialized PIM-Region starting at: %px\n", pim_data_virt_addr);
    // vadd_driver_code();
    // vmul_driver_code();
    gemv_driver_code();

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