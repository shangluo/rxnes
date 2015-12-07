//File:        cpu.c
//Description: Simple A203 Cpu Emulator
//Data:        2013.5.11

#include "cpu.h"
#include "ppu.h"
#include "log.h"
#include "input.h"
#include "ines.h"
#include <string.h>

//registers
registers regs;

//memroy
u8 memory[0x10000];

//sprite attribute memory
extern u8 oam[0x100];

//
extern ines_rom *c_rom;

//ppu address for writing and reading
static u16 ppu_addr;
static u8  b_tongle;
static u8  first_time;

//spr address
static u8 spr_addr;

//mirror type
extern u8 mirror;

//ppu position
extern u8 cam_x, cam_y, fine_x;
u32 cpu_cycles;

//mapper function
void mapper1_handler(u16 addr, u8 data);
void mapper2_handler(u16 addr, u8 data);
void mapper3_handler(u16 addr, u8 data);

static void cpu_handle_io( u16 reg );
static void cpu_handle_dma( void );
static void cpu_handle_nmi( void );

u8 opcode_cycles[] = {
    0x7, 0x6, 0x0, 0x8, 0x3,
    0x3, 0x5, 0x5, 0x3, 0x2,
    0x2, 0x2, 0x4, 0x4, 0x6,
    0x6, 0x3, 0x5, 0x0, 0x8,
    0x4, 0x4, 0x6, 0x6, 0x2,
    0x4, 0x2, 0x7, 0x4, 0x4,
    0x7, 0x7, 0x6, 0x6, 0x0,
    0x8, 0x3, 0x3, 0x5, 0x5,
    0x4, 0x2, 0x2, 0x2, 0x4,
    0x4, 0x6, 0x6, 0x2, 0x5,
    0x0, 0x8, 0x4, 0x4, 0x6,
    0x6, 0x2, 0x4, 0x2, 0x7,
    0x4, 0x4, 0x7, 0x7, 0x6,
    0x6, 0x0, 0x8, 0x3, 0x3,
    0x5, 0x5, 0x3, 0x2, 0x2,
    0x2, 0x3, 0x4, 0x6, 0x6,
    0x3, 0x5, 0x0, 0x8, 0x4,
    0x4, 0x6, 0x6, 0x2, 0x4,
    0x2, 0x7, 0x4, 0x4, 0x7,
    0x7, 0x6, 0x6, 0x0, 0x8,
    0x3, 0x3, 0x5, 0x5, 0x4,
    0x2, 0x2, 0x2, 0x5, 0x4,
    0x6, 0x6, 0x2, 0x5, 0x0,
    0x8, 0x4, 0x4, 0x6, 0x6,
    0x2, 0x4, 0x2, 0x7, 0x4,
    0x4, 0x7, 0x7, 0x2, 0x6,
    0x2, 0x6, 0x3, 0x3, 0x3,
    0x3, 0x2, 0x2, 0x2, 0x2,
    0x4, 0x4, 0x4, 0x4, 0x3,
    0x6, 0x0, 0x6, 0x4, 0x4,
    0x4, 0x4, 0x2, 0x5, 0x2,
    0x5, 0x5, 0x5, 0x5, 0x5,
    0x2, 0x6, 0x2, 0x6, 0x3,
    0x3, 0x3, 0x3, 0x2, 0x2,
    0x2, 0x2, 0x4, 0x4, 0x4,
    0x4, 0x2, 0x5, 0x0, 0x5,
    0x4, 0x4, 0x4, 0x4, 0x2,
    0x4, 0x2, 0x4, 0x4, 0x4,
    0x4, 0x4, 0x2, 0x6, 0x2,
    0x8, 0x3, 0x3, 0x5, 0x5,
    0x2, 0x2, 0x2, 0x2, 0x4,
    0x4, 0x6, 0x6, 0x3, 0x5,
    0x0, 0x8, 0x4, 0x4, 0x6,
    0x6, 0x2, 0x4, 0x2, 0x7,
    0x4, 0x4, 0x7, 0x7, 0x2,
    0x6, 0x2, 0x8, 0x3, 0x3,
    0x5, 0x5, 0x2, 0x2, 0x2,
    0x2, 0x4, 0x4, 0x6, 0x6,
    0x2, 0x5, 0x0, 0x8, 0x4,
    0x4, 0x6, 0x6, 0x2, 0x4,
    0x2, 0x7, 0x4, 0x4, 0x7,
    0x7
};//0x100

