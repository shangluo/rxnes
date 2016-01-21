//ppu.c

#include "ppu.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

//internal registers
u8 ppu_cam_x, ppu_cam_y;
static u8 ppu_sprite0_hit;

//vram
u8 ppu_vram[0x10000];

//oam
u8 ppu_oam[0x100];

// 
static u16 ppu_oam_addr;

//
static u16 ppu_vram_addr;
static u16 ppu_vram_addr_temp; // t register
static u8 ppu_fine_x;
static u8  ppu_write_toggle;
static u8  ppu_vram_read_buffer;

// secondary oam
static struct
{
	u8 secondary_oam[8][4];
	u8 sprite_index[8];
	u8 sprite_count;
}ppu_secondary_oam;

//screen for rendering
u16 ppu_screen_buffer[240][256];

//mirror type
static u8 ppu_nametable_mirror;

//attribute index table
static u8 ppu_attribute_index_table[30][32];

//attribute table
static u8 ppu_attribute_table[30][32];

//color table
static u16 ppu_palette[64];

//transparent color
#define PPU_TRANSPARENT_COLOR ppu_palette[*(ppu_vram + 0x3f00)]

//current scanline
/*static*/ u16 ppu_current_scanline;

//
static ppu_a12_listner ppu_current_a12_listener;

typedef u8 ppu_tile[8][8];

static ppu_tile ppu_pattern_table[0x2][0x100];

static const u8 ppu_color_pallete[64][3] =
{
	{ 0x7c, 0x7c, 0x7c },{ 0x00, 0x00, 0xfc },{ 0x00, 0x00, 0xbc },{ 0x44, 0x28, 0xbc },
	{ 0x94, 0x00, 0x84 },{ 0xa8, 0x00, 0x20 },{ 0xa8, 0x10, 0x00 },{ 0x88, 0x14, 0x00 },
	{ 0x50, 0x30, 0x00 },{ 0x00, 0x78, 0x00 },{ 0x00, 0x68, 0x00 },{ 0x00, 0x58, 0x00 },
	{ 0x00, 0x40, 0x58 },{ 0x00, 0x00, 0x00 },{ 0x00, 0x00, 0x00 },{ 0x00, 0x00, 0x00 },
	{ 0xbc, 0xbc, 0xbc },{ 0x00, 0x78, 0xf8 },{ 0x00, 0x58, 0xf8 },{ 0x68, 0x44, 0xfc },
	{ 0xd8, 0x00, 0xcc },{ 0xe4, 0x00, 0x58 },{ 0xf8, 0x38, 0x00 },{ 0xe4, 0x5c, 0x10 },
	{ 0xac, 0x7c, 0x00 },{ 0x00, 0xb8, 0x00 },{ 0x00, 0xa8, 0x00 },{ 0x00, 0xa8, 0x44 },
	{ 0x00, 0x88, 0x88 },{ 0x00, 0x00, 0x00 },{ 0x00, 0x00, 0x00 },{ 0x00, 0x00, 0x00 },
	{ 0xf8, 0xf8, 0xf8 },{ 0x3c, 0xbc, 0xfc },{ 0x68, 0x88, 0xfc },{ 0x98, 0x78, 0xf8 },
	{ 0xf8, 0x78, 0xf8 },{ 0xf8, 0x58, 0x98 },{ 0xf8, 0x78, 0x58 },{ 0xfc, 0xa0, 0x44 },
	{ 0xf8, 0xb8, 0x00 },{ 0xb8, 0xf8, 0x18 },{ 0x58, 0xd8, 0x54 },{ 0x58, 0xf8, 0x98 },
	{ 0x00, 0xe8, 0xd8 },{ 0x78, 0x78, 0x78 },{ 0x00, 0x00, 0x00 },{ 0x00, 0x00, 0x00 },
	{ 0xfc, 0xfc, 0xfc },{ 0xa4, 0xe4, 0xfc },{ 0xb8, 0xb8, 0xf8 },{ 0xd8, 0xb8, 0xf8 },
	{ 0xf8, 0xb8, 0xf8 },{ 0xf8, 0xa4, 0xc0 },{ 0xf0, 0xd0, 0xb0 },{ 0xfc, 0xe0, 0xa8 },
	{ 0xf8, 0xd8, 0x78 },{ 0xd8, 0xf8, 0x78 },{ 0xb8, 0xf8, 0xb8 },{ 0xb8, 0xf8, 0xd8 },
	{ 0x00, 0xfc, 0xfc },{ 0xf8, 0xd8, 0xf8 },{ 0x00, 0x00, 0x00 },{ 0x00, 0x00, 0x00 }
};

// for sprite 0 hit
static u8 ppu_scanline_bg_tiles[256 * 2];
static u8 ppu_scanline_sprite_0_tiles[256 * 2];

//set each attribute table
#define set_block( topleft, value )             \
{                                               \
    *( topleft ) = value;                       \
    *( topleft + 1 ) = value;                   \
    *( topleft + 32 ) = value;                  \
    *( topleft + 32 + 1 ) = value;              \
}

