//File:        cpu.c
//Description: Simple A203 Cpu Emulator
//Data:        2013.5.11

#include "cpu.h"
#include "ppu.h"
#include "papu.h"
#include "log.h"
#include "input.h"
#include "ines.h"
#include "global.h"
#include "mapper.h"
#include <string.h>

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
			u8 C : 1;
			u8 Z : 1;
			u8 I : 1;
			u8 D : 1;
			u8 B : 1;
			u8 R : 1;
			u8 V : 1;
			u8 N : 1;
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
} cpu_registers;

int irq_pending;

//registers
static cpu_registers regs;

//flags
#define CF regs.SR.C
#define ZF regs.SR.Z
#define IF regs.SR.I
#define DF regs.SR.D
#define BF regs.SR.B
#define RF regs.SR.R
#define VF regs.SR.V
#define NF regs.SR.N

//memroy
u8 memory[0x10000];

//
extern u8 mmc3_irq_enabled;
extern u8 mmc3_irq_counter;

//
extern ines_rom *c_rom;

//ppu position
u32 cpu_cycles;
u32 cpu_cycles_count;

//
static u8 cpu_nmi_pending;

static void cpu_handle_io( u16 reg );
static void cpu_handle_dma( void );
static void cpu_handle_irq(void);
static void cpu_handle_nmi( void );
//stack operation
static void cpu_stack_push(u8 data);
static u8   cpu_stack_pop();


u8 opcode_cycles[] = {
    0x7, 0x6, 0x0, 0x8, 0x3, // 0
    0x3, 0x5, 0x5, 0x3, 0x2, // 5
    0x2, 0x2, 0x4, 0x4, 0x6, // 10
    0x6, 0x3, 0x5, 0x0, 0x8, // 15
    0x4, 0x4, 0x6, 0x6, 0x2, // 20
    0x4, 0x2, 0x7, 0x4, 0x4, // 25 
    0x7, 0x7, 0x6, 0x6, 0x0, // 30
    0x8, 0x3, 0x3, 0x5, 0x5, // 35
    0x4, 0x2, 0x2, 0x2, 0x4, // 40
    0x4, 0x6, 0x6, 0x2, 0x5, // 45
    0x0, 0x8, 0x4, 0x4, 0x6, // 50
    0x6, 0x2, 0x4, 0x2, 0x7, // 55
    0x4, 0x4, 0x7, 0x7, 0x6, // 60
    0x6, 0x0, 0x8, 0x3, 0x3, // 65
    0x5, 0x5, 0x3, 0x2, 0x2, // 70
    0x2, 0x3, 0x4, 0x6, 0x6, // 75
    0x3, 0x5, 0x0, 0x8, 0x4, // 80
    0x4, 0x6, 0x6, 0x2, 0x4, // 85
    0x2, 0x7, 0x4, 0x4, 0x7, // 90
    0x7, 0x6, 0x6, 0x0, 0x8,  // 95
    0x3, 0x3, 0x5, 0x5, 0x4, // 100
    0x2, 0x2, 0x2, 0x5, 0x4, // 105
    0x6, 0x6, 0x2, 0x5, 0x0, // 110
    0x8, 0x4, 0x4, 0x6, 0x6, // 115
    0x2, 0x4, 0x2, 0x7, 0x4,  // 120
    0x4, 0x7, 0x7, 0x2, 0x6, // 125 
    0x2, 0x6, 0x3, 0x3, 0x3, //130
    0x3, 0x2, 0x2, 0x2, 0x2, //135
    0x4, 0x4, 0x4, 0x4, 0x3, //140
    0x6, 0x0, 0x6, 0x4, 0x4, //145
    0x4, 0x4, 0x2, 0x5, 0x2, // 150
    0x5, 0x5, 0x5, 0x5, 0x5, // 155
    0x2, 0x6, 0x2, 0x6, 0x3, // 160
    0x3, 0x3, 0x3, 0x2, 0x2, // 165
    0x2, 0x2, 0x4, 0x4, 0x4, //170
    0x4, 0x2, 0x5, 0x0, 0x5, // 175 
    0x4, 0x4, 0x4, 0x4, 0x2, // 180
    0x4, 0x2, 0x4, 0x4, 0x4, // 185
    0x4, 0x4, 0x2, 0x6, 0x2, // 190
    0x8, 0x3, 0x3, 0x5, 0x5, // 195
    0x2, 0x2, 0x2, 0x2, 0x4, // 200
    0x4, 0x6, 0x6, 0x3, 0x5, // 205
    0x0, 0x8, 0x4, 0x4, 0x6, // 210
    0x6, 0x2, 0x4, 0x2, 0x7, // 215
    0x4, 0x4, 0x7, 0x7, 0x2, // 220
    0x6, 0x2, 0x8, 0x3, 0x3, // 225
    0x5, 0x5, 0x2, 0x2, 0x2, // 230
    0x2, 0x4, 0x4, 0x6, 0x6, // 235
	0x2, 0x5, 0x0, 0x8, 0x4, // 240
	0x4, 0x6, 0x6, 0x2, 0x4, // 245
	0x2, 0x7, 0x4, 0x4, 0x7, // 250
	0x7						 // 255
};//0x100

#define setflag( flag, val )                                                    \
		do																		\
        {                                                                       \
            if ( val )                                                          \
                flag = 1;                                                       \
            else                                                                \
                flag = 0;                                                       \
        } while (0)

//immediately
#define getimm8( op, bytes )                                                    \
        do																		\
		{                                                                       \
            cpu_mm_get( regs.PC + 1, op );                                      \
            bytes = 2;                                                          \
        } while (0)

//zero page with X or Y
//NOTICE!!:Zero Page MUST BE WRAP AROUND!!
#define getzeroXY( op, addr, bytes, XF, YF )                                    \
        do																		\
		{                                                                       \
            u8 off;                                                             \
            cpu_mm_get( regs.PC + 1, off );                                     \
            if ( XF )                                                           \
                addr = ( off + regs.X ) % 0x100;                                \
            else if ( YF )                                                      \
                addr = ( off + regs.Y ) % 0x100;                                \
            else                                                                \
                addr = off;                                                     \
            cpu_mm_get( addr, op );												\
            bytes = 2;                                                          \
        } while(0)

#define getabsXYCross( op, addr, bytes, XF, YF, cross )                             \
		do																			\
        {                                                                           \
			cpu_mm_read( regs.PC + 1, (u8 *)&addr, sizeof ( addr ) );               \
			if ( XF )                                                               \
			    addr += regs.X;                                                     \
			else if ( YF )                                                          \
			    addr += regs.Y;														\
			if (cross)																\
			{																		\
				if ((addr & 0xff) == 0xff)											\
				{																	\
					cpu_cycles += 1;												\
				}																	\
			}																		\
			cpu_mm_get( addr, op );                                                 \
			bytes = 3;                                                              \
        }while (0)

//aboslute with X or Y
#define getabsXY( op, addr, bytes, XF, YF )                                         \
		getabsXYCross(op, addr, bytes, XF, YF, 0)

#define getindirectXYCross( op, addr, bytes, XF, YF, cross )				\
	do																		\
	{																		\
																			\
		u8 off;                                                             \
		cpu_mm_get(regs.PC + 1, off);                                       \
		if (XF)                                                             \
		{                                                                   \
			off = (off + regs.X) % 0x100;                                   \
			cpu_mm_read(off, (u8 *)&addr, sizeof(addr));                    \
		}                                                                   \
		else if (YF)                                                        \
		{                                                                   \
			cpu_mm_read(off, (u8 *)&addr, sizeof(addr));                    \
			addr = addr + regs.Y;                                           \
		}                                                                   \
		if (cross)															\
		{																	\
			if ((addr & 0xff) == 0xff)										\
			{																\
				cpu_cycles += 1;											\
			}																\
		}																	\
		cpu_mm_get(addr, op);												\
		bytes = 2;															\
	} while(0)

#define getindirectXY( op, addr, bytes, XF, YF )                                \
		getindirectXYCross(op, addr, bytes, XF, YF, 0)

