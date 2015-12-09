//ppu.h

#ifndef _PPU_H
#define _PPU_H

#include "types.h"
#include "cpu.h"

extern u8 vram[0x10000];
extern u8 saram[0x100];
extern u16 screen[240][256];

void ppu_init( void );
void ppu_reset ( void );
void ppu_build_tiles( void );
u32  ppu_render_scanline( u32 n_cycles );
void ppu_mm_write( u16 addr, u8 data );
u8 ppu_mm_get( u16 addr );
void ppu_fill_name_table(u8 *bits, int index);
void ppu_fill_pattern_table(u8 *bits, int index);

//ppu registers
#define PPU_CTRL_REG1   0x2000
#define PPU_CTRL_REG2   0x2001
#define PPU_STATUS      0x2002
#define PPU_SPR_ADDR    0x2003
#define PPU_SPR_DATA    0x2004
#define PPU_SCROLL_REG  0x2005
#define PPU_ADDRESS     0x2006
#define PPU_DATA        0x2007

//dma register
#define SPR_DMA         0x4014


#endif //_PPU_H