#ifndef PIM_INIT_STATE_H
#define PIM_INIT_STATE_H

#include "microkernels/kernel_datastructures.h"

typedef enum { SINGLE_BANK, ALL_BANK, PIM_ALL_BANK } pim_bank_mode_t;

// Function pointer for Strategy pattern
typedef int (*kernel_builder_t)(Microkernel *);

/**
 * Writes a raw byte stream to the PIM device's PIM_CONFIG memory region.
 */
int write_config_bytes(const char *data, size_t length);

/**
 * Configures the PIM device's operational bank mode by writing a formatted JSON
 * string into the PIM_CONFIG region.
 */
int set_bank_mode(pim_bank_mode_t bank_mode);

/**
 * Compiles a microkernel into a JSON string and writes it into the PIM_CONFIG
 * region.
 */
int set_kernel(kernel_builder_t builder);

#endif