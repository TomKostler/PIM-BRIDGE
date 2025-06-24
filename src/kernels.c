#include <linux/errno.h>
#include <linux/kernel.h>

#include "../include/microkernels/kernel_datastructures.h"
#include "../include/microkernels/kernels.h"

int build_kernel_vadd_X1(Microkernel *kernel_vadd) {

    Instruction instr0;
    Instruction instr1;
    Instruction instr2;
    Instruction instr3;
    Instruction instr4;

    instr0.type = MOV;
    instr0.mov.src.type = BANK;
    instr0.mov.dst.type = GRF_A;
    instr0.mov.dst.grfa.index = 0;

    instr1.type = ADD;
    instr1.add.src0.type = BANK;
    instr1.add.src1.type = GRF_A;
    instr1.add.src1.grfa.index = 0;
    instr1.add.dst.type = GRF_B;
    instr1.add.dst.grfb.index = 0;
    instr1.add.aam = false;

    instr2.type = FILL;
    instr2.fill.src.type = GRF_B;
    instr2.fill.src.grfb.index = 0;
    instr2.fill.dst.type = BANK;


    instr3.type = EXIT;

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

    kernel_vadd->blocks = 1;
    return 0;
}

int build_kernel_vadd_X2(Microkernel *kernel_vadd) {

    Instruction instr0;
    Instruction instr1;
    Instruction instr2;
    Instruction instr3;
    Instruction instr4;
    Instruction instr5;
    Instruction instr6;
    Instruction instr7;

    instr0.type = MOV;
    instr0.mov.src.type = BANK;
    instr0.mov.dst.type = GRF_A;
    instr0.mov.dst.grfa.index = 0;

    instr1.type = MOV;
    instr1.mov.src.type = BANK;
    instr1.mov.dst.type = GRF_A;
    instr1.mov.dst.grfa.index = 1;

    instr2.type = ADD;
    instr2.add.src0.type = BANK;
    instr2.add.src1.type = GRF_A;
    instr2.add.dst.grfa.index = 0;
    instr2.add.dst.type = GRF_B;
    instr2.add.dst.grfb.index = 0;
    instr2.add.aam = false;

    instr3.type = ADD;
    instr3.add.src0.type = BANK;
    instr3.add.src1.type = GRF_A;
    instr3.add.dst.grfa.index = 1;
    instr3.add.dst.type = GRF_B;
    instr3.add.dst.grfb.index = 1;
    instr3.add.aam = false;

    instr4.type = FILL;
    instr4.fill.src.type = GRF_B;
    instr4.fill.src.grfb.index = 0;
    instr4.fill.dst.type = BANK;

    instr5.type = FILL;
    instr5.fill.src.type = GRF_B;
    instr5.fill.src.grfb.index = 1;
    instr5.fill.dst.type = BANK;

    instr6.type = EXIT;

    instr7.type = NOP;

    kernel_vadd->kernel[0] = instr0;
    kernel_vadd->kernel[1] = instr1;
    kernel_vadd->kernel[2] = instr2;
    kernel_vadd->kernel[3] = instr3;
    kernel_vadd->kernel[4] = instr4;
    kernel_vadd->kernel[5] = instr5;
    kernel_vadd->kernel[6] = instr6;
    kernel_vadd->kernel[7] = instr7;
    kernel_vadd->kernel[8] = instr7;
    kernel_vadd->kernel[9] = instr7;
    kernel_vadd->kernel[10] = instr7;
    kernel_vadd->kernel[11] = instr7;
    kernel_vadd->kernel[12] = instr7;
    kernel_vadd->kernel[13] = instr7;
    kernel_vadd->kernel[14] = instr7;
    kernel_vadd->kernel[15] = instr7;
    kernel_vadd->kernel[16] = instr7;
    kernel_vadd->kernel[17] = instr7;
    kernel_vadd->kernel[18] = instr7;
    kernel_vadd->kernel[19] = instr7;
    kernel_vadd->kernel[20] = instr7;
    kernel_vadd->kernel[21] = instr7;
    kernel_vadd->kernel[22] = instr7;
    kernel_vadd->kernel[23] = instr7;
    kernel_vadd->kernel[24] = instr7;
    kernel_vadd->kernel[25] = instr7;
    kernel_vadd->kernel[26] = instr7;
    kernel_vadd->kernel[27] = instr7;
    kernel_vadd->kernel[28] = instr7;
    kernel_vadd->kernel[29] = instr7;
    kernel_vadd->kernel[30] = instr7;
    kernel_vadd->kernel[31] = instr7;

    kernel_vadd->blocks = 2;
    return 0;
}

