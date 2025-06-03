#include "kernel_datastructures.h"

#ifndef KERNEL_H
#define KERNEL_H

int build_kernel_vadd(Microkernel* kernel_vadd);

int parse_instruction_to_string(char *buffer, size_t size, const Instruction *instr);


#endif