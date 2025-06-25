#include <linux/slab.h>

#include "../include/microkernels/kernel_to_string.h"
#include "../include/microkernels/kernels.h"
#include "../include/pim_init_state.h"
#include "../include/pim_memory_region.h"

int write_config_bytes(const char *data, size_t length) {
    size_t i;
    for (i = 0; i < length; i++) {
        iowrite8(data[i], pim_config_virt_addr + i);
        dsb(SY);
    }

    iowrite8('\0', pim_config_virt_addr + i);
    return 0;
}

int set_bank_mode(pim_bank_mode_t bank_mode) {
    char buffer[128];
    const char *bank_mode_str;

    pr_info("Setting Bank Mode to: %d\n", bank_mode);

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

    snprintf(buffer, sizeof(buffer), "{\"bank_mode\":\"%s\",\"kernel\":null}",
             bank_mode_str);

    write_config_bytes(buffer, strlen(buffer));

    return 0;
}

int set_kernel(kernel_builder_t builder) {
    Microkernel kernel;
    int ret;
    char *buffer;
    char *ptr;
    size_t remaining;
    int written;
    int total_written;
    int result;

    ret = builder(&kernel);
    if (ret != 0) {
        pr_err("PIM: Kernel builder function failed with error %d\n", ret);
        return ret;
    }

#define BUFFER_SIZE 2048
    buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!buffer) {
        pr_err("Couldn't allocate memory\n");
        return -ENOMEM;
    }
    ptr = buffer;
    remaining = BUFFER_SIZE;
    written = 0;
    total_written = 0;
    result = 0;

    written = snprintf(ptr, remaining, "{\"bank_mode\":null,\"kernel\":[");
    if (written < 0 || written >= remaining) {
        result = -ENOMEM;
        goto cleanup_error;
    }
    ptr += written;
    remaining -= written;
    total_written += written;

    for (int i = 0; i < 32; ++i) {
        if (i > 0) {
            written = snprintf(ptr, remaining, ","); // for the comma
            if (written < 0 || written >= remaining) {
                result = -ENOMEM;
                goto cleanup_error;
            }
            ptr += written;
            remaining -= written;
            total_written += written;
        }

        written =
            parse_instruction_to_string(ptr, remaining, &kernel.kernel[i]);

        if (written < 0) {
            result = written;
            goto cleanup_error;
        }
        if (written >= remaining) {
            result = -ENOMEM;
            goto cleanup_error;
        }
        ptr += written;
        remaining -= written;
        total_written += written;
    }
    written = snprintf(ptr, remaining, "]}");
    if (written < 0 || written >= remaining) {
        result = -ENOMEM;
        goto cleanup_error;
    }
    ptr += written;
    remaining -= written;
    total_written += written;

    write_config_bytes(buffer, strlen(buffer));

cleanup_error:
    if (result != 0) {
        pr_err("Error at parsing (Code: %d)\n", result);
    }
    kfree(buffer);
    return kernel.blocks;
}