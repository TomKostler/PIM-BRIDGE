#ifndef KERNEL_TO_STRING_H
#define KERNEL_TO_STRING_H

#include "kernel_datastructures.h"


static int parse_file_to_string(char *buffer, size_t size, const File *file);

int parse_instruction_to_string(char *buffer, size_t size,
                                const Instruction *instr);

#endif