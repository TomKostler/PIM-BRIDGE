#include <linux/slab.h>
#include <linux/uaccess.h>

#include "../../include/microkernels/kernel_datastructures.h"
#include "../../include/microkernels/kernels.h"
#include "../../include/pim_configs.h"
#include "../../include/pim_data_allocator.h"
#include "../../include/pim_init_state.h"
#include "../../include/pim_memory_region.h"
#include "../../include/pim_vectors.h"
#include "../../include/read_write_triggers.h"

// For Testing => Contains f16-Floats in UINT16-Format
#include "../../testing_helper/f16_lut.h"

static int vadd_execute(uint16_t __iomem *vector_a_address,
                        uint16_t __iomem *vector_b_address,
                        uint16_t __iomem *vector_result_address,
                        uint16_t __iomem *dummy_region_address,
                        int vector_length, int kernel_blocks) {

    int num_chunks = (vector_length + (NUM_BANKS * ELEMENTS_PER_BANK - 1)) /
                     (NUM_BANKS * ELEMENTS_PER_BANK);
    pr_err("num_chunks reads: %d, kernel_blocks: %d\n", num_chunks,
           kernel_blocks);

    // Loop through all 512 Bit of the vectors => Calculated in Chunks
    for (int i = 0; i < num_chunks; i++) {

        const int chunk_size_elements = NUM_BANKS * ELEMENTS_PER_BANK;

        // Triggers MOV in PIM-VM
        for (int j = 0; j < kernel_blocks; j++) {
            trigger_read(vector_a_address + chunk_size_elements * i);
            pr_info("vector_a read triggered, address: %px\n",
                    vector_a_address);
        }

        // Triggers ADD in PIM-VM
        for (int j = 0; j < kernel_blocks; j++) {
            trigger_read(vector_b_address + chunk_size_elements * i);
            pr_info("vector_b read triggered, address: %px\n",
                    vector_b_address);
        }

        // Trigers FILL in PIM-VM
        for (int j = 0; j < kernel_blocks; j++) {
            trigger_write(vector_result_address + chunk_size_elements * i);
            pr_info("vector_result write triggered, address: %px\n",
                    vector_result_address);
        }

        // Dummy-Region Read => Triggers EXIT in PIM-VM
        trigger_read(dummy_region_address);
    }

    return 0;
}

void vadd_driver_code(void) {

    const int ROWS = 256;
    int kernel_blocks;
    uint16_t vector_arr_a[ROWS];
    uint16_t __iomem *vector_a_address;
    uint16_t vector_arr_b[ROWS];
    uint16_t __iomem *vector_b_address;
    uint16_t __iomem *vector_result_address;
    uint16_t __iomem *dummy_region_address;
    int i;

    // Set Microkernel to vadd
    kernel_blocks = set_kernel(build_kernel_vadd_X1);
    pr_err("kernel_blocks: %d\n", kernel_blocks);

    // Use FP16 numbers to add as uint_16 bit representation, since kernel isn't
    // allowing floats!
    // uint16_t vector_arr_a[] = {
    //     0x0000, // 0.0
    //     0x3C00, // 1.0
    //     0x4000, // 2.0"
    // };
    // uint16_t __iomem *vector_a_address = init_vector(vector_arr_a, ROWS);

    // uint16_t vector_arr_b[] = {
    //     0x3C00, // 1.0
    //     0x4000, // 2.0
    //     0x0000, // 0.0
    // };
    // uint16_t __iomem *vector_b_address = init_vector(vector_arr_b, ROWS);

    // Pattern Sample
    uint16_t pattern_a[] = {
        0x0000, // 0.0
        0x3C00, // 1.0
        0x4000, // 2.0"
    };
    for (int i = 0; i < 256; i++) {
        vector_arr_a[i] = pattern_a[i % 3];
    }
    vector_a_address = init_vector(vector_arr_a, 256);

    uint16_t pattern_b[] = {
        0x3C00, // 1.0
        0x4000, // 2.0
        0x0000, // 0.0
    };
    for (int i = 0; i < 256; i++) {
        vector_arr_b[i] = pattern_b[i % 3];
    }
    vector_b_address = init_vector(vector_arr_b, 256);

    // Example for testing for different row-sizes and kernel_block sizes
    // for (i = 0; i < ROWS; i++) {
    //     vector_arr_a[i] =
    //         f16_integer_lookup_table[i]; // Defined in the f16_lut.h
    // }
    // vector_a_address = init_vector(vector_arr_a, ROWS);

    // for (i = 0; i < ROWS; i++) {
    //     // Einfacher und schneller Tabellenzugriff
    //     vector_arr_b[i] = f16_integer_lookup_table[ROWS - i];
    // }
    // vector_b_address = init_vector(vector_arr_b, ROWS);

    // Init result vector
    vector_result_address = init_vector_result(ROWS);

    // Init dummy memory region for syncing operations that don't need data from
    // memory
    dummy_region_address = init_dummy_memory_region();

    // Guarantee that vectors and PIM_DATA_REGION is correctly initialized
    dsb(SY);

    // Switch to PIM_ALL_BANK to notifiy DRAM-Controller that reads/writes to
    // PIM Regions should be forwarded to PIM-VM
    set_bank_mode(PIM_ALL_BANK);

    vadd_execute(vector_a_address, vector_b_address, vector_result_address,
                 dummy_region_address, ROWS, kernel_blocks);

    set_bank_mode(SINGLE_BANK);

    // Evaluate the result and check, if the PIM-VM executed a correct vadd
    pr_err("--------- RESULT-VECTOR IS ---------:");
    for (i = 0; i < ROWS; i++) {
        uint16_t val = ioread16(vector_result_address);
        pr_err("VAL: (hex): 0x%x\n", val);
        vector_result_address++;
    }
}