void ppu_init_attribute_table( void )
{
    s32 i, j, l;
    u8  k = 0;
    u8 *topleft, *ptr;
    for ( i = 0; i < 30; i += 4 )
    {
        for ( j = 0; j < 32; j += 4 )
        {
            topleft = &ppu_attribute_table[i][j];

            for ( l = 0; l < 4; ++l )
            {
                if ( i >= 28 && l >= 2 )
                {
                    continue;
                }

                ptr = topleft + 32 * l;
                *( ptr ) = k;
                *( ptr + 1 ) = k;
                *( ptr + 2 ) = k;
                *( ptr + 3 ) = k;
            }
            ++k;
        }
    }
}

void ppu_init_attribute_index( void )
{
    s32 i, j;
    u8 *topleft, *ptr;
    for ( i = 0; i < 30; i += 4 )
    {
        for ( j = 0; j < 32; j += 4 )
        {
            topleft = &ppu_attribute_index_table[i][j];

            ptr = topleft;
            set_block( ptr, 0 );

            ptr = topleft + 2;
            set_block( ptr, 1 );

            //skip two row
            if ( i < 28 )
            {
                ptr = topleft + 32 * 2;
                set_block( ptr, 2 );

                ptr += 2;
                set_block( ptr, 3 );
            }
        }
    }
}

#define rgb_to_565( c, r, g, b)                        \
{                                                      \
    r >>= 3;                                           \
    g >>= 2;                                           \
    b >>= 3;                                           \
                                                       \
    c = ( r << 11 ) | ( g << 5 )  | b ;                \
}


static void ppu_build_pallete( void )
{
    int idx = 0;
	u8 r, g, b;
	for (idx = 0; idx < 64; ++idx)
	{
		r = ppu_color_pallete[idx][0];
		g = ppu_color_pallete[idx][1];
		b = ppu_color_pallete[idx][2];
		rgb_to_565(ppu_palette[idx], r, g, b);
	} 
}

 //get nametable address by mirror type
 //implement as macro for speed
//static u16 ppu_fix_nametable_addr(u16 addr)
//{
//	if (ppu_nametable_mirror)
//	{
//		addr &= ~0x0800;
//	}
//	else
//	{
//		addr &= ~0x0400;
//		if (addr >= 0x2800)
//		{
//			addr -= 0x400;
//		}
//	}
//
//	return addr;
//}

#define ppu_fix_nametable_addr(addr) \
	(ppu_nametable_mirror ? \
	(addr & ~0x0800) : \
	(((addr & ~0x0400) >= 0x2800) ? \
		 ((addr & ~0x0400) - 0x400) :\
		 (addr & ~0x0400)))

void ppu_set_mirror_mode(int mirror_type)
{
	ppu_nametable_mirror = mirror_type;
}

void ppu_build_tiles( void )
{
    s32 i;
    s32 j;
    s32 k;
    s32 m;

    //colors
    u8  c1, c2;
    u8  *ptr = ppu_vram;

    //
    u8 line[8];

    for ( i = 0; i < 2; ++i )
    {
        for ( j = 0; j < 0x100; ++j )
        {
            for ( k = 0; k < 8; ++k )
            {
                c1 = *( ptr + k );
                c2 = *( ptr + k + 8 );

                //core!!
                //build tiles
                for ( m = 0; m < 8; ++m )
                {
                    //fill first blank
                    line[m]  = 0;
                    line[m]  = ( c1 >> m ) & 1;
                    line[m] |= ( ( c2 >> m ) & 1 ) << 1;
                }
                memcpy( &ppu_pattern_table[i][j][k], &line, 8 );
            }

            //update ptr
            ptr += 16;
        }
    }
}

void ppu_init( void )
{
    ppu_current_scanline = 0;
    //ppu_build_tiles();
    ppu_build_pallete();
    ppu_init_attribute_table();
    ppu_init_attribute_index();
}

void ppu_reset( void )
{
    ppu_current_scanline = 0;
}

void ppu_reset_status()
{
	ppu_write_toggle = 0;
	memory[PPU_STATUS] &= 0x7f;
}

void ppu_mm_write( u16 addr, u8 data )
{
	//if (addr < 0x2000)
	//{
	//	ppu_build_tiles();
	//}

    //use internal access to memory
    //write to vram
    //notice it's mirrored

	if (addr >= 0x3000 && addr < 0x3f00)
	{
		addr &= 0x2fff;
	}

	if (addr >= 0x3f20 && addr <= 0x3fff)
	{
		addr &= 0x3f1f;
	}

	if (!(addr >= 0x2000 && addr < 0x3000))
		ppu_vram[ addr ] = data;


	// pallete index should be 0 ~ 0x3f
	if (addr >= 0x3f00 && addr <= 0x3f1f)
	{
		ppu_vram[addr] &= 0x3f;
	}

   // pallete
    if ( addr >= 0x3f00 && addr < 0x3f10 )
    {
        //  vram[ addr + 0x10 ] = memory[PPU_DATA];
        if ( addr == 0x3f00 ||
                addr == 0x3f04 ||
                addr == 0x3f08 ||
                addr == 0x3f0c
           )
        {	
            ppu_vram[ addr + 0x10 ] = ppu_vram[ addr ];
        }
        goto done;
    } else if ( addr >= 0x3f10 && addr <= 0x3f20 )
    {
        //  vram[ addr - 0x10 ] = memory[PPU_DATA];
        if ( addr == 0x3f10 ||
                addr == 0x3f14 ||
                addr == 0x3f18 ||
                addr == 0x3f1c
           )
        {
            ppu_vram[ addr - 0x10 ] = ppu_vram[ addr ];
        }
        goto done;
    }

	if (addr >= 0x2000 && addr < 0x3000)
	{
		ppu_vram[ppu_fix_nametable_addr(addr)] = data;
	}

done:
    ppu_vram[ addr + 0x4000 ] = memory[PPU_DATA];
}