#define setflag( flag, val )                                                    \
        {                                                                       \
            if ( val )                                                          \
                flag = 1;                                                       \
            else                                                                \
                flag = 0;                                                       \
        }

//immediately
#define getimm8( op, bytes )                                                    \
        {                                                                       \
            mm_get( regs.PC + 1, op );                                          \
            bytes = 2;                                                          \
        }

//zero page with X or Y
//NOTICE!!:Zero Page MUST BE WRAP AROUND!!
#define getzeroXY( op, addr, bytes, XF, YF )                                    \
        {                                                                       \
            u8 off;                                                             \
            mm_get( regs.PC + 1, off );                                         \
            if ( XF )                                                           \
                addr = ( off + regs.X ) % 0x100;                                \
            else if ( YF )                                                      \
                addr = ( off + regs.Y ) % 0x100;                                \
            else                                                                \
                addr = off;                                                     \
            mm_get( addr, op );                                                 \
            bytes = 2;                                                          \
        }

//aboslute with X or Y
#define getabsXY( op, addr, bytes, XF, YF )                                         \
        {                                                                           \
        cpu_mm_read( regs.PC + 1, (u8 *)&addr, sizeof ( addr ) );                   \
        if ( XF )                                                                   \
            addr += regs.X;                                                         \
        else if ( YF )                                                              \
            addr += regs.Y;                                                         \
        mm_get( addr, op );                                                         \
        bytes = 3;                                                                  \
        }

#define getindirectXY( op, addr, bytes, XF, YF )                                \
        {                                                                       \
            u8 off;                                                             \
            mm_get( regs.PC + 1, off );                                         \
            if ( XF )                                                           \
            {                                                                   \
                off = ( off + regs.X ) % 0x100;                                 \
                cpu_mm_read( off, (u8 *)&addr, sizeof( addr ));                 \
            }                                                                   \
            else if ( YF )                                                      \
            {                                                                   \
                cpu_mm_read( off, (u8 *)&addr, sizeof( addr ));                 \
                addr = addr + regs.Y;                                           \
            }                                                                   \
            mm_get( addr, op );                                                 \
            bytes = 2;                                                          \
        }

//set one byte to memory
#define mm_set( addr, data )                \
        {                                   \
            u8 _d = data;                   \
            cpu_mm_write( addr, &_d, 1 );   \
        }

//get one byte from memory
#define mm_get( addr, data ) cpu_mm_read( addr, (u8 *)&data, 1 )

static void reset_ppu_status( void )
{
    b_tongle = 0;
    memory[PPU_STATUS] &= 0x7f;
//  memory[PPU_ADDRESS] = 0x0;
//  memory[PPU_SCROLL_REG] = 0x0;
}

static void update_ppu_address( void )
{
    u8 paddr;
    mm_get( PPU_ADDRESS, paddr );

    //set least bits
    if ( b_tongle == 0 )
    {
        //clear
        ppu_addr = 0;

        ppu_addr |= paddr << 8;

        //update b_tongle
        b_tongle = ( b_tongle + 1 ) % 2;
    }
    //set most bits
    else
    {
        ppu_addr |= paddr;
        //update b_tongle
        b_tongle = ( b_tongle + 1 ) % 2;

        //address initialized
        first_time = 1;

        //log
        LOG_TRACE( "CPU", "Set PPU_ADDRESS register to %x at %x", ppu_addr, regs.PC );
    }
}

static void update_spr_address( void )
{
    spr_addr = memory[PPU_SPR_ADDR];
}

