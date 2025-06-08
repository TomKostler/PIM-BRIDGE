/*
	File containing datastructures needed for microkernel execution
*/

#ifndef PIM_MICROKERNELS_H
#define PIM_MICROKERNELS_H

#include <linux/types.h>


/* ------- Register and Bank File ------- */


typedef enum {
    GRF_A,
    GRF_B,
    SRF_M,
    SRF_A,
    BANK
} FileType;


typedef struct {
    FileType type;
    
    union {
        struct { uint8_t index; } grfa;
        struct { uint8_t index; } grfb;
        struct { uint8_t index; } srfm;
        struct { uint8_t index; } srfa;
    };
} File;



/* ------- Instructions ------- */

typedef enum {
	NOP,
    EXIT,
    JUMP,
    MOV,
    FILL,
    ADD,
    MUL,
    MAC,
    MAD
} InstructionType;


typedef struct {
    int offset;
    unsigned int count;
} JumpInstr;

typedef struct {
    File src;
    File dst;
} MovInstr;

typedef struct {
    File src;
    File dst;
} FillInstr;

typedef struct {
    File src0;
    File src1;
    File dst;
    bool aam;
} AddInstr;

typedef struct {
    File src0;
    File src1;
    File dst;
    bool aam;
} MulInstr;

typedef struct {
    File src0;
    File src1;
    File src2;
    File dst;
    bool aam;
} MacInstr;

typedef struct {
    File src0;
    File src1;
    File src2;
    File dst;
    bool aam;
} MadInstr;

typedef struct {
    InstructionType type;
    union {
        JumpInstr jump;
        MovInstr mov;
        FillInstr fill;
        AddInstr add;
        MulInstr mul;
        MacInstr mac;
        MadInstr mad;
    };
} Instruction;






typedef struct {
    Instruction kernel[32];
    int blocks;
} Microkernel;




#endif