//instructions
static u8 ADC(u8 opcode)
{
    u8 bytes = 0;
    u16 result = 0;
    u16 address = 0;
    u8 carry = CF;
    u8 operand = 0;
	u8 a = regs.A;
    switch( opcode )
    {
    case 0x69:
        getimm8( operand, bytes );
        break;

    case 0x65:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0x75:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0x6d:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0x7d:
        getabsXYCross( operand, address, bytes , 1, 0, 1);
        break;

    case 0x79:
		getabsXYCross( operand, address, bytes , 0, 1, 1);
        break;

    case 0x61:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0x71:
        //post
        getindirectXYCross ( operand, address, bytes, 0, 1, 1);
        break;

    default:
        break;
    }

    result = regs.A + operand + carry;
    regs.A = result & 0xff;

    setflag( CF, result & 0x100 );
    setflag( ZF, regs.A == 0 );
    setflag( NF, regs.A & 0x80 );
    setflag( VF, (result ^ a) & ( result ^ operand ) & 0x80);

    return bytes;
}

static u8 AND(u8 opcode)
{
    u8 bytes = 0;
    u16 address = 0;
    u8 operand = 0;
    switch( opcode )
    {
    case 0x29:
        getimm8( operand, bytes );
        break;

    case 0x25:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0x35:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0x2d:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0x3d:
        getabsXYCross( operand, address, bytes , 1, 0, 1);
        break;

    case 0x39:
        getabsXYCross( operand, address, bytes , 0, 1, 1);
        break;

    case 0x21:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0x31:
        //post
        getindirectXYCross ( operand, address, bytes, 0, 1, 1);
        break;

    default:
        break;
    }

    regs.A &= operand;
    setflag( ZF, regs.A == 0 );
    setflag( NF, regs.A & 0x80 );

    return bytes;
}

static u8 ASL(u8 opcode)
{
    u8 bytes = 0;
    u8 operand = 0;
    u16 address = 0;
    switch ( opcode )
    {
    case 0x0a:
        CF = ( regs.A >> 7 ) & 0x1;

        regs.A <<= 1;
        setflag( ZF, regs.A == 0 );
        setflag( NF, regs.A & 0x80 );

        bytes = 1;
        return bytes;

    case 0x06:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0x16:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0x0e:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0x1e:
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    default :
        break;
    }

    CF = ( operand >> 7 ) & 0x1;
    operand <<= 1;
    setflag( ZF, operand == 0 );
    setflag( NF, operand & 0x80 );

    cpu_mm_set( address, operand );
    return bytes;
}

static u8 BCC()
{
    s8 offset;
    if ( CF == 0 )
    {
        //get
        cpu_mm_get( regs.PC + 1, offset );
        regs.PC = regs.PC + offset + 2;
        return 0;
    }
    else
    {
        return 2;
    }
}

static u8 BCS()
{
    s8 offset;
    if ( CF == 1 )
    {
        //get
        cpu_mm_get( regs.PC + 1, offset );
        regs.PC = regs.PC + offset + 2;
        return 0;
    }
    else
    {
        return 2;
    }
}
static u8 BEQ()
{
    s8 offset;
    if ( ZF == 1 )
    {
        //get
        cpu_mm_get( regs.PC + 1, offset );
        regs.PC = regs.PC + offset + 2;
        return 0;
    }
    else
    {
        return 2;
    }
}

static u8 BIT(u8 opcode)
{
    u8 bytes = 0;
    u16 address = 0;
    u8 operand = 0;

    switch( opcode )
    {
    case 0x24:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0x2c:
        getabsXY( operand, address, bytes , 0, 0 );
        break;
    }

    setflag( ZF, !( operand & regs.A ) );
    setflag( VF, operand & 0x40 );
    setflag( NF, operand & 0x80 );

    return bytes;
}

static u8 BMI()
{
    s8 offset;
    if ( NF == 1 )
    {
        //get
        cpu_mm_get( regs.PC + 1, offset );
        regs.PC = regs.PC + offset + 2;
        return 0;
    }
    else
    {
        return 2;
    }
}

static u8 BNE()
{
    s8 offset;
    if ( ZF == 0 )
    {
        //get
        cpu_mm_get( regs.PC + 1, offset );
        regs.PC = regs.PC + offset + 2;
        return 0;
    }
    else
    {
        return 2;
    }
}

static u8 BPL()
{
    s8 offset = 0;
    if ( NF == 0 )
    {
        //get
        cpu_mm_get( regs.PC + 1, offset );
        regs.PC = regs.PC + offset + 2;
        return 0;
    }
    else
    {
        return 2;
    }
}

static u8 BRK()
{
    //push sr
    cpu_stack_push( regs.PCH );
    cpu_stack_push( regs.PCL );

    cpu_stack_push( regs.FLAGS );

    setflag(IF, 1);
	setflag(BF, 1);
	setflag(RF, 1);

    //jmp
    cpu_mm_read( 0xfffe, (u8 *)&regs.PC, sizeof ( regs.PC ) );
    return 2;
}

static u8 BVC()
{
    s8 offset = 0;

    if ( VF == 0 )
    {
        //get
        cpu_mm_get( regs.PC + 1, offset );
        regs.PC = regs.PC + offset + 2;
        return 0;
    }
    else
    {
        return 2;
    }
}

static u8 BVS()
{
    s8 offset = 0;

    //get
    cpu_mm_get( regs.PC + 1, offset );

    if ( VF == 1 )
    {
        regs.PC = regs.PC + offset + 2;
        return 0;
    }
    else
    {
        return 2;
    }
}

static u8 CLC()
{
    CF = 0;
    return 1;
}

static u8 CLD()
{
    DF = 0;
    return 1;
}

static u8 CLI()
{
    IF = 0;
    return 1;
}

static u8 CLV()
{
    VF = 0;
    return 1;
}
static u8 CMP( u8 opcode )
{
    u8 bytes = 0;
    u8 result = 0;
    u16 address = 0;
    u8 operand = 0;
    switch( opcode )
    {
    case 0xc9:
        getimm8( operand, bytes );
        break;

    case 0xc5:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0xd5:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0xcd:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0xdd:
        getabsXYCross( operand, address, bytes , 1, 0, 1);
        bytes = 3;
        break;

    case 0xd9:
        getabsXYCross( operand, address, bytes , 0, 1, 1);
        bytes = 3;
        break;

    case 0xc1:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0xd1:
        //post
        getindirectXYCross ( operand, address, bytes, 0, 1, 1 );
        break;

    default:
        break;
    }

    result = regs.A - operand;
    setflag( CF, regs.A >= operand );
    setflag( ZF, regs.A == operand );
    setflag( NF, result & 0x80 );

    return bytes;
}

static u8 CPX( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 result;
    u8 operand = 0;

    switch ( opcode )
    {
    case 0xe0:
        getimm8( operand, bytes );
        break;

    case 0xe4:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0xec:
        getabsXY( operand, address, bytes , 0, 0 );
        break;
    }

    result = regs.X - operand;

    setflag( CF, regs.X >= operand );
    setflag( ZF, regs.X == operand );
    setflag( NF, result & 0x80 );

    return bytes;
}

static u8 CPY( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 result;
    u8 operand = 0;

    switch ( opcode )
    {
    case 0xc0:
        getimm8( operand, bytes );
        break;

    case 0xc4:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0xcc:
        getabsXY( operand, address, bytes , 0, 0 );
        break;
    }

    result = regs.Y - operand;
    setflag( CF, regs.Y >= operand );
    setflag( ZF, regs.Y == operand );
    setflag( NF, result & 0x80 );

    return bytes;
}

static u8 DEC( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 operand = 0;

    switch ( opcode )
    {
    case 0xc6:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0xd6:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0xce:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0xde:
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    default:
        break;
    }

    operand -= 1;
    setflag( ZF, operand == 0 );
    setflag( NF, operand & 0x80 );
    //write back
    cpu_mm_set( address, operand );
    return bytes;
}

static u8 DEX()
{
    regs.X -= 1;

    setflag( ZF, regs.X == 0 );
    setflag( NF, regs.X & 0x80 );

    return 1;
}

static u8 DEY()
{
    regs.Y -= 1;

    setflag( ZF, regs.Y == 0 );
    setflag( NF, regs.Y & 0x80 );

    return 1;
}

static u8 EOR( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 operand = 0;
    switch( opcode )
    {
    case 0x49:
        getimm8( operand, bytes );
        break;

    case 0x45:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0x55:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0x4d:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0x5d:
        getabsXYCross( operand, address, bytes , 1, 0, 1);
        break;

    case 0x59:
        getabsXYCross( operand, address, bytes , 0, 1, 1);
        break;

    case 0x41:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0x51:
        //post
        getindirectXYCross ( operand, address, bytes, 0, 1, 1);
        break;

    default:
        break;
    }

    regs.A ^= operand;

    setflag ( ZF, regs.A == 0 );
    setflag ( NF, regs.A & 0x80 );

    return bytes;
}