static void write_oam_data( void )
{
    oam[spr_addr] = memory[PPU_SPR_DATA];
    ++spr_addr;
    ++memory[PPU_SPR_ADDR];
}

static void write_to_vram( void )
{
    u8 ctr1;

    ppu_mm_write( ppu_addr, memory[PPU_DATA] );

    //do increacement
    mm_get( PPU_CTRL_REG1, ctr1);
    if ( ctr1 & 0x4 )
    {
        memory[PPU_ADDRESS] += 32;
        ppu_addr += 32;
    }
    else
    {
        ++memory[PPU_ADDRESS];
        ++ppu_addr;
    }
}


static void read_from_oam( void )
{
    memory[PPU_SPR_DATA] = oam[spr_addr];
    ++spr_addr;
    ++memory[PPU_SPR_ADDR];
}

static void read_from_vram( void )
{
	u8 ctr1;

    //first time read
    //return a dummy value
    if ( first_time )
    {
        first_time = 0;
        return;
    }

    memory[PPU_DATA] = ppu_mm_get( ppu_addr );

	//do increacement
	mm_get(PPU_CTRL_REG1, ctr1);
	if (ctr1 & 0x4)
	{
		memory[PPU_ADDRESS] += 32;
		ppu_addr += 32;
	}
	else
	{
		++memory[PPU_ADDRESS];
		++ppu_addr;
	}
}


//instructions
static u8 ADC(u8 opcode)
{
    u8 bytes = 0;
    u16 result = 0;
    u16 address = 0;
    u8 carry = CF;
    u8 operand = 0;
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
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    case 0x79:
        getabsXY( operand, address, bytes , 0, 1 );
        break;

    case 0x61:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0x71:
        //post
        getindirectXY ( operand, address, bytes, 0, 1 );
        break;

    default:
        break;
    }

    result = regs.A + operand + carry;
    regs.A = result & 0xff;

    setflag( CF, result & 0x100 );
    setflag( ZF, regs.A == 0 );
    setflag( NF, regs.A & 0x80 );
    setflag( VF, ( regs.A & 0x80 ) != ( result & 0x80 ) );

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
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    case 0x39:
        getabsXY( operand, address, bytes , 0, 1 );
        break;

    case 0x21:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0x31:
        //post
        getindirectXY ( operand, address, bytes, 0, 1 );
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

    mm_set( address, operand );
    return bytes;
}