u8 ppu_mm_get( u16 addr )
{
	// pallete
	//if (addr >= 0x3f20 && addr <= 0x3fff)
	//{
	//	addr &= 0x3f1f;
	//}

	// name table
	if (addr >= 0x3000 && addr < 0x3fff)
	{
		addr &= 0x2fff;
	}

	if (addr >= 0x2000 && addr < 0x3000)
	{
		addr = ppu_fix_nametable_addr(addr);
	}
    return ppu_vram[addr];
}

//u8 ppu_name_table_fetch(u16 addr)
//{
//	return ppu_vram[ppu_fix_nametable_addr(addr)];
//}

#define ppu_name_table_fetch(addr) (ppu_vram[ppu_fix_nametable_addr((addr))])

void ppu_oam_write()
{
	ppu_oam[ppu_oam_addr] = memory[PPU_SPR_DATA];
	++ppu_oam_addr;
	++memory[PPU_SPR_ADDR];
}

void ppu_oam_update_addr()
{
	ppu_oam_addr = memory[PPU_SPR_ADDR];
}

void ppu_oam_read()
{
	memory[PPU_SPR_DATA] = ppu_oam[ppu_oam_addr];
	//++ppu_oam_addr;
	//++memory[PPU_SPR_ADDR];
}

void ppu_oam_dma(u8 *buf)
{
	//write to sprite
	memcpy(ppu_oam + ppu_oam_addr, buf, 0x100 - ppu_oam_addr);
	memcpy(ppu_oam, buf + 0x100 - ppu_oam_addr, ppu_oam_addr);
}

void ppu_vram_read()
{
	u8 ctr1;
	u8 old_a12;
	u16 addr;

	old_a12 = (ppu_vram_addr >> 12) & 0x01;

	u8 next_read_buf = ppu_mm_get(ppu_vram_addr);
	
	if (ppu_vram_addr >= 0x3f00 && ppu_vram_addr < 0x3fff)
	{
		// pallete
		addr = ppu_vram_addr;
		if (addr >= 0x3f20 && addr <= 0x3fff)
		{
			addr &= 0x3f1f;
		}

		memory[PPU_DATA] = ppu_vram[addr];
	}
	else
	{
		memory[PPU_DATA] = ppu_vram_read_buffer;
	}

	ppu_vram_read_buffer = next_read_buf;
	//do increacement
	cpu_mm_get(PPU_CTRL_REG1, ctr1);
	if (ctr1 & 0x4)
	{
		memory[PPU_ADDRESS] += 32;
		ppu_vram_addr += 32;
		ppu_vram_addr &= 0x3fff;
	}
	else
	{
		++memory[PPU_ADDRESS];
		++ppu_vram_addr;
		ppu_vram_addr &= 0x3fff;
	}

	// rising edge on a12
	if (ppu_current_a12_listener && ((ppu_vram_addr >> 12) & 0x01) == 1 && old_a12 == 0)
	{
		ppu_current_a12_listener();
	}
}

void ppu_vram_write()
{
	u8 ctr1;
	u8 old_a12;

	old_a12 = (ppu_vram_addr >> 12) & 0x01;
	ppu_mm_write(ppu_vram_addr, memory[PPU_DATA]);

	//do increacement
	cpu_mm_get(PPU_CTRL_REG1, ctr1);
	if (ctr1 & 0x4)
	{
		memory[PPU_ADDRESS] += 32;
		ppu_vram_addr += 32;
		ppu_vram_addr &= 0x3fff;
	}
	else
	{
		++memory[PPU_ADDRESS];
		++ppu_vram_addr;
		ppu_vram_addr &= 0x3fff;
	}

	// rising edge on a12
	if (ppu_current_a12_listener && ((ppu_vram_addr >> 12) & 0x01) == 1 && old_a12 == 0)
	{
		ppu_current_a12_listener();
	}
}

void ppu_scroll_reg_write()
{
	if (ppu_write_toggle)
	{
		ppu_vram_addr_temp &= ~0x73e0;
		ppu_vram_addr_temp |= (((memory[PPU_SCROLL_REG] & 0x07) << 12) | (memory[PPU_SCROLL_REG] & 0xf8) << 2);
		ppu_cam_y = memory[PPU_SCROLL_REG];
	}
	else
	{
		// first
		ppu_vram_addr_temp &= ~0x001f;
		ppu_vram_addr_temp |= (memory[PPU_SCROLL_REG] & 0xf8) >> 3;
		ppu_fine_x = memory[PPU_SCROLL_REG] & 0x07;
		ppu_cam_x = memory[PPU_SCROLL_REG];
	}
	ppu_write_toggle = (ppu_write_toggle + 1) % 2;
}

