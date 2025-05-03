#include "../include/microkernels/kernel_datastructures.h"
#include "../include/microkernels/kernels.h"
#include "../include/pim_data_allocator.h"
#include "../include/pim_init_state.h"
#include "../include/pim_memory_region.h"
#include "../include/pim_vectors.h"
#include "../include/pim_configs.h"


static inline int div_ceil(int x, int y) {
    return (x + y - 1) / y;
}


static int trigger_writes(void __iomem *vector_address, int vector_length) {
    uint16_t zero = 0; // equals f16-value for 0.0... in physical memory

    // Iterate through every chunk => USUALLY ONLY MAKES SENSE WHEN VECTOR IS MULTIPLE OF 256 
    int num_chunks = div_ceil(vector_length, PIM_VECTOR_ELEMENTS);

    for (int i = 0; i < num_chunks; ++i) {
        void __iomem *vec_base = vector_address + (i * PIM_VECTOR_ELEMENTS * CHUNK_SIZE_BITS) / 8;
        iowrite16(zero, vec_base);
        pr_info("WRITE: %p\n", vec_base);
        
        dsb(SY);
    }

    return 0;
}


static int trigger_reads(void __iomem *vector_address, int vector_length) {
    // Iterate through every chunk => ONLY MAKES SENSE WHEN VECTOR IS MULTIPLE OF 256
    int num_chunks = div_ceil(vector_length, PIM_VECTOR_ELEMENTS);

    for (int i = 0; i < num_chunks; ++i) {
        // Trigger an HBM-DRAM read to synchronize PIM-UNITS in DRAMSys
        volatile uint8_t val = ioread8(vector_address + (i*CHUNK_SIZE_BITS) / 8);
        (void)val; // Signals Compiler val is being used => No warning
        pr_info("READ: %p\n", vector_address);

        dsb(SY);
    }
    
    return 0;
}


static int vadd_execute(uint32_t __iomem *vector_a_address,
                        uint32_t __iomem *vector_b_address,
                        uint32_t __iomem *vector_result_address,
                        uint32_t __iomem *dummy_region_address,
                        int vector_length) {


    trigger_reads(vector_a_address, vector_length);
    pr_info("vector_a_address: %p\n", vector_a_address);
    trigger_reads(vector_b_address, vector_length);
    pr_info("vector_b_address: %p\n", vector_b_address);

    
    trigger_writes(vector_result_address, vector_length);
    pr_info("vector_result_address: %p\n", vector_result_address);


    // Dummy-Region Read
    volatile uint8_t val = ioread8(dummy_region_address);
    (void)val;


    return 0;
}



void vadd_driver_code(void) {

    // Set Microkernel to vadd
    int success_kernel = set_kernel(build_kernel_vadd);
    pr_info("building kernel success: %d\n", success_kernel);

    // Use FP16 numbers to add as uint_16 bit representation, since kernel isn't
    // allowing floats!
    uint16_t vector_arr_a[] = {
        0x0000, // 0.0
        0x3C00, // 1.0
        0x4000, // 2.0"
    };
    uint32_t __iomem *vector_a_address = init_vector(vector_arr_a, 3);

    uint16_t vector_arr_b[] = {
        0x3C00, // 1.0
        0x4000, // 2.0
        0x0000, // 0.0
    };
    uint32_t __iomem *vector_b_address = init_vector(vector_arr_b, 3);

    // Init result vector
    uint32_t __iomem *vector_result_address = init_vector_result(3);

    // Init dummy memory region for syncing operations that don't need data from
    // memory
    uint32_t __iomem *dummy_region_address = init_dummy_memory_region();

    // Guarantee that everything is working correctly
    dsb(SY);

    // Config for PIM-VM
    set_bank_mode(ALL_BANK);

    // m5_exit(0);

    vadd_execute(vector_a_address, vector_b_address, vector_result_address,
                 dummy_region_address, 3);

    // m5_exit(0);

    // char str[50];
    // uint16_t value = 0x3E47;
    // half_to_string(value, str);
    // pr_info("Das Ergebnis ist: %s\n", str);
}
