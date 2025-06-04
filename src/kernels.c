#include <linux/errno.h>
#include <linux/kernel.h>

#include "../include/microkernels/kernel_datastructures.h"
#include "../include/microkernels/kernels.h"

int build_kernel_vadd(Microkernel *kernel_vadd) {

    Instruction instr0;
    instr0.type = MOV;
    instr0.mov.src.type = BANK;
    instr0.mov.dst.type = GRF_A;
    instr0.mov.dst.grfa.index = 0;

    Instruction instr1;
    instr1.type = ADD;
    instr1.add.src0.type = BANK;
    instr1.add.src1.type = GRF_A;
    instr1.add.dst.grfa.index = 0;
    instr1.add.dst.type = GRF_B;
    instr1.add.dst.grfb.index = 0;
    instr1.add.aam = false;

    Instruction instr2;
    instr2.type = FILL;
    instr2.fill.src.type = GRF_B;
    instr2.fill.src.grfb.index = 0;
    instr2.fill.dst.type = BANK;

    Instruction instr3;
    instr3.type = EXIT;

    Instruction instr4;
    instr4.type = NOP;

    kernel_vadd->kernel[0] = instr0;
    kernel_vadd->kernel[1] = instr1;
    kernel_vadd->kernel[2] = instr2;
    kernel_vadd->kernel[3] = instr3;
    kernel_vadd->kernel[4] = instr4;
    kernel_vadd->kernel[5] = instr4;
    kernel_vadd->kernel[6] = instr4;
    kernel_vadd->kernel[7] = instr4;
    kernel_vadd->kernel[8] = instr4;
    kernel_vadd->kernel[9] = instr4;
    kernel_vadd->kernel[10] = instr4;
    kernel_vadd->kernel[11] = instr4;
    kernel_vadd->kernel[12] = instr4;
    kernel_vadd->kernel[13] = instr4;
    kernel_vadd->kernel[14] = instr4;
    kernel_vadd->kernel[15] = instr4;
    kernel_vadd->kernel[16] = instr4;
    kernel_vadd->kernel[17] = instr4;
    kernel_vadd->kernel[18] = instr4;
    kernel_vadd->kernel[19] = instr4;
    kernel_vadd->kernel[20] = instr4;
    kernel_vadd->kernel[21] = instr4;
    kernel_vadd->kernel[22] = instr4;
    kernel_vadd->kernel[23] = instr4;
    kernel_vadd->kernel[24] = instr4;
    kernel_vadd->kernel[25] = instr4;
    kernel_vadd->kernel[26] = instr4;
    kernel_vadd->kernel[27] = instr4;
    kernel_vadd->kernel[28] = instr4;
    kernel_vadd->kernel[29] = instr4;
    kernel_vadd->kernel[30] = instr4;
    kernel_vadd->kernel[31] = instr4;

    return 0;
}


int build_kernel_vmul(Microkernel* kernel_vmul) {
    Instruction instr0;
    instr0.type = MOV;
    instr0.mov.src.type = BANK;
    instr0.mov.dst.type = GRF_A;
    instr0.mov.dst.grfa.index = 0;

    Instruction instr1;
    instr1.type = MUL;
    instr1.add.src0.type = BANK;
    instr1.add.src1.type = GRF_A;
    instr1.add.dst.grfa.index = 0;
    instr1.add.dst.type = GRF_B;
    instr1.add.dst.grfb.index = 0;
    instr1.add.aam = false;

    Instruction instr2;
    instr2.type = FILL;
    instr2.fill.src.type = GRF_B;
    instr2.fill.src.grfb.index = 0;
    instr2.fill.dst.type = BANK;

    Instruction instr3;
    instr3.type = EXIT;

    Instruction instr4;
    instr4.type = NOP;

    kernel_vmul->kernel[0] = instr0;
    kernel_vmul->kernel[1] = instr1;
    kernel_vmul->kernel[2] = instr2;
    kernel_vmul->kernel[3] = instr3;
    kernel_vmul->kernel[4] = instr4;
    kernel_vmul->kernel[5] = instr4;
    kernel_vmul->kernel[6] = instr4;
    kernel_vmul->kernel[7] = instr4;
    kernel_vmul->kernel[8] = instr4;
    kernel_vmul->kernel[9] = instr4;
    kernel_vmul->kernel[10] = instr4;
    kernel_vmul->kernel[11] = instr4;
    kernel_vmul->kernel[12] = instr4;
    kernel_vmul->kernel[13] = instr4;
    kernel_vmul->kernel[14] = instr4;
    kernel_vmul->kernel[15] = instr4;
    kernel_vmul->kernel[16] = instr4;
    kernel_vmul->kernel[17] = instr4;
    kernel_vmul->kernel[18] = instr4;
    kernel_vmul->kernel[19] = instr4;
    kernel_vmul->kernel[20] = instr4;
    kernel_vmul->kernel[21] = instr4;
    kernel_vmul->kernel[22] = instr4;
    kernel_vmul->kernel[23] = instr4;
    kernel_vmul->kernel[24] = instr4;
    kernel_vmul->kernel[25] = instr4;
    kernel_vmul->kernel[26] = instr4;
    kernel_vmul->kernel[27] = instr4;
    kernel_vmul->kernel[28] = instr4;
    kernel_vmul->kernel[29] = instr4;
    kernel_vmul->kernel[30] = instr4;
    kernel_vmul->kernel[31] = instr4;

    return 0;
}