void ppu_vram_update_addr()
{
	u8 paddr;
	u8 old_a12;
	cpu_mm_get(PPU_ADDRESS, paddr);

	//set least bits
	if (ppu_write_toggle == 0)
	{
		//clear
		ppu_vram_addr_temp &= ~0x3f00;
		ppu_vram_addr_temp |= ((paddr & 0x3f) << 8);
		ppu_vram_addr_temp |= (1 << 14);

		//update b_tongle
		ppu_write_toggle = (ppu_write_toggle + 1) % 2;
	}
	//set most bits
	else
	{
		old_a12 = (ppu_vram_addr >> 12) & 0x01;

		ppu_vram_addr_temp &= 0x3f00;
		ppu_vram_addr_temp |= paddr;
		ppu_vram_addr = ppu_vram_addr_temp;
		//update b_tongle
		ppu_write_toggle = (ppu_write_toggle + 1) % 2;

		// rising edge on a12
		if (ppu_current_a12_listener && ((ppu_vram_addr >> 12) & 0x01) == 1 && old_a12 == 0)
		{
			ppu_current_a12_listener();
		}
	}
}

void ppu_set_a12_listener(ppu_a12_listner listener)
{
	ppu_current_a12_listener = listener;
}

//static u8 ppu_is_render_enabled()
//{
//	return (memory[PPU_CTRL_REG2] & 0x18)/* == 0x18*/;
//}

#define ppu_is_render_enabled() (memory[PPU_CTRL_REG2] & 0x18)

void ppu_register_write(u16 addr, u8 data)
{
	switch (addr)
	{
	case PPU_CTRL_REG1:
		ppu_vram_addr_temp &= 0x73ff;
		ppu_vram_addr_temp |= (data & 0x3) << 10;
		break;

	case PPU_CTRL_REG2:
		break;

	case PPU_STATUS:
		break;

	case PPU_ADDRESS:
		ppu_vram_update_addr();
		break;

	case PPU_DATA:
		ppu_vram_write();
		break;

	case PPU_SPR_ADDR:
		ppu_oam_update_addr();
		break;

	case PPU_SPR_DATA:
		ppu_oam_write();
		break;

	case PPU_SCROLL_REG:
		ppu_scroll_reg_write();
		break;
	}
}

void ppu_register_read(u16 addr, u8 *buf)
{
	if (addr == PPU_DATA)
	{
		ppu_vram_read();
		*buf = memory[addr];
	}
	else if (addr == PPU_SPR_DATA)
	{
		ppu_oam_read();
		*buf = memory[addr];
	}
	else if (addr == PPU_STATUS)
	{
		*buf = memory[addr];
		ppu_reset_status();
	}
}

static u8 ppu_sprite_evaluation( u16 scanline )
{
    u8 *ptr = ppu_oam;
	u8 spr_height;
    int cnt = 0;
	u8 real_cnt = 0;

	if (memory[PPU_CTRL_REG1] & 0x20)
	{
		spr_height = 16;
	}
	else
	{
		spr_height = 8;
	}

    for ( ; /*cnt < 8 &&*/ ptr < ppu_oam + 0x100; ptr += 4 )
    {
        if ( *ptr > 239 )
            continue;

        if ( scanline >= ( *ptr + 1 ) && scanline /*<=*/ < ( *ptr + 1 + (spr_height /*- 1*/) ) )
        {
			if (cnt < 8)
			{
				memcpy(ppu_secondary_oam.secondary_oam + cnt, ptr, 4 );
				ppu_secondary_oam.sprite_index[cnt] = (ptr - ppu_oam) / 4;
				cnt++;
			}
			real_cnt++;
        }
    }

	ppu_secondary_oam.sprite_count = cnt;

	if (real_cnt >= 8)
	{
		memory[PPU_STATUS] |= 0x20;
	}

    return cnt;
}

static u8 ppu_check_sprite_priority(u8 spr_index, u8 finex)
{
	int i;
	int idx;
	for (i = 0; i < ppu_secondary_oam.sprite_count; ++i)
	{
		if (ppu_secondary_oam.sprite_index[i] == spr_index)
		{
			idx = i;
			break;
		}
	}

	for (i = 0; i < ppu_secondary_oam.sprite_count; ++i)
	{
		if (ppu_secondary_oam.sprite_index[i] >= spr_index)
		{
			break;
		}

		if ((ppu_secondary_oam.secondary_oam[i][2] & 0x20)
			&& abs((int)ppu_secondary_oam.secondary_oam[idx][3] + finex >= (int)ppu_secondary_oam.secondary_oam[i][3]
				&& (int)ppu_secondary_oam.secondary_oam[idx][3] + finex < (int)ppu_secondary_oam.secondary_oam[i][3] + 8))
		{
			return 0;
		}
	}
	return 1;
}

