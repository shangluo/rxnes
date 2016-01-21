//ppu.h

#ifndef _PPU_H
#define _PPU_H

#include "types.h"
#include "cpu.h"

extern u8 ppu_vram[0x10000];
extern u16 ppu_screen_buffer[240][256];

typedef void(*ppu_a12_listner)();

void ppu_init( void );
void ppu_reset ( void );
void ppu_reset_status(void);
void ppu_set_mirror_mode(int mirror_type);
void ppu_build_tiles( void );
u32  ppu_render_scanline( u32 n_cycles );
void ppu_fill_name_table(u8 *bits, int index);
void ppu_fill_pattern_table(u8 *bits, int index);
void ppu_fill_pallete_table(u8 *bits);
void ppu_register_write(u16 addr, u8 data);
void ppu_register_read(u16 addr, u8 *buf);
void ppu_set_a12_listener(ppu_a12_listner listener);
void ppu_set_custom_pallete(int index, u16 color);
u16 ppu_get_pallete_color(int index);

// scroll
void ppu_scroll_reg_write();

// vram
void ppu_vram_read();
void ppu_vram_write();
void ppu_vram_update_addr();
void ppu_mm_write( u16 addr, u8 data );
u8 ppu_mm_get( u16 addr );

// oam
void ppu_oam_read();
void ppu_oam_write();
void ppu_oam_update_addr();
void ppu_oam_dma(u8 *buf);

// state
void ppu_state_write(FILE *fp);
void ppu_state_read(FILE *fp);

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