static u8 BCC()
{
    s8 offset;
    if ( CF == 0 )
    {
        //get
        mm_get( regs.PC + 1, offset );
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
        mm_get( regs.PC + 1, offset );
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
        mm_get( regs.PC + 1, offset );
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
        mm_get( regs.PC + 1, offset );
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
        mm_get( regs.PC + 1, offset );
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
        mm_get( regs.PC + 1, offset );
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
    push( regs.PCH );
    push( regs.PCL );

    push( regs.FLAGS );

    setflag( IF, 1 );
    setflag( BF, 1 );

    //jmp
    cpu_mm_read( 0xfffe, (u8 *)&regs.PC, sizeof ( regs.PC ) );
    return 0;
}

static u8 BVC()
{
    s8 offset = 0;

    if ( VF == 0 )
    {
        //get
        mm_get( regs.PC + 1, offset );
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
    mm_get( regs.PC + 1, offset );

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
        getabsXY( operand, address, bytes , 1, 0 );
        bytes = 3;
        break;

    case 0xd9:
        getabsXY( operand, address, bytes , 0, 1 );
        bytes = 3;
        break;

    case 0xc1:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0xd1:
        //post
        getindirectXY ( operand, address, bytes, 0, 1 );
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
    mm_set( address, operand );
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
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    case 0x59:
        getabsXY( operand, address, bytes , 0, 1 );
        break;

    case 0x41:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0x51:
        //post
        getindirectXY ( operand, address, bytes, 0, 1 );
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
    mm_set( address, operand );
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
        cpu_mm_read( address, (u8 *)&regs.PC, sizeof ( regs.PC ) );
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
    push( ( old_pc >> 8 ) & 0xff );
    push( old_pc & 0xff );

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
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    case 0xb9:
        getabsXY( operand, address, bytes , 0, 1 );
        break;

    case 0xa1:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0xb1:
        //post
        getindirectXY ( operand, address, bytes, 0, 1 );
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
        getabsXY( operand, address, bytes , 0, 1 );
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
        getabsXY( operand, address, bytes , 1, 0 );
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

    mm_set( address, operand );
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
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    case 0x19:
        getabsXY( operand, address, bytes , 0, 1 );
        break;

    case 0x01:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0x11:
        //post
        getindirectXY ( operand, address, bytes, 0, 1 );
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
    push( regs.A );
    LOG_TRACE( "PHA", "Excute PHA at 0x%x, push 0x%x to stack, SP = 0x%x", regs.PC, regs.A,regs.SP );
    return 1;
}

static u8 PHP()
{
    push( regs.FLAGS );

    return 1;
}

static u8 PLA()
{
    regs.A = pop();

    setflag( ZF, regs.A == 0 );
    setflag( NF, regs.A & 0x80 );

    LOG_TRACE( "PLA", "Excute PLA at 0x%x, pop 0x%x from stack, SP = 0x%x", regs.PC, regs.A, regs.SP );
    return 1;
}
static u8 PLP()
{
    regs.FLAGS = pop();
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
    mm_set( address, operand );
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

    mm_set( address, operand );
    return bytes;

}

static u8 RTI()
{

    LOG_TRACE( "RTI", "RTI occur at 0x%x", regs.PC );
    //popup sr
    regs.FLAGS = pop();

    //popup pc
    regs.PCL = pop();
    regs.PCH = pop();

    return 0;
}

static u8 RTS()
{
    u16 old_pc = regs.PC;
    regs.PCL = pop();
    regs.PCH = pop();
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
        getabsXY( operand, address, bytes , 1, 0 );
        break;

    case 0xf9:
        getabsXY( operand, address, bytes , 0, 1 );
        break;

    case 0xe1:
        getindirectXY ( operand, address, bytes, 1, 0 );
        break;

    case 0xf1:
        //post
        getindirectXY ( operand, address, bytes, 0, 1 );
        break;

    default:
        break;
    }

    result = regs.A - operand - ( 1 - carry );
    regs.A = result & 0xff;
    setflag( CF, result < 0x100 );
    setflag( ZF, regs.A == 0 );
    setflag( NF, regs.A & 0x80 );
    setflag( VF, regs.A & 0x80 || result >= 0x80 );

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
        mm_get( regs.PC + 1, off );
        address = off;
        bytes = 2;
        break;

        //zeroX
    case 0x95:
        mm_get( regs.PC + 1, off );
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
        mm_get( regs.PC + 1, off );
        address = ( off + regs.X ) % 0x100;
        cpu_mm_read( address, (u8 *)&address, 2 );
        bytes = 2;
        break;

    case 0x91:
        //post
        mm_get( regs.PC + 1, off );
        cpu_mm_read( off, (u8 *)&address, 2 );
        address += regs.Y;
        bytes = 2;
        break;

    default:
        break;
    }

    mm_set( address, regs.A );
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
        mm_get( regs.PC + 1, off );
        address = off;
        bytes = 2;
        break;

    case 0x96:
        mm_get( regs.PC + 1, off );
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

    mm_set( address, regs.X );
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
        mm_get( regs.PC + 1, off );
        address = off;
        bytes = 2;
        break;

    case 0x94:
        mm_get( regs.PC + 1, off );
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

    mm_set( address, regs.Y );

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


void cpu_reset( void )
{
    u8 buf = 0;
    //sp decreace
    regs.SP -= 3;

    //disable interrupt
    IF = 1;

    //apu mute
    mm_set( 0x4017, buf );

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
        mm_get( regs.PC, opcode );
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

        //update PC
        regs.PC += bytes;

        //check nmi
        cpu_handle_nmi();

        //button
        input_check_state();
    }

    return cpu_cycles;
}