static u8 INC( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 operand = 0;

    switch ( opcode )
    {
    case 0xe6:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0xf6:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0xee:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0xfe:
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    default:
        break;
    }

    operand += 1;
    setflag( ZF, operand == 0 );
    setflag( NF, operand & 0x80 );
    //write back
    cpu_mm_set( address, operand );
    return bytes;
}

static u8 INX()
{
    regs.X += 1;

    setflag( ZF, regs.X == 0 );
    setflag( NF, regs.X & 0x80 );

    return 1;
}

static u8 INY()
{
    regs.Y += 1;

    setflag( ZF, regs.Y == 0 );
    setflag( NF, regs.Y & 0x80 );

    return 1;
}

static u8 JMP( u8 opcode )
{
	u8 addr_low, addr_high;
    u16 address = 0;
    u16 old_pc = regs.PC;

    if ( opcode == 0x4c )
    {
        cpu_mm_read( regs.PC + 1, (u8 *)&regs.PC, sizeof ( regs.PC ) );
        LOG_TRACE( "JMP", "Excute JMP from 0x%x, to 0x%x", old_pc, regs.PC );
        return 0;
    }

    if ( opcode == 0x6c )
    {
        cpu_mm_read( regs.PC + 1, (u8 *)&address, sizeof ( address ) );

		// jmp shouldn't across page boundary
		if ((address & 0xff) == 0xff)
		{
			cpu_mm_get(address, addr_low);
			cpu_mm_get(address & 0xff00, addr_high);
			regs.PC = addr_low | (addr_high << 8);
		}
		else
		{
			cpu_mm_read( address, (u8 *)&regs.PC, sizeof ( regs.PC ) );
		}
        LOG_TRACE( "JMP[Table]", "Excute JMP from 0x%x, to 0x%x", old_pc, regs.PC );
        return 0;
    }

    return 0;
}

static u8 JSR()
{
    u16 old_pc = regs.PC + 3 - 1;

    //yes it's quite wrong!!
    //push( regs.PCH );
    //push( regs.PCL + 3 - 1 );
    cpu_stack_push( ( old_pc >> 8 ) & 0xff );
    cpu_stack_push( old_pc & 0xff );

//  LOG_TRACE( "JSR", "CURRENT SP IS %X", regs.SP );


    //address = memory[regs.PC + 1] | ( memory[regs.PC + 2] << 8 );
    //regs.PC = address;
    cpu_mm_read( regs.PC + 1, (u8 *)&regs.PC, sizeof ( regs.PC ) );
    LOG_TRACE( "JSR", "Excute JSR from 0x%x, to 0x%x", old_pc, regs.PC );

    return 0;
}

static u8 LDA( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 operand = 0;
    switch( opcode )
    {
    case 0xa9:
        getimm8( operand, bytes );
        break;

    case 0xa5:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0xb5:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0xad:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0xbd:
        getabsXYCross( operand, address, bytes , 1, 0, 1 );
        break;

    case 0xb9:
        getabsXYCross( operand, address, bytes , 0, 1, 1 );
        break;

    case 0xa1:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0xb1:
        //post
        getindirectXYCross ( operand, address, bytes, 0, 1, 1 );
        break;

    default:
        break;
    }

    regs.A = operand;
    setflag( ZF, regs.A == 0 );
    setflag( NF, regs.A & 0x80 );

    return bytes;
}

static u8 LDX( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 operand = 0;
    switch( opcode )
    {
    case 0xa2:
        getimm8( operand, bytes );
        break;

    case 0xa6:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0xb6:
        getzeroXY( operand, address, bytes , 0, 1 );
        break;

    case 0xae:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0xbe:
        getabsXYCross( operand, address, bytes , 0, 1, 1 );
        break;

    default:
        break;
    }

    regs.X = operand;
    setflag( ZF, regs.X == 0 );
    setflag( NF, regs.X & 0x80 );

    return bytes;

}

static u8 LDY( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 operand = 0;
    switch( opcode )
    {
    case 0xa0:
        getimm8( operand, bytes );
        break;

    case 0xa4:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0xb4:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0xac:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0xbc:
        getabsXYCross( operand, address, bytes , 1, 0, 1 );
        break;

    default:
        break;
    }

    regs.Y = operand;
    setflag( ZF, regs.Y == 0 );
    setflag( NF, regs.Y & 0x80 );

    return bytes;
}

static u8 LSR( u8 opcode )
{
    u8 bytes = 0;
    u8 operand = 0;
    u16 address = 0;
    switch ( opcode )
    {
    case 0x4a:
        CF = regs.A & 0x1;
        regs.A >>= 1;
        setflag( ZF, regs.A == 0 );
        setflag( NF, regs.A & 0x80 );

        bytes = 1;
        return bytes;

    case 0x46:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0x56:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0x4e:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0x5e:
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    default :
        break;
    }

    CF = operand & 0x1;

    operand >>= 1;

    setflag( ZF, operand == 0 );
    setflag( NF, operand & 0x80 );

    cpu_mm_set( address, operand );
    return bytes;
}

static u8 NOP()
{
    return 1;
}

static u8 ORA( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 operand = 0;
    switch( opcode )
    {
    case 0x09:
        getimm8( operand, bytes );
        break;

    case 0x05:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0x15:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0x0d:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0x1d:
        getabsXYCross( operand, address, bytes , 1, 0, 1 );
        break;

    case 0x19:
        getabsXYCross( operand, address, bytes , 0, 1, 1 );
        break;

    case 0x01:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0x11:
        //post
        getindirectXYCross ( operand, address, bytes, 0, 1, 1);
        break;

    default:
        break;
    }

    regs.A |= operand;

    setflag( ZF, regs.A == 0 );
    setflag( NF, regs.A & 0x80 );


    return bytes;
}

static u8 PHA()
{
    cpu_stack_push( regs.A );
    LOG_TRACE( "PHA", "Excute PHA at 0x%x, push 0x%x to stack, SP = 0x%x", regs.PC, regs.A,regs.SP );
    return 1;
}

static u8 PHP()
{
	setflag(BF, 1);
	setflag(RF, 1);
	cpu_stack_push( regs.FLAGS );
	return 1;
}

static u8 PLA()
{
    regs.A = cpu_stack_pop();

    setflag( ZF, regs.A == 0 );
    setflag( NF, regs.A & 0x80 );

    LOG_TRACE( "PLA", "Excute PLA at 0x%x, pop 0x%x from stack, SP = 0x%x", regs.PC, regs.A, regs.SP );
    return 1;
}
static u8 PLP()
{
	setflag(IF, 0);
    regs.FLAGS = cpu_stack_pop();
    return 1;
}

static u8 ROL( u8 opcode )
{
    u8 bytes = 0;
    u8 operand = 0;
    u16 address = 0;
    u8 carry = CF;
    switch ( opcode )
    {
    case 0x2a:
        setflag( CF, regs.A & 0x80 );
        regs.A <<= 1;
        regs.A |= carry;

        setflag( ZF, regs.A == 0 );
        setflag( NF, regs.A & 0x80 );

        bytes = 1;
        return bytes;

    case 0x26:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0x36:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0x2e:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0x3e:
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    default :
        break;
    }

    setflag( CF, operand & 0x80 );
    operand <<= 1;
    operand |= carry;

    setflag( ZF, operand == 0 );
    setflag( NF, operand & 0x80 );
    cpu_mm_set( address, operand );
    return bytes;
}

static u8 ROR( u8 opcode )
{
    u8 bytes = 0;
    u8 operand = 0;
    u16 address = 0;
    u8 carry = CF;
    switch ( opcode )
    {
    case 0x6a:
        setflag( CF, regs.A & 0x1 );

        regs.A >>= 1;
        regs.A |= ( carry << 7 );

        setflag( ZF, regs.A == 0 );
        setflag( NF, regs.A & 0x80 );


        bytes = 1;
        return bytes;

    case 0x66:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0x76:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0x6e:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0x7e:
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    default :
        break;
    }

    setflag( CF, operand & 0x1 );

    operand >>= 1;
    operand |= ( carry << 7 );

    setflag( ZF, operand == 0 );
    setflag( NF, operand & 0x80 );

    cpu_mm_set( address, operand );
    return bytes;

}

