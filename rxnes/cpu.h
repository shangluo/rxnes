//cpu.h

#ifndef _CPU_H
#define _CPU_H

#include "types.h"

//cpu memory
extern u8 memory[0x10000];

void cpu_init( void );
u32  cpu_execute_translate( u32 n_cycles );
void cpu_test( void );

//cpu memory management
void cpu_mm_write( u16 addr, u8 *buf, u16 len );
void cpu_mm_read( u16 addr, u8 *buf, u16 len );

//set one byte to memory
#define cpu_mm_set( addr, data )        \
	do                                  \
    {                                   \
        u8 _d = data;                   \
        cpu_mm_write( addr, &_d, 1 );   \
    } while (0)


//get one byte from memory
#define cpu_mm_get( addr, data )				\
	do                                          \
	{											\
		cpu_mm_read(addr, (u8 *)&data, 1);		\
	} while (0)

// interrupt
void cpu_set_nmi_pending();

//debugging
u8  cpu_disassemble_intruction(u16 addr, char *buf, int len);
u16 cpu_read_register_value(char *register_name);
#endif //_CPU_H