void push( u8 data )
{
    regs.SP = regs.SP - 1;
//  cpu_mm_write( data, 0x100 + regs.SP );
    mm_set( 0x100 + regs.SP, data );
//  LOG_TRACE( "PUSH", "%X", data );
}

u8 pop()
{
    u8 bytes = 0;
    mm_get( regs.SP + 0x100, bytes );
    regs.SP += 1;

//  LOG_TRACE( "POP", "%X", bytes );
    return bytes;
}

void cpu_mm_write( u16 addr, u8 *buf, u16 len )
{
    int i;
    int n_addr;


    if ( addr >= 0x8000 &&  c_rom->mapper != 0x0 )
    {
        switch( c_rom->mapper )
        {
        case 0x1:
            mapper1_handler( addr, *buf );
			break;
		case 0x2:
			mapper2_handler(addr, *buf);
			break;
		case 0x3:
			mapper3_handler(addr, *buf);
			break;
		default:
            break;
        }

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

void cpu_handle_io( u16 reg )
{
    //
    u8 mem;

    switch ( reg )
    {
    case PPU_CTRL_REG1:
        break;

    case PPU_CTRL_REG2:
        break;

    case PPU_STATUS:
        break;

    case PPU_ADDRESS:
        update_ppu_address();
        break;

    case PPU_DATA:
        write_to_vram();
        break;

    case PPU_SPR_ADDR:
        update_spr_address();
        break;

    case PPU_SPR_DATA:
        write_oam_data();
        break;

    case PPU_SCROLL_REG:
        if ( b_tongle )
        {
            cam_y = memory[PPU_SCROLL_REG];

        } else
        {
            cam_x = memory[PPU_SCROLL_REG];
//          fine_x = ( fine_x + cam_x ) % 256;
            fine_x = cam_x;
        }
        b_tongle = ( b_tongle + 1 ) % 2;
        break;

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
    if ( addr == PPU_DATA )
    {
        read_from_vram();
    }
    else if ( addr == PPU_SPR_DATA )
    {
        read_from_vram();
    }
    else if ( addr == PPU_STATUS )
    {
        memcpy( buf, memory + addr, 1 );
        reset_ppu_status();

        return;
    }
    else if ( addr == JOYPAD_1 )
    {
        //joypad1

        memory[addr] = input_get_next_state();
//      return;
    }

    memcpy( buf, memory + addr, len );
}

void cpu_handle_nmi( void )
{
    // if nmi enable
    if ( memory[PPU_STATUS] & 0x80 && memory[PPU_CTRL_REG1] & 0x80 )
    {
        LOG_TRACE( "NMI", "NMI occur at %x", regs.PC );
        //push pc
        push( regs.PCH );
        push( regs.PCL );

        //push sr
        push( regs.FLAGS );

        //jmp to vector table
        //regs.PCL = memory[0xfffa];
        //regs.PCH = memory[0xfffb];
        cpu_mm_read( 0xfffa, (u8 *)&regs.PC, sizeof ( regs.PC ) );

        //clear flag
        memory[PPU_STATUS] &= 0x7f;

        cpu_cycles += 7;
    }
}

void cpu_handle_dma( void )
{
    u16 addr = 0;
    u8 buf[0x100];
    mm_get( SPR_DMA, addr );
    addr = 0x100 * addr;
    cpu_mm_read( addr, buf, 0x100 );

    //write to sprite
    memcpy( oam, buf, 0x100 );

    cpu_cycles += 512;
    LOG_TRACE( "DMA", "Transfer DMA at %x", regs.PC );
}


void cpu_test( void )
{
    u8 d;
    mm_set(0x0, 1);
    mm_set(0x1, 0xff);
    mm_set(0x3, 0xff);
    mm_get(0x0, d);
    mm_get(0x2, d);
    mm_get(0x3, d);
}