// phase 0 : background sprite
// phase 1 : background
// phase 2 : front sprite
//static void ppu_write_render_line(u16 line[], u16 offset, u8 pattern_attr, u8 attribute, int param, int scanline)
//{
//	int phase = param & 0x0f;
//	int spr_index = (param >> 4) & 0xff;
//	u16 pallete_offset = phase == 1 ? 0x3f00 : 0x3f10;
//	int finex = (param >> 12) & 0xff;
//
//	if (phase == 1)
//	{
//		if (( pattern_attr != 0 || (( pattern_attr == 0) && (*(line + offset) == PPU_TRANSPARENT_COLOR))) && ((memory[PPU_CTRL_REG2] & 0x02) || (!(memory[PPU_CTRL_REG2] & 0x02) && (offset - ppu_cam_x) >= 8)))
//		{
//			*(line + offset) = ppu_palette[(ppu_vram + pallete_offset)[pattern_attr ? (attribute << 2 | pattern_attr) : pattern_attr]];
//		}
//		if((memory[PPU_CTRL_REG2] & 0x02) || (!(memory[PPU_CTRL_REG2] & 0x02) && (offset - ppu_cam_x) >= 8))
//			ppu_scanline_bg_tiles[offset] = pattern_attr;
//	}
//	else if (pattern_attr != 0)
//	{
//		if ((phase == 0 || (phase == 2 && ppu_check_sprite_priority(spr_index, finex))) && ((memory[PPU_CTRL_REG2] & 0x04) || (!(memory[PPU_CTRL_REG2] & 0x04) && (offset - ppu_cam_x) >= 8)))
//			*(line + offset) = ppu_palette[(ppu_vram + pallete_offset)[attribute << 2 | pattern_attr]];
//		if (spr_index == 0 && ((memory[PPU_CTRL_REG2] & 0x04) || (!(memory[PPU_CTRL_REG2] & 0x04) && (offset - ppu_cam_x) >= 8 )))
//			ppu_scanline_sprite_0_tiles[offset] = pattern_attr;
//	}
//}

#define ppu_write_render_line(line, offset, pattern_attr, attribute, param, scanline) \
do																					  \
{																					  \
	int phase = param & 0x0f;														  \
	int spr_index = (param >> 4) & 0xff;											  \
	u16 pallete_offset = phase == 1 ? 0x3f00 : 0x3f10;								  \
	int finex = (param >> 12) & 0xff;												  \
																					  \
	if (phase == 1)																	  \
	{																				  \
		if (( pattern_attr != 0 ||													  \
		 (( pattern_attr == 0) && (*(line + offset) == PPU_TRANSPARENT_COLOR))) &&    \
		 ((memory[PPU_CTRL_REG2] & 0x02) || (!(memory[PPU_CTRL_REG2] & 0x02) && (offset - ppu_cam_x) >= 8))) \
		{																																\
			*(line + offset) = ppu_palette[(ppu_vram + pallete_offset)[pattern_attr ? (attribute << 2 | pattern_attr) : pattern_attr]];	\
		}																																\
		if((memory[PPU_CTRL_REG2] & 0x02) || (!(memory[PPU_CTRL_REG2] & 0x02) && (offset - ppu_cam_x) >= 8))							\
			ppu_scanline_bg_tiles[offset] = pattern_attr;																				\
	}																				\
	else if (pattern_attr != 0)														\
	{																				\
		if ((phase == 0 || (phase == 2 && ppu_check_sprite_priority(spr_index, finex))) && \
		   ((memory[PPU_CTRL_REG2] & 0x04) || (!(memory[PPU_CTRL_REG2] & 0x04) && (offset - ppu_cam_x) >= 8))) \
			*(line + offset) = ppu_palette[(ppu_vram + pallete_offset)[attribute << 2 | pattern_attr]];		\
		if (spr_index == 0 && ((memory[PPU_CTRL_REG2] & 0x04) || (!(memory[PPU_CTRL_REG2] & 0x04) && (offset - ppu_cam_x) >= 8 ))) \
			ppu_scanline_sprite_0_tiles[offset] = pattern_attr; \
	} \
} while (0)

#define ppu_get_tile_data_by_index(data, pattern_table, index, finex, finey)   \
do																			   \
{																			   \
	u8 tile[2];																   \
	s8 index_off = 0;														   \
	s8 fine_y_off = 0;														   \
	if (finey >= 8)															   \
	{																		   \
		index_off = 1;														   \
		fine_y_off = -8;													   \
	}																		   \
	u16 offset = (pattern_table ? 0x1000 : 0) + (index + index_off) * 2 * 8 + (finey + fine_y_off); \
	tile[0] = *(ppu_vram + offset);											   \
	tile[1] = *(ppu_vram + offset + 8);										   \
	data = (((tile[0] >> (finex)) & 0x01) | (((tile[1] >> (finex)) & 0x01) << 1)); \
} while (0)
 
