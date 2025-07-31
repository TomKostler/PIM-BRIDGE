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
#include <linux/io.h>
#include <linux/ktime.h>

// For Testing => Contains f16-Floats in UINT16-Format
#include "../../testing_helper/f16_lut.h"

/**
 * Executes a vector addition (VADD) operation on the PIM units by triggering
 * memory-mapped reads and writes to perform MOV, ADD, FILL, and EXIT commands
 * across all data chunks in the input vectors.
 */
static int vadd_execute(uint16_t __iomem *vector_a_address,
                        uint16_t __iomem *vector_b_address,
                        uint16_t __iomem *vector_result_address,
                        uint16_t __iomem *dummy_region_address,
                        int vector_length, int kernel_blocks) {

    int num_chunks = (vector_length + (NUM_BANKS * ELEMENTS_PER_BANK - 1)) /
                     (NUM_BANKS * ELEMENTS_PER_BANK);

    // Loop through all 512 Bit of the vectors => Calculated in Chunks
    for (int i = 0; i < num_chunks; i++) {

        const int chunk_size_elements = NUM_BANKS * ELEMENTS_PER_BANK;

        // Triggers MOV in PIM-VM
        for (int j = 0; j < kernel_blocks; j++) {
            trigger_read(vector_a_address + chunk_size_elements * i);
        }
        mb();

        // Triggers ADD in PIM-VM
        for (int j = 0; j < kernel_blocks; j++) {
            trigger_read(vector_b_address + chunk_size_elements * i);
        }
        mb();

        // Trigers FILL in PIM-VM
        for (int j = 0; j < kernel_blocks; j++) {
            trigger_write(vector_result_address + chunk_size_elements * i);
        }
        mb();

        // Dummy-Region Read => Triggers EXIT in PIM-VM
        trigger_read(dummy_region_address);
        mb();
    }
    return 0;
}

/**
 * Executes a vector addition (VADD) operation using predefined kernel-space
 * vectors.
 */
void vadd_driver_code(void) {

    int kernel_blocks;
    uint16_t *vector_arr_a = NULL;
    uint16_t __iomem *vector_a_address;
    uint16_t *vector_arr_b = NULL;
    uint16_t __iomem *vector_b_address = NULL;
    uint16_t __iomem *vector_result_address = NULL;
    uint16_t __iomem *dummy_region_address = NULL;
    int i;

    // ROWS = 1 << 20;
    ROWS = 256;

    vector_arr_a = vmalloc(ROWS * sizeof(uint16_t));
    vector_arr_b = vmalloc(ROWS * sizeof(uint16_t));

    // Set Microkernel to vadd
    kernel_blocks = set_kernel(build_kernel_vadd_X1);
    pr_err("kernel_blocks: %d\n", kernel_blocks);

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
    for (i = 0; i < ROWS; i++) {
        vector_arr_a[i] =
            f16_integer_lookup_table[i]; // Defined in the f16_lut.h
    }
    vector_a_address = init_vector(vector_arr_a, ROWS);

    for (i = 0; i < ROWS; i++) {
        vector_arr_b[i] = f16_integer_lookup_table[ROWS - i];
    }
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

    vfree(vector_arr_a);
    vfree(vector_arr_b);
}

/**
 * Performs a vector addition (VADD) operation using the PIM architecture.
 * Takes two input vectors from user space, executes the addition in memory, and
 * copies the result back to user space.
 */
int vadd_from_userspace(uint16_t *vector_arr_a, uint16_t *vector_arr_b,
                        struct pim_vectors *vectors_descriptor) {

    const int ROWS = vectors_descriptor->len1;
    int kernel_blocks;
    uint16_t __iomem *vector_a_address;
    uint16_t __iomem *vector_b_address;
    uint16_t __iomem *vector_result_address;
    uint16_t __iomem *dummy_region_address;

    kernel_builder_t builder;

    if (ROWS == 256) {
        builder = build_kernel_vadd_X1;
    } else if (ROWS == 512) {
        builder = build_kernel_vadd_X2;
    } else if (ROWS == 1024) {
        builder = build_kernel_vadd_X3;
    } else if (ROWS >= 2048) {
        builder = build_kernel_vadd_X4;
    } else {
        pr_err(
            "Vector length must be at least 256. If the vectors are too short, "
            "just fill them up with zeros.");
        return -EINVAL;
    }

    kernel_blocks = set_kernel(builder);

    vector_a_address = init_vector(vector_arr_a, ROWS);
    if (!vector_a_address) {
        pr_err("PIM: Failed to init vector a\n");
        return -ENOMEM;
    }

    vector_b_address = init_vector(vector_arr_b, ROWS);
    if (!vector_b_address) {
        pr_err("PIM: Failed to init vector b\n");
        return -ENOMEM;
    }

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

    vadd_execute(vector_a_address, vector_b_address, vector_result_address,
                 dummy_region_address, ROWS, kernel_blocks);

    set_bank_mode(SINGLE_BANK);

    vectors_descriptor->result_offset =
        (uint64_t)vector_result_address - (uint64_t)pim_data_virt_addr;

    return 0;
}