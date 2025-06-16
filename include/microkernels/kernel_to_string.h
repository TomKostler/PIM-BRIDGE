#ifndef KERNEL_TO_STRING_H
#define KERNEL_TO_STRING_H

#include "kernel_datastructures.h"

/**
 * Serializes a single PIM instruction struct into its full JSON string
 * equivalent for transmission to the PIM-VM.
 */
int parse_instruction_to_string(char *buffer, size_t size,
                                const Instruction *instr);

#endif