static void ppu_render_sprite(u16 line[], u8 spr_buf[4], u8 spr_index, u16 scanline)
{
	int i;
	u8 spr_x, spr_y;
	u8 sprite_height;
	u8 spr_mask;
	u8 attribute;
	u8 sprite_tile;
	//ppu_tile *sprite;
	u8 set;
	u8 idx;
	u16 sprite_index;
	int offset;
	int pattern_attr;
	int param = (spr_buf[2] & 0x20) ? 0 : 2 | (spr_index << 4);

	// select tile
	if (memory[PPU_CTRL_REG1] & 0x8)
		sprite_tile = 1;
	else
		sprite_tile = 0;

	if (memory[PPU_CTRL_REG1] & 0x20)
	{
		spr_mask = 0xfe; // ignore last bit as band selector
		sprite_height = 16;
	}
	else
	{
		spr_mask = 0xff;
		sprite_height = 8;
	}

	spr_y = spr_buf[0];
	attribute = spr_buf[2];
	spr_x = spr_buf[3];

	if (sprite_height == 16)
	{
		sprite_tile = spr_buf[1] & 0x1;
	}

	//sprite = &ppu_pattern_table[sprite_tile][spr_buf[1] & spr_mask];
	sprite_index = spr_buf[1] & spr_mask;
	idx = scanline - spr_y - 1;

	set = attribute & 0x3;

	for (i = 0; i < 8; ++i)
	{
		if (spr_x + i < 256)
		{
			offset = ppu_cam_x + spr_x + i;
			switch (attribute & 0xc0)
			{
			case 0x00:
				//pattern_attr = (*sprite)[idx][7 - i];
				ppu_get_tile_data_by_index(pattern_attr, sprite_tile, sprite_index, 7 - i, idx);
				param |= (7 - i) << 12;
				break;

			case 0xc0:
				//pattern_attr = (*sprite)[sprite_height - 1 - idx][i];
				ppu_get_tile_data_by_index(pattern_attr, sprite_tile, sprite_index, i, sprite_height - 1 - idx);
				param |= (i) << 12;
				break;
				//flip h
			case 0x40:
				//pattern_attr = (*sprite)[idx][i];
				ppu_get_tile_data_by_index(pattern_attr, sprite_tile, sprite_index, i, idx);
				param |= (i) << 12;
				break;
			case 0x80:
				//pattern_attr = (*sprite)[sprite_height - 1 - idx][7 - i];
				ppu_get_tile_data_by_index(pattern_attr, sprite_tile, sprite_index, 7 - i, sprite_height - 1 - idx);
				param |= (7 - i) << 12;
				break;
			}

			ppu_write_render_line(line, offset, pattern_attr, set, param, scanline);
		}
	}
}

static void ppu_increase_coarse_y()
{
	int y;
	if ((ppu_vram_addr & 0x7000) != 0x7000)        // if fine Y < 7
	{
		ppu_vram_addr += 0x1000;                      // increment fine Y
	}
	else
	{
		ppu_vram_addr &= ~0x7000;                     // fine Y = 0
		y = (ppu_vram_addr & 0x03e0) >> 5;        // let y = coarse Y
		if (y == 29)
		{
			y = 0;                          // coarse Y = 0
			ppu_vram_addr ^= 0x0800;                    // switch vertical nametable
		}
		else if (y == 31)
		{
			y = 0;                          // coarse Y = 0, nametable not switched
		}
		else
		{
			y += 1;                         // increment coarse Y
		}
		ppu_vram_addr = (ppu_vram_addr & ~0x03e0) | (y << 5);     // put coarse Y back into v
	}

}

static void ppu_increase_coarse_x()
{
	if ((ppu_vram_addr & 0x001f) == 31)
	{
		// if coarse X == 31
		ppu_vram_addr &= ~0x001f;          // coarse X = 0
		ppu_vram_addr ^= 0x0400;          // switch horizontal nametable
	}
	else
	{
		ppu_vram_addr += 1;                // increment coarse X
	}
}

static void ppu_render_background(u16 line[], u16 scanline)
{
	int r, c;
	int i, idx, k;
	u16 base_nt, next_nt;
	u16 base_attr, next_attr;
	u8 bg_tile;
	//ppu_tile *bg;
	u8 attribute_index, attribute;
	u8 set = 0;
	u16 real_line =/* (ppu_cam_y + scanline) % 240*/(((ppu_vram_addr & 0x03e0) >> 2) | (ppu_vram_addr >> 12) & 0x7);
	u16 nametables[2];
	u16 attrs[2];
	u8 tile_pattern;

	//base name tables
	switch (/*memory[PPU_CTRL_REG1]*/(ppu_vram_addr >> 10) & 0x3)
	{
	case 0x0:
		base_nt = 0x2000;
		break;

	case 0x1:
		base_nt = 0x2400;
		break;

	case 0x2:
		base_nt = 0x2800;
		break;

	case 0x3:
		base_nt = 0x2c00;
		break;
	}

	base_attr = base_nt + 0x3c0;
	if (base_nt == 0x2400 || base_nt == 0x2c00)
	{
		next_nt = base_nt - 0x400;
	}
	else
	{
		next_nt = base_nt + 0x400;
	}

	next_attr = next_nt + 0x3c0;

	nametables[0] = base_nt;
	nametables[1] = next_nt;

	attrs[0] = base_attr;
	attrs[1] = next_attr;

	if (memory[PPU_CTRL_REG1] & 0x10)
	{
		bg_tile = 1;
	}
	else
		bg_tile = 0;

	//draw background
	r = real_line / 8;
	idx = real_line % 8;

	for (k = 0; k < 2; ++k)
	{
		for (c = 0; c < 32; ++c)
		{
			//bg = &ppu_pattern_table[bg_tile][ppu_name_table_fetch(nametables[k] + r * 32 + c)];

			//get crospondding attribute table
			attribute = ppu_name_table_fetch(attrs[k] + ppu_attribute_table[r][c]);
			attribute_index = ppu_attribute_index_table[r][c];

			switch (attribute_index)
			{
			case 0x0:
				set = attribute & 0x3;
				break;

			case 0x1:
				set = (attribute & 0xc) >> 2;
				break;

			case 0x2:
				set = (attribute & 0x30) >> 4;
				break;

			case 0x3:
				set = (attribute & 0xc0) >> 6;
				break;
			}

			for (i = 0; i < 8; ++i)
			{
				ppu_get_tile_data_by_index(tile_pattern, bg_tile, ppu_name_table_fetch(nametables[k] + r * 32 + c), i, idx);
				ppu_write_render_line(line, k * 256 + 8 * c + 7 - i, tile_pattern, set, 1, real_line);
			}
		}
	}
}