int build_kernel_vmul_X1(Microkernel *kernel_vmul) {
    Instruction instr0;
    Instruction instr1;
    Instruction instr2;
    Instruction instr3;
    Instruction instr4;

    instr0.type = MOV;
    instr0.mov.src.type = BANK;
    instr0.mov.dst.type = GRF_A;
    instr0.mov.dst.grfa.index = 0;

    instr1.type = MUL;
    instr1.mul.src0.type = BANK;
    instr1.mul.src1.type = GRF_A;
    instr1.mul.src1.grfa.index = 0;
    instr1.mul.dst.type = GRF_B;
    instr1.mul.dst.grfb.index = 0;
    instr1.mul.aam = false;

    instr2.type = FILL;
    instr2.fill.src.type = GRF_B;
    instr2.fill.src.grfb.index = 0;
    instr2.fill.dst.type = BANK;

    instr3.type = EXIT;

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

    kernel_vmul->blocks = 1;

    return 0;
}

int build_kernel_vmul_X2(Microkernel *kernel_vmul) {

    Instruction instr0;
    Instruction instr1;
    Instruction instr2;
    Instruction instr3;
    Instruction instr4;
    Instruction instr5;
    Instruction instr6;
    Instruction instr7;

    instr0.type = MOV;
    instr0.mov.src.type = BANK;
    instr0.mov.dst.type = GRF_A;
    instr0.mov.dst.grfa.index = 0;

    instr1.type = MOV;
    instr1.mov.src.type = BANK;
    instr1.mov.dst.type = GRF_A;
    instr1.mov.dst.grfa.index = 1;


    instr2.type = MUL;
    instr2.mul.src0.type = BANK;
    instr2.mul.src1.type = GRF_A;
    instr2.mul.src1.grfa.index = 0;
    instr2.mul.dst.type = GRF_B;
    instr2.mul.dst.grfb.index = 0;
    instr2.mul.aam = false;


    instr3.type = MUL;
    instr3.mul.src0.type = BANK;
    instr3.mul.src1.type = GRF_A;
    instr3.mul.dst.grfa.index = 1;
    instr3.mul.dst.type = GRF_B;
    instr3.mul.dst.grfb.index = 1;
    instr3.mul.aam = false;

    instr4.type = FILL;
    instr4.fill.src.type = GRF_B;
    instr4.fill.src.grfb.index = 0;
    instr4.fill.dst.type = BANK;

    instr5.type = FILL;
    instr5.fill.src.type = GRF_B;
    instr5.fill.src.grfb.index = 1;
    instr5.fill.dst.type = BANK;

    instr6.type = EXIT;

    instr7.type = NOP;

    kernel_vmul->kernel[0] = instr0;
    kernel_vmul->kernel[1] = instr1;
    kernel_vmul->kernel[2] = instr2;
    kernel_vmul->kernel[3] = instr3;
    kernel_vmul->kernel[4] = instr4;
    kernel_vmul->kernel[5] = instr5;
    kernel_vmul->kernel[6] = instr6;
    kernel_vmul->kernel[7] = instr7;
    kernel_vmul->kernel[8] = instr7;
    kernel_vmul->kernel[9] = instr7;
    kernel_vmul->kernel[10] = instr7;
    kernel_vmul->kernel[11] = instr7;
    kernel_vmul->kernel[12] = instr7;
    kernel_vmul->kernel[13] = instr7;
    kernel_vmul->kernel[14] = instr7;
    kernel_vmul->kernel[15] = instr7;
    kernel_vmul->kernel[16] = instr7;
    kernel_vmul->kernel[17] = instr7;
    kernel_vmul->kernel[18] = instr7;
    kernel_vmul->kernel[19] = instr7;
    kernel_vmul->kernel[20] = instr7;
    kernel_vmul->kernel[21] = instr7;
    kernel_vmul->kernel[22] = instr7;
    kernel_vmul->kernel[23] = instr7;
    kernel_vmul->kernel[24] = instr7;
    kernel_vmul->kernel[25] = instr7;
    kernel_vmul->kernel[26] = instr7;
    kernel_vmul->kernel[27] = instr7;
    kernel_vmul->kernel[28] = instr7;
    kernel_vmul->kernel[29] = instr7;
    kernel_vmul->kernel[30] = instr7;
    kernel_vmul->kernel[31] = instr7;

    kernel_vmul->blocks = 2;
    return 0;
}

