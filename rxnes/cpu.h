//cpu.h

#ifndef _CPU_H
#define _CPU_H

#include "types.h"

//cpu memory
extern u8 memory[0x10000];

//registers pack from 6052
typedef struct
{
    u8 A;
    u8 X;
    u8 Y;

    union
    {
        struct
        {
            u8 C:1;
            u8 Z:1;
            u8 I:1;
            u8 D:1;
            u8 B:1;
            u8 R:1;
            u8 V:1;
            u8 N:1;
        } SR;
        u8 FLAGS;
    };
    u8 SP;

    union
    {
        struct
        {
            u8 PCL;
            u8 PCH;
        };
        u16 PC;
    };
} registers;

extern registers regs;

//flags
#define CF regs.SR.C
#define ZF regs.SR.Z
#define IF regs.SR.I
#define DF regs.SR.D
#define BF regs.SR.B
#define VF regs.SR.V
#define NF regs.SR.N

void cpu_reset( void );
u32  cpu_execute_translate( s32 n_cycles );
void cpu_test( void );

//stack operation
void push( u8 data );
u8   pop();

//cpu memory management
void cpu_mm_write( u16 addr, u8 *buf, u16 len );
void cpu_mm_read( u16 addr, u8 *buf, u16 len );

#endif //_CPU_H