u32 ppu_render_scanline( u32 n_cycles )
{
    int i, cnt;
	u16 line[256 * 2];

	//memset(line, 0, sizeof(line));
	for (i = 0; i < 256 * 2; ++i)
	{
		line[i] = PPU_TRANSPARENT_COLOR;
	}

	memset(ppu_scanline_bg_tiles, 0, sizeof(ppu_scanline_bg_tiles));
	memset(ppu_scanline_sprite_0_tiles, 0, sizeof(ppu_scanline_sprite_0_tiles));

    //background disable
    if ( !ppu_is_render_enabled() )
    {
        goto finish;
    }

	if (ppu_current_scanline >= 0 && ppu_current_scanline < 240)
	{
		cnt = ppu_sprite_evaluation(ppu_current_scanline);

		// draw sprite high priority
		// if ppu draw sprite bits enabled
		if (memory[PPU_CTRL_REG2] & 0x10)
		{
			for (i = cnt - 1; i >=0 ; --i)
			{
				if (ppu_secondary_oam.secondary_oam[i][2] & 0x20)
				{
					ppu_render_sprite(line, ppu_secondary_oam.secondary_oam[i], ppu_secondary_oam.sprite_index[i], ppu_current_scanline);
				}
			}
		}

		if (memory[PPU_CTRL_REG2] & 0x08)
		{
			ppu_render_background(line, ppu_current_scanline);
		}

		// draw sprite high priority
		// if ppu draw sprite bits enabled
		if (memory[PPU_CTRL_REG2] & 0x10)
		{
			for (i = cnt - 1; i >= 0; --i)
			{
				if (!(ppu_secondary_oam.secondary_oam[i][2] & 0x20))
				{
					ppu_render_sprite(line, ppu_secondary_oam.secondary_oam[i], ppu_secondary_oam.sprite_index[i], ppu_current_scanline);
				}
			}
		}
	}

	if ( (ppu_current_scanline >= 0 && ppu_current_scanline < 240) &&
		!ppu_sprite0_hit && ppu_secondary_oam.sprite_count > 0 && ppu_secondary_oam.sprite_index[0] == 0)
	{
		for (i = ppu_secondary_oam.secondary_oam[0][3]; i < ppu_secondary_oam.secondary_oam[0][3] + 8; ++i)
		{
			if (ppu_scanline_bg_tiles[i] && ppu_scanline_sprite_0_tiles[i] && (i - ppu_cam_x) < 255 )
			{
				ppu_sprite0_hit = 1;
				memory[PPU_STATUS] |= 0x40;
				break;
			}
		}
	}
	
	if (ppu_current_a12_listener 
		&& (ppu_current_scanline >= 0 && ppu_current_scanline < 240 || ppu_current_scanline == 261) 
		&& ppu_is_render_enabled()
		/*&& ((memory[PPU_CTRL_REG1] & 0x08) >> 3) != ((memory[PPU_CTRL_REG1] & 0x10) >> 4)*/)
	{
		//DEBUG_PRINT("Tick MMC3 at line %d\n", ppu_current_scanline);
		ppu_current_a12_listener();
	}

	if (ppu_is_render_enabled() && (ppu_current_scanline == 261 || ppu_current_scanline >= 0 && ppu_current_scanline < 240))
	{
		ppu_increase_coarse_y();
		ppu_vram_addr &= ~0x041f;
		ppu_vram_addr |= (ppu_vram_addr_temp & 0x041f);
	}

	if (ppu_is_render_enabled() && ppu_current_scanline == 261)
	{
		ppu_vram_addr &= ~0x7be0;
		ppu_vram_addr |= (ppu_vram_addr_temp & 0x7be0);
	}

	//if (ppu_is_render_enabled())
	//{
	//	for (i = 0; i < 32 + 2; ++i)
	//	{
	//		ppu_increase_coarse_x();
	//	}
	//}

finish:
	//draw back
	if (ppu_current_scanline >= 0 && ppu_current_scanline < 240)
		memcpy( &ppu_screen_buffer[ppu_current_scanline], line + ppu_cam_x, 256 * 2 );

	// set vblack
	extern u32 cpu_cycles_count;
	static u32 last_cpu_tick = 0;
	if (ppu_current_scanline == 240)
	{
		last_cpu_tick = cpu_cycles_count;
		memory[PPU_STATUS] |= 0x80;

		if (memory[PPU_CTRL_REG1] & 0x80)
		{
			cpu_set_nmi_pending();
		}
	}

    if ( ppu_current_scanline == 260 )
    {
		int tickcount = cpu_cycles_count - last_cpu_tick;
		//DEBUG_PRINT("nmi lasts %d cycles\n", tickcount);
        memory[PPU_STATUS] &= ~0x80;
		memory[PPU_STATUS] &= ~0x20;
		memory[PPU_STATUS] &= ~0x40;
		ppu_sprite0_hit = 0;
    }

    ppu_current_scanline = ( ppu_current_scanline + 1 ) % 262;

    return 1;
}