static u8 RTI()
{
    LOG_TRACE( "RTI", "RTI occur at 0x%x", regs.PC );
    //popup sr
    regs.FLAGS = cpu_stack_pop();

    //popup pc
    regs.PCL = cpu_stack_pop();
    regs.PCH = cpu_stack_pop();

    return 0;
}

static u8 RTS()
{
    u16 old_pc = regs.PC;
    regs.PCL = cpu_stack_pop();
    regs.PCH = cpu_stack_pop();
    LOG_TRACE( "RTS", "Excute RTS from 0x%x, to 0x%x", old_pc, regs.PC );
//  LOG_TRACE( "RTS", "CURRENT SP IS %X", regs.SP );
    return 1;
}

static u8 SBC( u8 opcode )
{
    u8 bytes = 0;
    u16 result = 0;
    u16 address = 0;
    u8 carry = CF;
    u8 operand = 0;
	u8 a = regs.A;
    switch( opcode )
    {
    case 0xe9:
        getimm8( operand, bytes );
        break;

    case 0xe5:
        getzeroXY( operand, address, bytes , 0, 0 );
        break;

    case 0xf5:
        getzeroXY( operand, address, bytes , 1, 0 );
        break;

    case 0xed:
        getabsXY( operand, address, bytes , 0, 0 );
        break;

    case 0xfd:
        getabsXYCross( operand, address, bytes , 1, 0, 1 );
        break;

    case 0xf9:
        getabsXYCross( operand, address, bytes , 0, 1, 1 );
        break;

    case 0xe1:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0xf1:
        //post
        getindirectXYCross ( operand, address, bytes, 0, 1, 1 );
        break;

    default:
        break;
    }

    result = regs.A - operand - ( 1 - carry );
    regs.A = result & 0xff;
    setflag( CF, !(result & 0x100) );
    setflag( ZF, regs.A == 0 );
    setflag( NF, regs.A & 0x80 );
    setflag( VF, (a ^ operand) & ( a^ result) & 0x80 );

    return bytes;

}
static u8 SEC()
{
    CF = 1;
    return 1;
}
static u8 SED()
{
    DF = 1;
    return 1;
}

static u8 SEI()
{
    IF = 1;
    return 1;
}

static u8 STA( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 off = 0;

    switch( opcode )
    {
        //FIXEDME!:!!DON'T FORGET WRAP AROUND IN ZERO PAGE
        //zero
    case 0x85:
        cpu_mm_get( regs.PC + 1, off );
        address = off;
        bytes = 2;
        break;

        //zeroX
    case 0x95:
        cpu_mm_get( regs.PC + 1, off );
        address = ( off + regs.X ) % 0x100;
        bytes = 2;
        break;

        //abs
    case 0x8d:
        cpu_mm_read( regs.PC + 1, (u8 *)&address, 2 );
        bytes = 3;
        break;

    case 0x9d:
        cpu_mm_read( regs.PC + 1, (u8 *)&address, 2 );
        address += regs.X;
        bytes = 3;
        break;

    case 0x99:
        cpu_mm_read( regs.PC + 1, (u8 *)&address, 2 );
        address += regs.Y;
        bytes = 3;
        break;

        //indirectX
    case 0x81:
        cpu_mm_get( regs.PC + 1, off );
        address = ( off + regs.X ) % 0x100;
        cpu_mm_read( address, (u8 *)&address, 2 );
        bytes = 2;
        break;

    case 0x91:
        //post
        cpu_mm_get( regs.PC + 1, off );
        cpu_mm_read( off, (u8 *)&address, 2 );
        address += regs.Y;
        bytes = 2;
        break;

    default:
        break;
    }

    cpu_mm_set( address, regs.A );
    return bytes;
}

static u8 STX( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 off;
    switch( opcode )
    {
    case 0x86:
        cpu_mm_get( regs.PC + 1, off );
        address = off;
        bytes = 2;
        break;

    case 0x96:
        cpu_mm_get( regs.PC + 1, off );
        address = ( off + regs.Y ) % 0x100 ;
        bytes = 2;
        break;

    case 0x8e:
        cpu_mm_read( regs.PC + 1, (u8 *)&address, 2 );
        bytes = 3;
        break;

    default:
        break;
    }

    cpu_mm_set( address, regs.X );
    return bytes;
}

static u8 STY( u8 opcode )
{
    u8 bytes = 0;
    u16 address = 0;
    u8 off = 0;
    switch( opcode )
    {
    case 0x84:
        cpu_mm_get( regs.PC + 1, off );
        address = off;
        bytes = 2;
        break;

    case 0x94:
        cpu_mm_get( regs.PC + 1, off );
        address =  ( off + regs.X ) % 0x100;
        bytes = 2;
        break;

    case 0x8c:
        cpu_mm_read( regs.PC + 1, (u8 *)&address, 2 );
        bytes = 3;
        break;

    default:
        break;
    }

    cpu_mm_set( address, regs.Y );

    return bytes;
}

static u8 TAX()
{
    regs.X = regs.A;

    setflag( ZF, regs.X == 0 );
    setflag( NF, regs.X & 0x80 );

    return 1;
}

static u8 TAY()
{
    regs.Y = regs.A;
    setflag( ZF, regs.Y == 0 );
    setflag( NF, regs.Y & 0x80 );

    return 1;
}
static u8 TSX()
{
    regs.X = regs.SP;

    setflag( ZF, regs.X == 0 );
    setflag( NF, regs.X & 0x80 );

    return 1;
}
static u8 TXA()
{
    regs.A = regs.X;
    setflag( ZF, regs.A == 0 );
    setflag( NF, regs.A & 0x80 );
    return 1;
}
static u8 TXS()
{
    regs.SP = regs.X;
    return 1;
}

static u8 TYA()
{
    regs.A = regs.Y;

    setflag( ZF, regs.Y == 0 );
    setflag( NF, regs.Y & 0x80 );

    return 1;
}


void cpu_init( void )
{
    u8 buf = 0;
    //sp decreace
    regs.SP -= 3;

    //disable interrupt
    IF = 1;

    //apu mute
    cpu_mm_set( 0x4017, buf );

    //regs.PC = memory[0xfffc] | ( memory[0xfffd] << 8 );
    cpu_mm_read( 0xfffc, (u8 *)&regs.PC, sizeof ( regs.PC ));
}

