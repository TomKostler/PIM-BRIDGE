#include <linux/slab.h>
#include <linux/uaccess.h>

#include "../../include/bins.h"
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

/**
 * Executes a vector multiplication (VMUL) operation on the PIM units by issuing
 * memory-mapped triggers for MOV, MUL, FILL, and EXIT operations across all
 * chunks of the input vectors.
 */
static int vmul_execute(uint16_t __iomem *vector_a_address,
                        uint16_t __iomem *vector_b_address,
                        uint16_t __iomem *vector_result_address,
                        uint16_t __iomem *dummy_region_address,
                        int vector_length, int kernel_blocks) {

    int num_chunks = (vector_length + (NUM_BANKS * ELEMENTS_PER_BANK - 1)) /
                     (NUM_BANKS * ELEMENTS_PER_BANK);

    for (int i = 0; i < num_chunks; i++) {

        const int chunk_size_elements = NUM_BANKS * ELEMENTS_PER_BANK;

        // Triggers MOV in PIM-VM
        for (int j = 0; j < kernel_blocks; j++) {
            trigger_read(vector_a_address + chunk_size_elements * i);
        }
        rmb();

        // Triggers ADD in PIM-VM
        for (int j = 0; j < kernel_blocks; j++) {
            trigger_read(vector_b_address + chunk_size_elements * i);
        }
        rmb();

        // Trigers FILL in PIM-VM
        for (int j = 0; j < kernel_blocks; j++) {
            trigger_write(vector_result_address + chunk_size_elements * i);
        }
        wmb();

        // Dummy-Region Read => Triggers EXIT in PIM-VM
        trigger_read(dummy_region_address);
        rmb();
    }
    return 0;
}

/**
 * Executes a vector multiplication (VMUL) operation entirely within the kernel
 * for testing purposes.
 */
void vmul_driver_code(void) {

    int kernel_blocks;
    uint16_t vector_arr_a[256];
    uint16_t __iomem *vector_a_address;
    uint16_t vector_arr_b[256];
    uint16_t __iomem *vector_b_address;
    uint16_t __iomem *vector_result_address;
    uint16_t __iomem *dummy_region_address;

    ROWS = 256;

    kernel_blocks = set_kernel(build_kernel_vmul_X1);
    pr_info("Building kernel success: %d\n", kernel_blocks);

    // Use FP16 numbers to add as uint_16 bit representation, since kernel isn't
    // allowing floats!
    // Pattern Sample
    // uint16_t pattern_a[] = {
    //     0x0000, // 0.0
    //     0x3C00, // 1.0
    //     0x4000, // 2.0"
    // };
    // for (int i = 0; i < ROWS; i++) {
    //     vector_arr_a[i] = pattern_a[i % 3];
    // }
    // vector_a_address = init_vector(vector_arr_a, ROWS);

    // uint16_t pattern_b[] = {
    //     0x3C00, // 1.0
    //     0x4000, // 2.0
    //     0x0000, // 0.0
    // };
    // for (int i = 0; i < ROWS; i++) {
    //     vector_arr_b[i] = pattern_b[i % 3];
    // }
    // vector_b_address = init_vector(vector_arr_b, ROWS);

    // Example for testing for different row-sizes and kernel_block sizes
    memset16(vector_arr_a, 0x4000, ROWS);
    vector_a_address = init_vector(vector_arr_a, ROWS);

    memset16(vector_arr_b, 0x4200, ROWS);
    vector_b_address = init_vector(vector_arr_b, ROWS);

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

    vmul_execute(vector_a_address, vector_b_address, vector_result_address,
                 dummy_region_address, ROWS, kernel_blocks);

    set_bank_mode(SINGLE_BANK);

    // Evaluate the result and check, if the PIM-VM executed a correct vadd
    pr_err("--------- RESULT-VECTOR IS ---------:");
    for (int i = 0; i < ROWS; i++) {
        uint16_t val = ioread16(vector_result_address);
        pr_err("VAL: (hex): 0x%x\n", val);
        vector_result_address++;
    }
}

/**
 * Executes a vector multiplication (VMUL) operation using input vectors from
 * mapped user space. Initializes PIM memory regions, performs the VMUL
 * operation, and copies the result back to user space.
 */
int vmul_from_userspace(uint16_t *vector_a_address, uint16_t *vector_b_address,
                        struct pim_vectors *vectors_descriptor) {
    const int ROWS = vectors_descriptor->len;
    int kernel_blocks;
    uint16_t __iomem *vector_result_address;
    uint16_t __iomem *dummy_region_address;

    kernel_builder_t builder;
    if (ROWS == 256) {
        builder = build_kernel_vmul_X1;
    } else if (ROWS == 512) {
        builder = build_kernel_vmul_X2;
    } else if (ROWS == 1024) {
        builder = build_kernel_vmul_X3;
    } else if (ROWS >= 2048) {
        builder = build_kernel_vmul_X4;
    } else {
        pr_err(
            "Vector length must be at least 256. If the vectors are too short, "
            "just fill them up with zeros.");
        return -EINVAL;
    }

    kernel_blocks = set_kernel(builder);

    // Init result vector
    vector_result_address = init_vector_result(ROWS);
    if (!vector_result_address) {
        pr_err("PIM: Failed to init result vector\n");
        return -ENOMEM;
    }

    // Init dummy memory region for syncing operations that don't need data from
    // memory
    dummy_region_address = init_dummy_memory_region();
    if (!dummy_region_address) {
        pr_err("PIM: Failed to init dummy region\n");
        return -ENOMEM;
    }

    // Guarantee that vectors and PIM_DATA_REGION is correctly initialized
    dsb(SY);

    // Switch to PIM_ALL_BANK to notifiy DRAM-Controller that reads/writes to
    // PIM Regions should be forwarded to PIM-VM
    set_bank_mode(PIM_ALL_BANK);

    vmul_execute(vector_a_address, vector_b_address, vector_result_address,
                 dummy_region_address, ROWS, kernel_blocks);

    set_bank_mode(SINGLE_BANK);

    vectors_descriptor->result_offset =
        (uint64_t)vector_result_address - (uint64_t)pim_data_virt_addr;

    return 0;
}
