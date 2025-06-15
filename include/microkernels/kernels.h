#include "kernel_datastructures.h"

#ifndef KERNEL_H
#define KERNEL_H

int build_kernel_vadd_X1(Microkernel *kernel_vadd);
int build_kernel_vadd_X2(Microkernel *kernel_vadd);

int build_kernel_vmul_X1(Microkernel *kernel_vmul);
int build_kernel_vmul_X2(Microkernel *kernel_vmul);

int build_kernel_gemv(Microkernel *kernel_vmul);

#endif