int build_kernel_gemv(Microkernel *kernel_gemv) {
    Instruction instr0;
    Instruction instr1;
    Instruction instr2;
    Instruction instr3;
    Instruction instr4;
    Instruction instr5;
    Instruction instr6;
    Instruction instr7;
    Instruction instr8;
    Instruction instr9;
    Instruction instr10;
    Instruction instr11;
    Instruction instr12;
    Instruction instr13;
    Instruction instr14;
    Instruction instr15;
    Instruction instr16;
    Instruction instr17;
    Instruction instr18;
    Instruction instr19;

    instr0.type = MOV;
    instr0.mov.src.type = BANK;
    instr0.mov.dst.type = GRF_A;
    instr0.mov.dst.grfa.index = 0;

    instr1.type = MOV;
    instr1.mov.src.type = BANK;
    instr1.mov.dst.type = GRF_A;
    instr1.mov.dst.grfa.index = 1;

    instr2.type = MOV;
    instr2.mov.src.type = BANK;
    instr2.mov.dst.type = GRF_A;
    instr2.mov.dst.grfa.index = 2;

    instr3.type = MOV;
    instr3.mov.src.type = BANK;
    instr3.mov.dst.type = GRF_A;
    instr3.mov.dst.grfa.index = 3;

    instr4.type = MOV;
    instr4.mov.src.type = BANK;
    instr4.mov.dst.type = GRF_A;
    instr4.mov.dst.grfa.index = 4;

    instr5.type = MOV;
    instr5.mov.src.type = BANK;
    instr5.mov.dst.type = GRF_A;
    instr5.mov.dst.grfa.index = 5;

    instr6.type = MOV;
    instr6.mov.src.type = BANK;
    instr6.mov.dst.type = GRF_A;
    instr6.mov.dst.grfa.index = 6;

    instr7.type = MOV;
    instr7.mov.src.type = BANK;
    instr7.mov.dst.type = GRF_A;
    instr7.mov.dst.grfa.index = 7;

    instr8.type = MAC;
    instr8.mac.src0.type = BANK;
    instr8.mac.src1.type = GRF_A;
    instr8.mac.src1.grfa.index = 0;
    instr8.mac.src2.type = GRF_B;
    instr8.mac.src2.grfb.index = 0;
    instr8.mac.dst.type = GRF_B;
    instr8.mac.dst.grfb.index = 0;
    instr8.mac.aam = true;

    instr9.type = JUMP;
    instr9.jump.offset = -1;
    instr9.jump.count = 63;

    instr10.type = FILL;
    instr10.fill.src.type = GRF_B;
    instr10.fill.src.grfb.index = 0;
    instr10.fill.dst.type = BANK;

    instr11.type = FILL;
    instr11.fill.src.type = GRF_B;
    instr11.fill.src.grfb.index = 1;
    instr11.fill.dst.type = BANK;

    instr12.type = FILL;
    instr12.fill.src.type = GRF_B;
    instr12.fill.src.grfb.index = 2;
    instr12.fill.dst.type = BANK;

    instr13.type = FILL;
    instr13.fill.src.type = GRF_B;
    instr13.fill.src.grfb.index = 3;
    instr13.fill.dst.type = BANK;

    instr14.type = FILL;
    instr14.fill.src.type = GRF_B;
    instr14.fill.src.grfb.index = 4;
    instr14.fill.dst.type = BANK;

    instr15.type = FILL;
    instr15.fill.src.type = GRF_B;
    instr15.fill.src.grfb.index = 5;
    instr15.fill.dst.type = BANK;

    instr16.type = FILL;
    instr16.fill.src.type = GRF_B;
    instr16.fill.src.grfb.index = 6;
    instr16.fill.dst.type = BANK;

    instr17.type = FILL;
    instr17.fill.src.type = GRF_B;
    instr17.fill.src.grfb.index = 7;
    instr17.fill.dst.type = BANK;

    instr18.type = EXIT;

    instr19.type = NOP;

    kernel_gemv->kernel[0] = instr0;
    kernel_gemv->kernel[1] = instr1;
    kernel_gemv->kernel[2] = instr2;
    kernel_gemv->kernel[3] = instr3;
    kernel_gemv->kernel[4] = instr4;
    kernel_gemv->kernel[5] = instr5;
    kernel_gemv->kernel[6] = instr6;
    kernel_gemv->kernel[7] = instr7;
    kernel_gemv->kernel[8] = instr8;
    kernel_gemv->kernel[9] = instr9;
    kernel_gemv->kernel[10] = instr10;
    kernel_gemv->kernel[11] = instr11;
    kernel_gemv->kernel[12] = instr12;
    kernel_gemv->kernel[13] = instr13;
    kernel_gemv->kernel[14] = instr14;
    kernel_gemv->kernel[15] = instr15;
    kernel_gemv->kernel[16] = instr16;
    kernel_gemv->kernel[17] = instr17;
    kernel_gemv->kernel[18] = instr18;
    kernel_gemv->kernel[19] = instr19;
    kernel_gemv->kernel[20] = instr19;
    kernel_gemv->kernel[21] = instr19;
    kernel_gemv->kernel[22] = instr19;
    kernel_gemv->kernel[23] = instr19;
    kernel_gemv->kernel[24] = instr19;
    kernel_gemv->kernel[25] = instr19;
    kernel_gemv->kernel[26] = instr19;
    kernel_gemv->kernel[27] = instr19;
    kernel_gemv->kernel[28] = instr19;
    kernel_gemv->kernel[29] = instr19;
    kernel_gemv->kernel[30] = instr19;
    kernel_gemv->kernel[31] = instr19;

    return 0;
}
