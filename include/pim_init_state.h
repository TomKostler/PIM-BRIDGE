#ifndef PIM_INIT_STATE_H
#define PIM_INIT_STATE_H

#include "microkernels/kernel_datastructures.h"

typedef enum {
	SINGLE_BANK,
	ALL_BANK,
	PIM_ALL_BANK
} pim_bank_mode_t;


// Function pointer for Strategy pattern
typedef int (*kernel_builder_t)(Microkernel*);


int write_config_bytes(const char *data, size_t length);

int set_bank_mode(pim_bank_mode_t bank_mode);

int set_kernel(kernel_builder_t builder);


#endif