u32  cpu_execute_translate( u32 n_cycles )
{
    u8 opcode;
    u8 bytes = 0;
    cpu_cycles = 0;

    while(  cpu_cycles < n_cycles )
    {
        //fetch one opcode
        cpu_mm_get( regs.PC, opcode );
//      LOG_TRACE( "CPU", "PC AT %X", regs.PC );

        switch( opcode )
        {
        case 0x00:
            bytes = BRK();
            break;

        case 0x01:
        case 0x05:
        case 0x09:
        case 0x0d:
        case 0x11:
        case 0x15:
        case 0x19:
        case 0x1d:
            bytes = ORA(opcode);
            break;

        case 0x06:
        case 0x0a:
        case 0x0e:
        case 0x16:
        case 0x1e:
            bytes = ASL(opcode);
            break;

        case 0x08:
            bytes = PHP();
            break;

        case 0x10:
            bytes = BPL();
            break;

        case 0x18:
            bytes = CLC();
            break;

        case 0x20:
            bytes = JSR();
            break;

        case 0x21:
        case 0x25:
        case 0x29:
        case 0x2d:
        case 0x31:
        case 0x35:
        case 0x39:
        case 0x3d:
            bytes = AND(opcode);
            break;

        case 0x24:
        case 0x2c:
            bytes = BIT(opcode);
            break;

        case 0x26:
        case 0x2a:
        case 0x2e:
        case 0x36:
        case 0x3e:
            bytes = ROL(opcode);
            break;

        case 0x28:
            bytes = PLP();
            break;

        case 0x30:
            bytes = BMI();
            break;

        case 0x38:
            bytes = SEC();
            break;

        case 0x40:
            bytes = RTI();
            break;

        case 0x41:
        case 0x45:
        case 0x49:
        case 0x4d:
        case 0x51:
        case 0x55:
        case 0x59:
        case 0x5d:
            bytes = EOR(opcode);
            break;

        case 0x46:
        case 0x4a:
        case 0x4e:
        case 0x56:
        case 0x5e:
            bytes = LSR(opcode);
            break;

        case 0x48:
            bytes = PHA();
            break;

        case 0x4c:
        case 0x6c:
            bytes = JMP(opcode);
            break;

        case 0x50:
            bytes = BVC();
            break;

        case 0x58:
            bytes = CLI();
            break;

        case 0x60:
            bytes = RTS();
            break;

        case 0x61:
        case 0x65:
        case 0x69:
        case 0x6d:
        case 0x71:
        case 0x75:
        case 0x79:
        case 0x7d:
            bytes = ADC(opcode);
            break;

        case 0x66:
        case 0x6a:
        case 0x6e:
        case 0x76:
        case 0x7e:
            bytes = ROR(opcode);
            break;

        case 0x68:
            bytes = PLA();
            break;

        case 0x70:
            bytes = BVS();
            break;


        case 0x78:
            bytes = SEI();
            break;

        case 0x81:
        case 0x85:
        case 0x8d:
        case 0x91:
        case 0x95:
        case 0x99:
        case 0x9d:
            bytes = STA(opcode);
            break;

        case 0x84:
        case 0x94:
        case 0x8c:
            bytes = STY(opcode);
            break;

        case 0x86:
        case 0x96:
        case 0x8e:
            bytes = STX(opcode);
            break;


        case 0x88:
            bytes = DEY();
            break;

        case 0x8a:
            bytes = TXA();
            break;

        case 0x90:
            bytes = BCC();
            break;


        case 0x98:
            bytes = TYA();
            break;


        case 0x9a:
            bytes = TXS();
            break;

        case 0xa0:
        case 0xa4:
        case 0xac:
        case 0xb4:
        case 0xbc:
            bytes = LDY(opcode);
            break;

        case 0xa1:
        case 0xa5:
        case 0xa9:
        case 0xad:
        case 0xb1:
        case 0xb5:
        case 0xb9:
        case 0xbd:
            bytes = LDA(opcode);
            break;

        case 0xa2:
        case 0xa6:
        case 0xae:
        case 0xb6:
        case 0xbe:
            bytes = LDX(opcode);
            break;


        case 0xa8:
            bytes = TAY();
            break;


        case 0xaa:
            bytes = TAX();
            break;


        case 0xb0:
            bytes = BCS();
            break;


        case 0xb8:
            bytes = CLV();
            break;


        case 0xba:
            bytes = TSX();
            break;

        case 0xc0:
        case 0xc4:
        case 0xcc:
            bytes = CPY(opcode);
            break;

        case 0xc1:
        case 0xc5:
        case 0xc9:
        case 0xcd:
        case 0xd1:
        case 0xd5:
        case 0xd9:
        case 0xdd:
            bytes = CMP(opcode);
            break;

        case 0xc6:
        case 0xce:
        case 0xd6:
        case 0xde:
            bytes = DEC(opcode);
            break;


        case 0xc8:
            bytes = INY();
            break;


        case 0xca:
            bytes = DEX();
            break;


        case 0xd0:
            bytes = BNE();
            break;


        case 0xd8:
            bytes = CLD();
            break;

        case 0xe0:
        case 0xe4:
        case 0xec:
            bytes = CPX(opcode);
            break;

        case 0xe1:
        case 0xe5:
        case 0xe9:
        case 0xed:
        case 0xf1:
        case 0xf5:
        case 0xf9:
        case 0xfd:
            bytes = SBC(opcode);
            break;

        case 0xe6:
        case 0xee:
        case 0xf6:
        case 0xfe:
            bytes = INC(opcode);
            break;


        case 0xe8:
            bytes = INX();
            break;


        case 0xea:
            bytes = NOP();
            break;


        case 0xf0:
            bytes = BEQ();
            break;


        case 0xf8:
            bytes = SED();
            break;

        default:
            LOG_TRACE( "CPU", "Bad opcode %x at %x", opcode, regs.PC );
            //ignore?
            bytes = 1;
            break;
        }

        cpu_cycles += opcode_cycles[ opcode ];
		cpu_cycles_count += opcode_cycles[opcode];

        //update PC
        regs.PC += bytes;

		//check irq
		cpu_handle_irq();

        //check nmi
        cpu_handle_nmi();

        //button
        input_check_state();
    }

    return cpu_cycles;
}

void cpu_stack_push( u8 data )
{
//  cpu_mm_write( data, 0x100 + regs.SP );
    cpu_mm_set( 0x100 + regs.SP, data );
    regs.SP = regs.SP - 1;
//  LOG_TRACE( "PUSH", "%X", data );
}

u8 cpu_stack_pop()
{
    u8 bytes = 0;
    regs.SP += 1;
    cpu_mm_get( regs.SP + 0x100, bytes );

//  LOG_TRACE( "POP", "%X", bytes );
    return bytes;
}

void cpu_mm_write( u16 addr, u8 *buf, u16 len )
{
    int i;
    int n_addr;

	if (addr < 0x100)
	{
		for (i = 0; i < len; ++i)
		{
			memory[addr] = buf[i];
			addr = (addr + 1) % 0x100;
		}

		return;
	}

    if ( addr >= 0x8000 &&  c_rom->mapper != 0x0 )
    {
		mapper_write(addr, *buf);
        //not really write on rom
        return;
    }

    //memory from $0000~$07ff mirrored 3 times from $0800~$1fff
    if ( addr < 0x2000 )
    {
        n_addr = addr;
        for ( i = 0; i < 4; ++i )
        {
            memcpy( memory + n_addr, buf, len );
            n_addr = ( n_addr + 0x800 ) % 0x2000;
        }
    }

    //ppu register mirror
    //else if ( addr >= 0x2000 && addr <= 0x2007 )
    //{
    //  for ( i = addr; i < 0x4000; i += 8 )
    //  {
    //      memcpy( memory + i, buf, len );
    //  }
    //}

    else
    {
        memcpy( memory + addr, buf, len );
    }

    cpu_handle_io( addr );
}

void cpu_handle_io( u16 addr )
{
    //
    u8 mem;

	// papu
	if (addr >= 0x4000 && addr < 0x4013
		|| addr == 0x4015
		|| addr == 0x4017)
	{
		papu_register_write(addr, memory[addr]);
	}

	if (addr >= PPU_CTRL_REG1 && addr <= PPU_DATA)
	{
		ppu_register_write(addr, memory[addr]);
	}

	switch (addr)
	{
    case SPR_DMA:
        cpu_handle_dma();
        break;

    case JOYPAD_1:
        mem = memory[JOYPAD_1];

        if ( mem == 1 )
            input_reset();
        else
            input_set_strobe( 1 ) ;

        break;

    case JOYPAD_2:
        mem = memory[JOYPAD_2];
        break;

    default:
        break;
    }
}

void cpu_mm_read( u16 addr, u8 *buf, u16 len )
{
	u8 i;

	// zero page
	if (addr < 0x100)
	{
		for (i = 0; i < len; ++i)
		{
			buf[i] = memory[addr];
			addr = (addr + 1) % 0x100;
		}

		return;
	}
	else if (addr == PPU_DATA || addr == PPU_SPR_DATA || addr == PPU_STATUS)
	{
		ppu_register_read(addr, buf);
		return;
	}
	else if (addr == JOYPAD_1)
    {
        //joypad1
        memory[addr] = input_get_next_state();
//      return;
    }

	// papu
	if (addr == 0x4015 )
	{
		memory[addr] = papu_register_read(addr);
	}


    memcpy( buf, memory + addr, len );
}

void cpu_handle_nmi( void )
{
    // if nmi enable
    if ( memory[PPU_STATUS] & 0x80 && memory[PPU_CTRL_REG1] & 0x80 && cpu_nmi_pending)
    {
        LOG_TRACE( "NMI", "NMI occur at %x", regs.PC );

        //push pc
        cpu_stack_push( regs.PCH );
        cpu_stack_push( regs.PCL );

        //push sr
        cpu_stack_push( regs.FLAGS );

		setflag(IF, 1);

        //jmp to vector table
        //regs.PCL = memory[0xfffa];
        //regs.PCH = memory[0xfffb];
        cpu_mm_read( 0xfffa, (u8 *)&regs.PC, sizeof ( regs.PC ) );

        //clear flag
		cpu_nmi_pending = 0;

        cpu_cycles += 7;
    }
}

void cpu_set_nmi_pending()
{
	cpu_nmi_pending = 1;
}

