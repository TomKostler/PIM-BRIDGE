#include <linux/kernel.h>
#include <linux/module.h>

#include "../include/pim_init_state.h"
#include "../include/pim_memory_region.h"
#include "../include/microkernels/kernels.h"


int write_config_bytes(const char *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        iowrite8(data[i], pim_config_virt_addr + i);
        dsb(SY);
    }


    // test if string was written correctly:
    pr_info("--------- PIM_CONFIG is ---------:");
    char buffer[2048];
    size_t i;
    for (i = 0; i < length; ++i) {
        char val = ioread8(pim_config_virt_addr + i);
        if (val == '\0') {
            break;
        }
        buffer[i] = val;
    }
    buffer[i] = '\0';
    pr_info("Buffer geschrieben: %s\n", buffer);


    dsb(SY);
    return 0;
}


int set_bank_mode(pim_bank_mode_t bank_mode) {
	pr_info("Setting Bank Mode to: %d\n", bank_mode);

    char buffer[128];
    const char *bank_mode_str;

    switch (bank_mode) {
        case SINGLE_BANK: 
        	bank_mode_str = "SingleBank";
        	break;
        case ALL_BANK: 
        	bank_mode_str = "AllBank";
        	break;
        case PIM_ALL_BANK: 
        	bank_mode_str = "PimAllBank";
        	break;
        default:
            pr_err("PIM Config Error: Unknown bank mode %d\n", bank_mode);
            return -EINVAL;
    }

    snprintf(buffer, sizeof(buffer), "{\"bank_mode\":\"%s\",\"kernel\":null}", bank_mode_str);
    pr_info("Buffer: %s\n", buffer);

    write_config_bytes(buffer, strlen(buffer));

	return 0;
}



int set_kernel(kernel_builder_t builder) {
	Microkernel kernel;
	int success_kernel = builder(&kernel);


	// TODO: PARSING IN USER_SPACE LIB!!!

	#define BUFFER_SIZE 2048
    char *buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!buffer) {
        pr_err("Couldn't allocate memory\n");
        return -ENOMEM;
    }

    char *ptr = buffer;
    size_t remaining = BUFFER_SIZE;
    int written = 0;
    int total_written = 0;
    int result = 0;

    written = snprintf(ptr, remaining, "{\"bank_mode\":null,\"kernel\":[");
    if (written < 0 || written >= remaining) { result = -ENOMEM; goto cleanup_error; }
    ptr += written; remaining -= written; total_written += written;

    for (int i = 0; i < 32; ++i) {
        if (i > 0) {
            written = snprintf(ptr, remaining, ","); // for the comma
            if (written < 0 || written >= remaining) { result = -ENOMEM; goto cleanup_error; }
             ptr += written; remaining -= written; total_written += written;
        }
 
        
        written = parse_instruction_to_string(ptr, remaining, &kernel.kernel[i]);
        if (written < 0) { result = written; goto cleanup_error; }
        if (written >= remaining) { result = -ENOMEM; goto cleanup_error; }
        ptr += written; remaining -= written; total_written += written;
    }

    written = snprintf(ptr, remaining, "]}");
    if (written < 0 || written >= remaining) { result = -ENOMEM; goto cleanup_error; }
    total_written += written;


    write_config_bytes(buffer, strlen(buffer));




cleanup_error:
    if (result != 0) {
        pr_err("Error at parsing (Code: %d)\n", result);
    }
    kfree(buffer);
    return result;


	return 0;
}