void ppu_fill_pattern_table(u8 *bits, int index)
{
	int r, c;
	int i, idx;
	//ppu_tile *bg;
	u8 pattern_attr;
	u16 line[128];

	u16 scan_line;

	for (scan_line = 0; scan_line < 128; ++scan_line)
	{
		r = scan_line / 8;
		idx = scan_line % 8;

		for (c = 0; c < 16; ++c)
		{
			//bg = &ppu_pattern_table[index][r * 16 + c];

			for (i = 0; i < 8; ++i)
			{
				ppu_get_tile_data_by_index(pattern_attr, index, r * 16 + c, i, idx);
				*(line + 8 * c + 7 - i) = ppu_palette[(ppu_vram + 0x3f00)[pattern_attr ? (0 * 4 + pattern_attr) : pattern_attr]];
			}
		}

		//draw back
		memcpy(bits, line, sizeof(line));
		bits += sizeof(line);
	}
}

void ppu_fill_name_table(u8 *bits, int index)
{
	int r, c;
	int i, idx;
	u16 base_nt;
	u16 base_attr;
	u8 bg_tile;
	//ppu_tile *bg;
	u16 line[256];
	u8 attribute_index, attribute;
	u8 set = 0;
	u16 scan_line;
	u8 tile_pattern;

	for (scan_line = 0; scan_line < 240; ++scan_line)
	{
		//base name tables
		switch (index)
		{
		case 0x0:
			base_nt = + 0x2000;
			break;

		case 0x1:
			base_nt = + 0x2400;
			break;

		case 0x2:
			base_nt = + 0x2800;
			break;

		case 0x3:
			base_nt = + 0x2c00;
			break;
		}

		base_attr = base_nt + 0x3c0;

		if (memory[PPU_CTRL_REG1] & 0x10)
		{
			bg_tile = 1;
		}
		else
			bg_tile = 0;

		//draw background
		r = scan_line / 8;
		idx = scan_line % 8;

		for (c = 0; c < 32; ++c)
		{
			//bg = &ppu_pattern_table[bg_tile][ppu_name_table_fetch( base_nt + r * 32 + c)];

			//get crospondding attribute table
			attribute = ppu_name_table_fetch(base_attr + ppu_attribute_table[r][c]);
			attribute_index = ppu_attribute_index_table[r][c];

			switch (attribute_index)
			{
			case 0x0:
				set = attribute & 0x3;
				break;

			case 0x1:
				set = (attribute & 0xc) >> 2;
				break;

			case 0x2:
				set = (attribute & 0x30) >> 4;
				break;

			case 0x3:
				set = (attribute & 0xc0) >> 6;
				break;
			}


			for (i = 0; i < 8; ++i)
			{
				ppu_get_tile_data_by_index(tile_pattern, bg_tile, ppu_name_table_fetch(base_nt + r * 32 + c), i, idx);
				*(line + 8 * c + 7 - i) = ppu_palette[(ppu_vram + 0x3f00)[tile_pattern ? ((set << 2) | tile_pattern) : tile_pattern]];
			}
		}

		//draw back
		memcpy(bits, line, sizeof(line));
		bits += sizeof(line);
	}
}

void ppu_fill_pallete_table(u8 *bits)
{
	int i;
	for (i = 0; i < 32; ++i)
	{ 
		memcpy(bits, &ppu_palette[*(ppu_vram + 0x3f00 + i)], sizeof(u16));
		bits += sizeof(u16);
	}
}

void ppu_set_custom_pallete(int index, u16 color)
{
	if (index < 64)
	{
		ppu_palette[index] = color;
	}
}

u16 ppu_get_pallete_color(int index)
{
	if (index < 64)
	{
		return ppu_palette[index];
	}
	return 0;
}

void ppu_state_write(FILE *fp)
{
	fwrite(ppu_vram, sizeof(ppu_vram), 1, fp);
	fwrite(&ppu_secondary_oam, sizeof(ppu_secondary_oam), 1, fp);
}

void ppu_state_read(FILE *fp)
{
	fread(ppu_vram, sizeof(ppu_vram), 1, fp);
	fread(&ppu_secondary_oam, sizeof(ppu_secondary_oam), 1, fp);
}