void vadd_from_userspace(__u64 result_addr, uint16_t *vector_arr_a,
                         uint16_t *vector_arr_b, int len) {
    const int ROWS = len;
    int kernel_blocks;
    uint16_t __iomem *vector_a_address;
    uint16_t __iomem *vector_b_address;
    uint16_t __iomem *vector_result_address;
    uint16_t __iomem *dummy_region_address;
    uint16_t *kernel_result_buffer = NULL;
    int i;

    kernel_blocks = set_kernel(build_kernel_vadd_X1);
    pr_err("kernel_blocks: %d\n", kernel_blocks);

    vector_a_address = init_vector(vector_arr_a, ROWS);
    uint16_t __iomem *vec_a_addr = vector_a_address;
    vector_b_address = init_vector(vector_arr_b, ROWS);
    uint16_t __iomem *vec_b_addr = vector_b_address;

    // pr_err("--------- vector a IS ---------:");
    // for (i = 0; i < ROWS; i++) {
    //     uint16_t val = ioread16(vec_a_addr);
    //     pr_err("VAL: (hex): 0x%x\n", val);
    //     vec_a_addr++;
    // }
    // pr_err("--------- vector b IS ---------:");
    // for (i = 0; i < ROWS; i++) {
    //     uint16_t val = ioread16(vec_b_addr);
    //     pr_err("VAL: (hex): 0x%x\n", val);
    //     vec_b_addr++;
    // }

    // Init result vector
    vector_result_address = init_vector_result(ROWS);

    // Init dummy memory region for syncing operations that don't need data from
    // memory
    dummy_region_address = init_dummy_memory_region();

    // Guarantee that vectors and PIM_DATA_REGION is correctly initialized
    dsb(SY);

    // Switch to PIM_ALL_BANK to notifiy DRAM-Controller that reads/writes to
    // PIM Regions should be forwarded to PIM-VM
    set_bank_mode(PIM_ALL_BANK);

    vadd_execute(vector_a_address, vector_b_address, vector_result_address,
                 dummy_region_address, ROWS, kernel_blocks);

    set_bank_mode(SINGLE_BANK);

    // Init a buffer for the result
    kernel_result_buffer = kmalloc(ROWS * sizeof(uint16_t), GFP_KERNEL);
    if (!kernel_result_buffer) {
        pr_err("PIM: Failed to allocate kernel buffer for result\n");
        return;
    }

    // Fill the result Buffer
    // pr_err("--------- VADD: RESULT-VECTOR IS ---------:");
    for (i = 0; i < ROWS; i++) {
        kernel_result_buffer[i] = ioread16(vector_result_address + i);
        // pr_err("VAL: (hex): 0x%x\n", kernel_result_buffer[i]);
    }

    // Copy the result back to the userspace
    if (copy_to_user((void __user *)result_addr, kernel_result_buffer,
                     ROWS * sizeof(uint16_t))) {
        pr_err("PIM: Failed to copy result vector to user space\n");
    } else {
        pr_info("PIM: Successfully copied result to user space.\n");
    }

    kfree(kernel_result_buffer);
}