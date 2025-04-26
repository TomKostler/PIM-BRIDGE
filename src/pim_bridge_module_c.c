#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/types.h>

#include "../include/pim_data_memory_region.h"
#include "../include/pim_vectors.h"
#include "../include/pim_data_allocator.h"



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tom Kostler");
MODULE_DESCRIPTION("Kernel-Module acting as a driver for PIM-Functionality");


volatile u32 __iomem *pim_data_virt_addr = NULL;
void vadd_driver_code(void);

static int __init pim_bridge_init(void) {
    pr_info("Loading PIM-Bridge kernel module\n");


    pim_data_virt_addr = ioremap(PIM_DATA_MEMORY_REGION_BASE, PIM_DATA_MEMORY_REGION_SIZE);
    if (!pim_data_virt_addr) {
        pr_err("ioremap failed\n");
        return -ENOMEM;
    }
    pr_info("Initialized PIM-Region starting at: %p\n", pim_data_virt_addr);

    vadd_driver_code();

    return 0;
}



void vadd_driver_code() {
    // Use FP16 numbers to add as uint_16 bit representation, since kernel isn't
    // allowing floats!
    uint16_t vector_arr_a[] = {
        0x0000, // 0.0
        0x3C00, // 1.0
        0x4000, // 2.0"
    };
    int success_a = init_vector(vector_arr_a, 3);
    pr_info("init_vector a success: %d\n", success_a);


    uint16_t vector_arr_b[] = {
        0x3C00, // 1.0
        0x4000, // 2.0
        0x0000, // 0.0
    };
    int success_b = init_vector(vector_arr_b, 3);
    pr_info("init_vector b success: %d\n", success_b);


    // Init result vector
    int success_c = init_vector_result(3);
    pr_info("init_vector result success: %d\n", success_c);


    // Init dummy memory region for syncing operations that don't need data from memory
    int success_dummy_region = init_dummy_memory_region();
    pr_info("dummy region allocation success: %d\n", success_dummy_region);


    // Guarantee that everything is working correctly
    mb();


    // char str[50];
    // uint16_t value = 0x3E47;
    // half_to_string(value, str);
    // pr_info("Das Ergebnis ist: %s\n", str);
}





static void __exit pim_bridge_exit(void) {

    if (pim_data_virt_addr)
        iounmap(pim_data_virt_addr);

    pr_info("Unloading PIM-Bridge kernel module\n");
}

module_init(pim_bridge_init);
module_exit(pim_bridge_exit);