void cpu_handle_irq(void)
{
	// if nmi enable
	if (!regs.SR.I && mmc3_irq_enabled && !mmc3_irq_counter)
	{
		LOG_TRACE("IRQ", "IRQ occur at %x", regs.PC);
		//push pc
		cpu_stack_push(regs.PCH);
		cpu_stack_push(regs.PCL);

		//push sr
		cpu_stack_push(regs.FLAGS);

		setflag(IF, 1);

		cpu_mm_read(0xfffe, (u8 *)&regs.PC, sizeof(regs.PC));

		cpu_cycles += 7;
	}
}


void cpu_handle_dma( void )
{
	u16 addr = 0;
	u8 buf[0x100];
	cpu_mm_get(SPR_DMA, addr);
	addr = 0x100 * addr;
	cpu_mm_read(addr, buf, 0x100);

	ppu_oam_dma(buf);

    cpu_cycles += 513;
    LOG_TRACE( "DMA", "Transfer DMA at %x", regs.PC );
}


void cpu_test( void )
{
    u8 d;
    cpu_mm_set(0x0, 1);
    cpu_mm_set(0x1, 0xff);
    cpu_mm_set(0x3, 0xff);
    cpu_mm_get(0x0, d);
    cpu_mm_get(0x2, d);
    cpu_mm_get(0x3, d);
}

typedef enum 
{
	Accumulator,
	Implied,
	Relative,
	Immediate,
	Zero_Page,   
	Zero_Page_X,
	Zero_Page_Y,
	Absolute,
	Absolute_X, 
	Absolute_Y,
	Indirect,
	Indirect_X,
	Indirect_Y
}CPU_ADDRESSING_MODE;

static void cpu_disassemble_format_operand(CPU_ADDRESSING_MODE mode, u16 operand,char *buf, int len)
{
	switch (mode)
	{
	case Implied:
		break;
	case Accumulator:
		sprintf(buf, "%s", "A");
		break;
	case Immediate:
		sprintf(buf, "#$%02X", operand);
		break;
	case Relative:
		sprintf(buf, "$%02X", operand);
		break;
	case Zero_Page:
		sprintf(buf, "$%02X", operand);
		break;
	case Zero_Page_X:
		sprintf(buf, "$%02X, X", operand);
		break;
	case Zero_Page_Y:
		sprintf(buf, "$%02X, Y", operand);
		break;
	case Absolute:
		sprintf(buf, "$%04X", operand);
		break;
	case Absolute_X:
		sprintf(buf, "$%04X, X", operand);
		break;
	case Absolute_Y:
		sprintf(buf, "$%04X, Y", operand);
		break;
	case Indirect:
		sprintf(buf, "($%04X)", operand);
		break;
	case Indirect_X:
		sprintf(buf, "($%02X, X)", operand);
		break;
	case Indirect_Y:
		sprintf(buf, "($%04X, Y)", operand);
		break;
	default:
		break;
	}
}

static u8 cpu_disassemble_intruction_internal(u16 addr, char *instruction, CPU_ADDRESSING_MODE mode, char *buf, int len)
{
	u8 bytes = 1;
	u16 operand = 0;
	char operand_buf[20];
	memset(operand_buf, 0, sizeof(operand_buf));
	switch (mode)
	{
	case Implied:
	case Accumulator:
		bytes = 1;
		break;
	case Immediate:
	case Relative:
	case Zero_Page:
	case Zero_Page_X:
	case Zero_Page_Y:
	case Indirect_X:
	case Indirect_Y:
		operand = memory[addr + 1];
		bytes = 2;
		break;
	case Indirect:
	case Absolute:
	case Absolute_X:
	case Absolute_Y:
		memcpy(&operand, memory + addr + 1, 2);
		bytes = 3;
		break;
	default:
		break;
	}
	
	cpu_disassemble_format_operand(mode, operand, operand_buf, 20);
	if (operand_buf[0])
		sprintf(buf, "%s   %s", instruction, operand_buf);
	else
		strcpy(buf, instruction);
	return bytes;
}

