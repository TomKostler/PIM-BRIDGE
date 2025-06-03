#include "../include/microkernels/kernel_datastructures.h"
#include "../include/microkernels/kernels.h"
#include "../include/pim_configs.h"
#include "../include/pim_data_allocator.h"
#include "../include/pim_init_state.h"
#include "../include/pim_memory_region.h"
#include "../include/pim_vectors.h"

static int trigger_write(void __iomem *address) {
    iowrite16(0, address);
    dsb(SY);
    return 0;
}

static int trigger_read(void __iomem *address) {
    volatile uint8_t val = ioread8(address);
    (void)val;
    dsb(SY);
    return 0;
}

static int vadd_execute(uint16_t __iomem *vector_a_address,
                        uint16_t __iomem *vector_b_address,
                        uint16_t __iomem *vector_result_address,
                        uint16_t __iomem *dummy_region_address,
                        int vector_length) {

    int num_chunks = (vector_length + (NUM_BANKS * ELEMENTS_PER_BANK - 1)) /
                     (NUM_BANKS * ELEMENTS_PER_BANK);
    pr_err("num_chunks reads: %d\n", num_chunks);

    for (int i = 0; i < num_chunks; i++) {

        const int chunk_size_elements = NUM_BANKS * ELEMENTS_PER_BANK;

        pr_err("Befor trigger reads\n");

        // Triggers MOV in PIM-VM
        trigger_read(vector_a_address + chunk_size_elements * i);
        pr_err("vector_a_address: %px\n", vector_a_address);

        // Triggers ADD in PIM-VM
        trigger_read(vector_b_address + chunk_size_elements * i);
        pr_info("vector_b_address: %px\n", vector_b_address);

        // Trigers FILL in PIM-VM
        trigger_write(vector_result_address + chunk_size_elements * i);
        pr_info("vector_result_address: %px\n", vector_result_address);

        // Dummy-Region Read => Triggers EXIT in PIM-VM
        trigger_read(dummy_region_address);
    }

    return 0;
}

void vadd_driver_code(void) {

    // Set Microkernel to vadd
    int success_kernel = set_kernel(build_kernel_vadd);
    pr_info("Building kernel success: %d\n", success_kernel);

    // Use FP16 numbers to add as uint_16 bit representation, since kernel isn't
    // allowing floats!
    // uint16_t vector_arr_a[] = {
    //     0x0000, // 0.0
    //     0x3C00, // 1.0
    //     0x4000, // 2.0"
    // };
    // uint16_t __iomem *vector_a_address = init_vector(vector_arr_a, 3);

    // uint16_t vector_arr_b[] = {
    //     0x3C00, // 1.0
    //     0x4000, // 2.0
    //     0x0000, // 0.0
    // };
    // uint16_t __iomem *vector_b_address = init_vector(vector_arr_b, 3);

    uint16_t pattern_a[] = {
        0x0000, // 0.0
        0x3C00, // 1.0
        0x4000, // 2.0"
    };
    uint16_t vector_arr_a[1024];
    for (int i = 0; i < 1024; i++) {
        vector_arr_a[i] = pattern_a[i % 3];
    }
    uint16_t __iomem *vector_a_address = init_vector(vector_arr_a, 1024);

    uint16_t pattern_b[] = {
        0x3C00, // 1.0
        0x4000, // 2.0
        0x0000, // 0.0
    };
    uint16_t vector_arr_b[1024];
    for (int i = 0; i < 1024; i++) {
        vector_arr_b[i] = pattern_b[i % 3];
    }

    uint16_t __iomem *vector_b_address = init_vector(vector_arr_b, 1024);

    // Init result vector
    uint16_t __iomem *vector_result_address = init_vector_result(1024);

    // Init dummy memory region for syncing operations that don't need data from
    // memory
    uint16_t __iomem *dummy_region_address = init_dummy_memory_region();

    // Guarantee that vectors and PIM_DATA_REGION is correctly initialized
    dsb(SY);

    // Switch to PIM_ALL_BANK to notifiy DRAM-Controller that reads/writes to
    // PIM Regions should be forwarded to PIM-VM
    set_bank_mode(PIM_ALL_BANK);

    vadd_execute(vector_a_address, vector_b_address, vector_result_address,
                 dummy_region_address, 1024);

    set_bank_mode(SINGLE_BANK);

    // Evaluate the result and check, if the PIM-VM executed a correct vadd
    pr_err("--------- RESULT-VECTOR IS ---------:");
    for (int i = 0; i < 1024; i++) {
        uint16_t val = ioread16(vector_result_address);
        pr_err("VAL: (hex): 0x%x\n", val);
        vector_result_address++;
    }
}