u8 cpu_disassemble_intruction(u16 addr, char *buf, int len)
{
	u8 opcode;
	u8 bytes = 0;

	memset(buf, 0, len);

	//fetch one opcode
	opcode = memory[addr];

	switch (opcode)
	{
	case 0x00:
		bytes = cpu_disassemble_intruction_internal(addr, "BRK", Implied, buf, len);
		break;

	case 0x01:
		bytes = cpu_disassemble_intruction_internal(addr, "ORA", Indirect_X, buf, len);
		break;
	case 0x05:
		bytes = cpu_disassemble_intruction_internal(addr, "ORA", Zero_Page, buf, len);
		break;
	case 0x09:
		bytes = cpu_disassemble_intruction_internal(addr, "ORA", Immediate, buf, len);
		break;
	case 0x0d:
		bytes = cpu_disassemble_intruction_internal(addr, "ORA", Absolute, buf, len);
		break;
	case 0x11:
		bytes = cpu_disassemble_intruction_internal(addr, "ORA", Indirect_Y, buf, len);
		break;
	case 0x15:
		bytes = cpu_disassemble_intruction_internal(addr, "ORA", Zero_Page_X, buf, len);
		break;
	case 0x19:
		bytes = cpu_disassemble_intruction_internal(addr, "ORA", Absolute_Y, buf, len);
		break;
	case 0x1d:
		bytes = cpu_disassemble_intruction_internal(addr, "ORA", Absolute_X, buf, len);
		break;

	case 0x06:
		bytes = cpu_disassemble_intruction_internal(addr, "ASL", Zero_Page, buf, len);
		break;
	case 0x0a:
		bytes = cpu_disassemble_intruction_internal(addr, "ASL", Accumulator, buf, len);
		break;
	case 0x0e:
		bytes = cpu_disassemble_intruction_internal(addr, "ASL", Absolute, buf, len);
		break;
	case 0x16:
		bytes = cpu_disassemble_intruction_internal(addr, "ASL", Zero_Page_X, buf, len);
		break;
	case 0x1e:
		bytes = cpu_disassemble_intruction_internal(addr, "ASL", Absolute_X, buf, len);
		break;

	case 0x08:
		bytes = cpu_disassemble_intruction_internal(addr, "PHP", Implied, buf, len);
		break;

	case 0x10:
		bytes = cpu_disassemble_intruction_internal(addr, "BPL", Relative, buf, len);
		break;

	case 0x18:
		bytes = cpu_disassemble_intruction_internal(addr, "CLC", Implied, buf, len);
		break;

	case 0x20:
		bytes = cpu_disassemble_intruction_internal(addr, "JSR", Absolute, buf, len);
		break;

	case 0x21:
		bytes = cpu_disassemble_intruction_internal(addr, "AND", Indirect_X, buf, len);
		break;
	case 0x25:
		bytes = cpu_disassemble_intruction_internal(addr, "AND", Zero_Page, buf, len);
		break;
	case 0x29:
		bytes = cpu_disassemble_intruction_internal(addr, "AND", Immediate, buf, len);
		break;
	case 0x2d:
		bytes = cpu_disassemble_intruction_internal(addr, "AND", Absolute, buf, len);
		break;
	case 0x31:
		bytes = cpu_disassemble_intruction_internal(addr, "AND", Indirect_Y, buf, len);
		break;
	case 0x35:
		bytes = cpu_disassemble_intruction_internal(addr, "AND", Zero_Page_X, buf, len);
		break;
	case 0x39:
		bytes = cpu_disassemble_intruction_internal(addr, "AND", Absolute_Y, buf, len);
		break;
	case 0x3d:
		bytes = cpu_disassemble_intruction_internal(addr, "AND", Absolute_X, buf, len);
		break;

	case 0x24:
		bytes = cpu_disassemble_intruction_internal(addr, "BIT", Zero_Page, buf, len);
		break;
	case 0x2c:
		bytes = cpu_disassemble_intruction_internal(addr, "BIT", Absolute, buf, len);
		break;

	case 0x26:
		bytes = cpu_disassemble_intruction_internal(addr, "ROL", Zero_Page, buf, len);
		break;
	case 0x2a:
		bytes = cpu_disassemble_intruction_internal(addr, "ROL", Accumulator, buf, len);
		break;
	case 0x2e:
		bytes = cpu_disassemble_intruction_internal(addr, "ROL", Absolute, buf, len);
		break;
	case 0x36:
		bytes = cpu_disassemble_intruction_internal(addr, "ROL", Zero_Page_X, buf, len);
		break;
	case 0x3e:
		bytes = cpu_disassemble_intruction_internal(addr, "ROL", Absolute_X, buf, len);
		break;

	case 0x28:
		bytes = cpu_disassemble_intruction_internal(addr, "PLP", Implied, buf, len);
		break;

	case 0x30:
		bytes = cpu_disassemble_intruction_internal(addr, "BMI", Relative, buf, len);
		break;

	case 0x38:
		bytes = cpu_disassemble_intruction_internal(addr, "SEC", Implied, buf, len);
		break;

	case 0x40:
		bytes = cpu_disassemble_intruction_internal(addr, "RTI", Implied, buf, len);
		break;

	case 0x41:
		bytes = cpu_disassemble_intruction_internal(addr, "EOR", Indirect_X, buf, len);
		break;
	case 0x45:
		bytes = cpu_disassemble_intruction_internal(addr, "EOR", Zero_Page, buf, len);
		break;
	case 0x49:
		bytes = cpu_disassemble_intruction_internal(addr, "EOR", Immediate, buf, len);
		break;
	case 0x4d:
		bytes = cpu_disassemble_intruction_internal(addr, "EOR", Absolute, buf, len);
		break;
	case 0x51:
		bytes = cpu_disassemble_intruction_internal(addr, "EOR", Indirect_Y, buf, len);
		break;
	case 0x55:
		bytes = cpu_disassemble_intruction_internal(addr, "EOR", Zero_Page_X, buf, len);
		break;
	case 0x59:
		bytes = cpu_disassemble_intruction_internal(addr, "EOR", Absolute_Y, buf, len);
		break;
	case 0x5d:
		bytes = cpu_disassemble_intruction_internal(addr, "EOR", Absolute_X, buf, len);
		break;


	case 0x46:
		bytes = cpu_disassemble_intruction_internal(addr, "LSR", Zero_Page, buf, len);
		break;
	case 0x4a:
		bytes = cpu_disassemble_intruction_internal(addr, "LSR", Accumulator, buf, len);
		break;
	case 0x4e:
		bytes = cpu_disassemble_intruction_internal(addr, "LSR", Absolute, buf, len);
		break;
	case 0x56:
		bytes = cpu_disassemble_intruction_internal(addr, "LSR", Zero_Page_X, buf, len);
		break;
	case 0x5e:
		bytes = cpu_disassemble_intruction_internal(addr, "LSR", Absolute_X, buf, len);
		break;

	case 0x48:
		bytes = cpu_disassemble_intruction_internal(addr, "PHA", Implied, buf, len);
		break;

	case 0x4c:
		bytes = cpu_disassemble_intruction_internal(addr, "JMP", Absolute, buf, len);
		break;
	case 0x6c:
		bytes = cpu_disassemble_intruction_internal(addr, "JMP", Indirect, buf, len);
		break;

	case 0x50:
		bytes = cpu_disassemble_intruction_internal(addr, "BVC", Relative, buf, len);
		break;

	case 0x58:
		bytes = cpu_disassemble_intruction_internal(addr, "CLI", Implied, buf, len);
		break;

	case 0x60:
		bytes = cpu_disassemble_intruction_internal(addr, "RTS", Implied, buf, len);
		break;

	case 0x61:
		bytes = cpu_disassemble_intruction_internal(addr, "ADC", Indirect_X, buf, len);
		break;
	case 0x65:
		bytes = cpu_disassemble_intruction_internal(addr, "ADC", Zero_Page, buf, len);
		break;
	case 0x69:
		bytes = cpu_disassemble_intruction_internal(addr, "ADC", Immediate, buf, len);
		break;
	case 0x6d:
		bytes = cpu_disassemble_intruction_internal(addr, "ADC", Absolute, buf, len);
		break;
	case 0x71:
		bytes = cpu_disassemble_intruction_internal(addr, "ADC", Indirect_Y, buf, len);
		break;
	case 0x75:
		bytes = cpu_disassemble_intruction_internal(addr, "ADC", Zero_Page_X, buf, len);
		break;
	case 0x79:
		bytes = cpu_disassemble_intruction_internal(addr, "ADC", Absolute_Y, buf, len);
		break;
	case 0x7d:
		bytes = cpu_disassemble_intruction_internal(addr, "ADC", Absolute_X, buf, len);
		break;

	case 0x66:
		bytes = cpu_disassemble_intruction_internal(addr, "ROR", Zero_Page, buf, len);
		break;
	case 0x6a:
		bytes = cpu_disassemble_intruction_internal(addr, "ROR", Accumulator, buf, len);
		break;
	case 0x6e:
		bytes = cpu_disassemble_intruction_internal(addr, "ROR", Absolute, buf, len);
		break;
	case 0x76:
		bytes = cpu_disassemble_intruction_internal(addr, "ROR", Zero_Page_X, buf, len);
		break;
	case 0x7e:
		bytes = cpu_disassemble_intruction_internal(addr, "ROR", Absolute_X, buf, len);
		break;

	case 0x68:
		bytes = cpu_disassemble_intruction_internal(addr, "PLA", Implied, buf, len);
		break;

	case 0x70:
		bytes = cpu_disassemble_intruction_internal(addr, "BVS", Relative, buf, len);
		break;


	case 0x78:
		bytes = cpu_disassemble_intruction_internal(addr, "SEI", Implied, buf, len);
		break;

	case 0x81:
		bytes = cpu_disassemble_intruction_internal(addr, "STA", Indirect_X, buf, len);
		break;
	case 0x85:
		bytes = cpu_disassemble_intruction_internal(addr, "STA", Zero_Page, buf, len);
		break;
	case 0x8d:
		bytes = cpu_disassemble_intruction_internal(addr, "STA", Absolute, buf, len);
		break;
	case 0x91:
		bytes = cpu_disassemble_intruction_internal(addr, "STA", Indirect_Y, buf, len);
		break;
	case 0x95:
		bytes = cpu_disassemble_intruction_internal(addr, "STA", Zero_Page_X, buf, len);
		break;
	case 0x99:
		bytes = cpu_disassemble_intruction_internal(addr, "STA", Absolute_Y, buf, len);
		break;
	case 0x9d:
		bytes = cpu_disassemble_intruction_internal(addr, "STA", Absolute_X, buf, len);
		break;


	case 0x84:
		bytes = cpu_disassemble_intruction_internal(addr, "STY", Zero_Page, buf, len);
		break;
	case 0x94:
		bytes = cpu_disassemble_intruction_internal(addr, "STY", Zero_Page_X, buf, len);
		break;
	case 0x8c:
		bytes = cpu_disassemble_intruction_internal(addr, "STY", Absolute, buf, len);
		break;

	case 0x86:
		bytes = cpu_disassemble_intruction_internal(addr, "STX", Zero_Page, buf, len);
		break;
	case 0x96:
		bytes = cpu_disassemble_intruction_internal(addr, "STX", Zero_Page_Y, buf, len);
		break;
	case 0x8e:
		bytes = cpu_disassemble_intruction_internal(addr, "STX", Absolute, buf, len);
		break;


	case 0x88:
		bytes = cpu_disassemble_intruction_internal(addr, "DEY", Implied, buf, len);
		break;

	case 0x8a:
		bytes = cpu_disassemble_intruction_internal(addr, "TXA", Implied, buf, len);
		break;

	case 0x90:
		bytes = cpu_disassemble_intruction_internal(addr, "BCC", Relative, buf, len);
		break;


	case 0x98:
		bytes = cpu_disassemble_intruction_internal(addr, "TYA", Implied, buf, len);
		break;


	case 0x9a:
		bytes = cpu_disassemble_intruction_internal(addr, "TXS", Implied, buf, len);
		break;

	case 0xa0:
		bytes = cpu_disassemble_intruction_internal(addr, "LDY", Immediate, buf, len);
		break;
	case 0xa4:
		bytes = cpu_disassemble_intruction_internal(addr, "LDY", Zero_Page, buf, len);
		break;
	case 0xac:
		bytes = cpu_disassemble_intruction_internal(addr, "LDY", Absolute, buf, len);
		break;
	case 0xb4:
		bytes = cpu_disassemble_intruction_internal(addr, "LDY", Zero_Page_X, buf, len);
		break;
	case 0xbc:
		bytes = cpu_disassemble_intruction_internal(addr, "LDY", Absolute_X, buf, len);
		break;

	case 0xa1:
		bytes = cpu_disassemble_intruction_internal(addr, "LDA", Indirect_X, buf, len);
		break;
	case 0xa5:
		bytes = cpu_disassemble_intruction_internal(addr, "LDA", Zero_Page, buf, len);
		break;
	case 0xa9:
		bytes = cpu_disassemble_intruction_internal(addr, "LDA", Immediate, buf, len);
		break;
	case 0xad:
		bytes = cpu_disassemble_intruction_internal(addr, "LDA", Absolute, buf, len);
		break;
	case 0xb1:
		bytes = cpu_disassemble_intruction_internal(addr, "LDA", Indirect_Y, buf, len);
		break;
	case 0xb5:
		bytes = cpu_disassemble_intruction_internal(addr, "LDA", Zero_Page_X, buf, len);
		break;
	case 0xb9:
		bytes = cpu_disassemble_intruction_internal(addr, "LDA", Absolute_Y, buf, len);
		break;
	case 0xbd:
		bytes = cpu_disassemble_intruction_internal(addr, "LDA", Absolute_X, buf, len);
		break;

	case 0xa2:
		bytes = cpu_disassemble_intruction_internal(addr, "LDX", Immediate, buf, len);
		break;
	case 0xa6:
		bytes = cpu_disassemble_intruction_internal(addr, "LDX", Zero_Page, buf, len);
		break;
	case 0xae:
		bytes = cpu_disassemble_intruction_internal(addr, "LDX", Absolute, buf, len);
		break;
	case 0xb6:
		bytes = cpu_disassemble_intruction_internal(addr, "LDX", Zero_Page_Y, buf, len);
		break;
	case 0xbe:
		bytes = cpu_disassemble_intruction_internal(addr, "LDX", Absolute_Y, buf, len);
		break;


	case 0xa8:
		bytes = cpu_disassemble_intruction_internal(addr, "TAY", Implied, buf, len);
		break;


	case 0xaa:
		bytes = cpu_disassemble_intruction_internal(addr, "TAX", Implied, buf, len);
		break;


	case 0xb0:
		bytes = cpu_disassemble_intruction_internal(addr, "BCS", Relative, buf, len);
		break;


	case 0xb8:
		bytes = cpu_disassemble_intruction_internal(addr, "CLV", Implied, buf, len);
		break;


	case 0xba:
		bytes = cpu_disassemble_intruction_internal(addr, "TSX", Implied, buf, len);
		break;

	case 0xc0:
		bytes = cpu_disassemble_intruction_internal(addr, "CPY", Immediate, buf, len);
		break;
	case 0xc4:
		bytes = cpu_disassemble_intruction_internal(addr, "CPY", Zero_Page, buf, len);
		break;
	case 0xcc:
		bytes = cpu_disassemble_intruction_internal(addr, "CPY", Absolute, buf, len);
		break;

	case 0xc1:
		bytes = cpu_disassemble_intruction_internal(addr, "CMP", Indirect_X, buf, len);
		break;
	case 0xc5:
		bytes = cpu_disassemble_intruction_internal(addr, "CMP", Zero_Page, buf, len);
		break;
	case 0xc9:
		bytes = cpu_disassemble_intruction_internal(addr, "CMP", Immediate, buf, len);
		break;
	case 0xcd:
		bytes = cpu_disassemble_intruction_internal(addr, "CMP", Absolute, buf, len);
		break;
	case 0xd1:
		bytes = cpu_disassemble_intruction_internal(addr, "CMP", Indirect_Y, buf, len);
		break;
	case 0xd5:
		bytes = cpu_disassemble_intruction_internal(addr, "CMP", Zero_Page_X, buf, len);
		break;
	case 0xd9:
		bytes = cpu_disassemble_intruction_internal(addr, "CMP", Absolute_Y, buf, len);
		break;
	case 0xdd:
		bytes = cpu_disassemble_intruction_internal(addr, "CMP", Absolute_X, buf, len);
		break;

	case 0xc6:
		bytes = cpu_disassemble_intruction_internal(addr, "DEC", Zero_Page, buf, len);
		break;
	case 0xce:
		bytes = cpu_disassemble_intruction_internal(addr, "DEC", Absolute, buf, len);
		break;
	case 0xd6:
		bytes = cpu_disassemble_intruction_internal(addr, "DEC", Zero_Page_X, buf, len);
		break;
	case 0xde:
		bytes = cpu_disassemble_intruction_internal(addr, "DEC", Absolute_X, buf, len);
		break;


	case 0xc8:
		bytes = cpu_disassemble_intruction_internal(addr, "INY", Implied, buf, len);
		break;


	case 0xca:
		bytes = cpu_disassemble_intruction_internal(addr, "DEX", Implied, buf, len);
		break;


	case 0xd0:
		bytes = cpu_disassemble_intruction_internal(addr, "BNE", Relative, buf, len);
		break;


	case 0xd8:
		bytes = cpu_disassemble_intruction_internal(addr, "CLD", Implied, buf, len);
		break;

	case 0xe0:
		bytes = cpu_disassemble_intruction_internal(addr, "CPX", Immediate, buf, len);
		break;
	case 0xe4:
		bytes = cpu_disassemble_intruction_internal(addr, "CPX", Zero_Page, buf, len);
		break;
	case 0xec:
		bytes = cpu_disassemble_intruction_internal(addr, "CPX", Absolute, buf, len);
		break;

	case 0xe1:
		bytes = cpu_disassemble_intruction_internal(addr, "SBC", Indirect_X, buf, len);
		break;
	case 0xe5:
		bytes = cpu_disassemble_intruction_internal(addr, "SBC", Zero_Page, buf, len);
		break;
	case 0xe9:
		bytes = cpu_disassemble_intruction_internal(addr, "SBC", Immediate, buf, len);
		break;
	case 0xed:
		bytes = cpu_disassemble_intruction_internal(addr, "SBC", Absolute, buf, len);
		break;
	case 0xf1:
		bytes = cpu_disassemble_intruction_internal(addr, "SBC", Indirect_Y, buf, len);
		break;
	case 0xf5:
		bytes = cpu_disassemble_intruction_internal(addr, "SBC", Zero_Page_X, buf, len);
		break;
	case 0xf9:
		bytes = cpu_disassemble_intruction_internal(addr, "SBC", Absolute_Y, buf, len);
		break;
	case 0xfd:
		bytes = cpu_disassemble_intruction_internal(addr, "SBC", Absolute_X, buf, len);
		break;

	case 0xe6:
		bytes = cpu_disassemble_intruction_internal(addr, "INC", Zero_Page, buf, len);
		break;
	case 0xee:
		bytes = cpu_disassemble_intruction_internal(addr, "INC", Absolute, buf, len);
		break;
	case 0xf6:
		bytes = cpu_disassemble_intruction_internal(addr, "INC", Zero_Page_X, buf, len);
		break;
	case 0xfe:
		bytes = cpu_disassemble_intruction_internal(addr, "INC", Absolute_X, buf, len);
		break;


	case 0xe8:
		bytes = cpu_disassemble_intruction_internal(addr, "INX", Implied, buf, len);
		break;


	case 0xea:
		bytes = cpu_disassemble_intruction_internal(addr, "NOP", Implied, buf, len);
		break;


	case 0xf0:
		bytes = cpu_disassemble_intruction_internal(addr, "BEQ", Relative, buf, len);
		break;


	case 0xf8:
		bytes = cpu_disassemble_intruction_internal(addr, "SED", Implied, buf, len);
		break;

	default:
		bytes = 1;
		break;
	}

	return bytes;
}

u16 cpu_read_register_value(char *register_name)
{
#define BEGIN_REG_SWITCH(val, param, reg)   \
		if (strcmp(param, #reg) == 0)		\
		{									\
			val = regs.##reg;				\
		}
#define REG_SWITCH(val, param, reg)			\
		else BEGIN_REG_SWITCH(val, param, reg)

#define END_REG_SWITCH()

	u16 value;
	BEGIN_REG_SWITCH(value, register_name, A)
		REG_SWITCH(value, register_name, X)
		REG_SWITCH(value, register_name, Y)
		REG_SWITCH(value, register_name, SP)
		REG_SWITCH(value, register_name, PC)
		REG_SWITCH(value, register_name, FLAGS)
		REG_SWITCH(value, register_name, SR.C)
		REG_SWITCH(value, register_name, SR.Z)
		REG_SWITCH(value, register_name, SR.I)
		REG_SWITCH(value, register_name, SR.D)
		REG_SWITCH(value, register_name, SR.B)
		REG_SWITCH(value, register_name, SR.R)
		REG_SWITCH(value, register_name, SR.V)
		REG_SWITCH(value, register_name, SR.N)
	END_REG_SWITCH()

	return value;
}