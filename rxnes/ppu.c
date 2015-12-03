//ppu.c

#include "ppu.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#define NAME_TABLE_BASE 0x2000

//internal registers
u16 v;
u16 t;
u16 fine_x;
u8 cam_x, cam_y;
u8 sprite0_hit;
u8 hit_line;

//vram
u8 vram[0x10000];

//oam
u8 oam[0x100];

//screen for rendering
u16 screen[240][256];

//mirror type
u8 mirror;

//attribute index table
u8 attribute_index_table[30][32];

//attribute table
u8 attribute_table[30][32];

//
u8 oam_idx[64];

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
            topleft = &attribute_table[i][j];

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
            topleft = &attribute_index_table[i][j];

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

//color table
u16 palette[64];

//current scanline
static u16 cur_scanline;
static u8 cur_vline;

typedef u8 tile[8][8];

static tile tiles[0x2][0x100];

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


static void ppu_build_pallete( void )
{
    int idx = 0;
	u8 r, g, b;
	for (idx = 0; idx < 64; ++idx)
	{
		r = ppu_color_pallete[idx][0];
		g = ppu_color_pallete[idx][1];
		b = ppu_color_pallete[idx][2];
		rgb_to_565(palette[idx], r, g, b);
	} 
}

void ppu_build_tiles( void )
{
    s32 i;
    s32 j;
    s32 k;
    s32 m;

    //colors
    u8  c1, c2;
    u8  *ptr = vram;

    //
    u8 line[8];

    //fucking complicated! damn
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
                memcpy( &tiles[i][j][k], &line, 8 );
            }

            //update ptr
            ptr += 16;
        }
    }
}

void ppu_init( void )
{
    cur_scanline = 0;
    ppu_build_tiles();
    ppu_build_pallete();
    ppu_init_attribute_table();
    ppu_init_attribute_index();
}

void ppu_reset( void )
{
    cur_scanline = 0;
}

void ppu_mm_write( u16 addr, u8 data )
{
    //use internal access to memory
    //write to vram
    //notice it's mirrored

    if ( addr >= 0x3000 && addr < 0x3f00 )
    {
        //ignore
        return;
    }


    vram[ addr ] = memory[PPU_DATA];

    //pallete
    if ( addr >= 0x3f00 && addr < 0x3f10 )
    {
        //  vram[ addr + 0x10 ] = memory[PPU_DATA];
        if ( addr == 0x3f00 ||
                addr == 0x3f04 ||
                addr == 0x3f08 ||
                addr == 0x3f0c
           )
        {
            vram[ addr + 0x10 ] = vram[ addr ];
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
            vram[ addr - 0x10 ] = vram[ addr ];
        }
        goto done;
    }

    //Vertical
    if ( mirror )
    {
        if ( ( addr >= 0x2000 && addr <  0x2400 ) ||
                ( addr >= 0x2400 && addr <= 0x2800 ) )
        {
            vram[ addr + 0x800 ] = vram[ addr ];
            vram[ addr + 0x1000 ] = vram[ addr ];

            if (  addr + 0x800 + 0x1000 < 0x3f00 )
                vram[ addr + 0x800 + 0x1000 ] = vram[ addr ];
        }
        else if ( addr >= 0x2800 && addr < 0x3000 )
        {
            vram[ addr - 0x800 ] = vram[ addr ];
            vram[ addr + 0x1000 ] = vram[ addr ];
            vram[ addr -0x800 + 0x1000 ] = vram[ addr ];

            if (  addr + 0x800 + 0x1000 < 0x3f00 )
                vram[ addr + 0x800 + 0x1000 ] = vram[ addr ];
        }
    }
    else
    {
        if ( ( addr >= 0x2000 && addr <  0x2400 ) ||
                ( addr >= 0x2800 && addr <= 0x2c00 ) )
        {
            vram[ addr + 0x400 ] = vram[ addr ];
            vram[ addr + 0x1000 ] = vram[ addr ];
            if (  addr + 0x400 + 0x1000 < 0x3f00 )
                vram[ addr + 0x400 + 0x1000 ] = vram[ addr ];
        }
        else if ( ( addr >= 0x2400 && addr < 0x2800 ) ||
                  ( addr >= 0x2c00 && addr < 0x3000 ) )
        {
            vram[ addr - 0x400 ] = vram[ addr ];
            vram[ addr + 0x1000 ] = vram[ addr ];
            if (  addr - 0x400 + 0x1000 < 0x3f00 )
                vram[ addr + 0x400 + 0x1000 ] = vram[ addr ];
        }
    }

done:
    vram[ addr + 0x4000 ] = memory[PPU_DATA];
}

u8 ppu_mm_get( u16 addr )
{
    return vram[addr];
}


static u8 ppu_fill_current_oam( u16 scanline, u8 *oam_tmp )
{
    u8 *ptr = oam;
    int cnt = 0;

    for ( ; cnt < 8 && ptr < oam + 0x100; ptr += 4 )
    {
        if ( *ptr > 239 )
            continue;

        if ( scanline >= ( *ptr - 1 ) && scanline <= ( *ptr - 1 + 7 ) )
        {
            memcpy( oam_tmp + cnt * 4  , ptr, 4 );
            cnt++;
        }
    }

    return cnt;
}

u32 ppu_render_scanline( u32 n_cycles )
{
    int cnt;
    int r, c;
    int i, j, idx;
    u8 oam_tmp[8][4];
    u8 *base_nt, *next_nt;
    u8 *base_attr, *next_attr;
    u8 bg_tile, sprite_tile;
    tile *bg, *sprite;
    u16 line[256 * 2];
    u16 *b_line, *n_line;
    u8 attribute_index, attribute;
    u8 set = 0;
    u8 spr_x, spr_y;
    u16 spr_c;


    b_line = line;
    n_line = line + 256;

    //background disable
    if ( !( memory[PPU_CTRL_REG2] & 0x04 ) )
    {
        memset( &screen[cur_scanline], 0, 256 * 2 );
        goto finish;
    }

    if ( sprite0_hit )
    {
        if ( cur_scanline == hit_line  )
            memory[PPU_STATUS] |= 0x40;
    }

    //base name tables
    switch ( memory[PPU_CTRL_REG1] & 0x3 )
    {
    case 0x0:
        base_nt = vram + 0x2000;
        break;

    case 0x1:
        base_nt = vram + 0x2400;
        break;

    case 0x2:
        base_nt = vram + 0x2800;
        break;

    case 0x3:
        base_nt = vram + 0x2c00;
        break;
    }

    base_attr = base_nt + 0x3c0;

    if ( mirror )
    {
        next_nt = base_nt + 0x400;
    }
    else
        next_nt = base_nt + 0x800;

    next_attr = next_nt + 0x3c0;

    if ( memory[PPU_CTRL_REG1] & 0x10 )
    {
        bg_tile = 1;
    }
    else
        bg_tile = 0;

    if ( memory[PPU_CTRL_REG1] & 0x8 )
    {
        sprite_tile = 1;
    }
    else
        sprite_tile = 0;

    if ( cur_scanline >= 0 && cur_scanline <= 240 )
    {
        cnt = ppu_fill_current_oam( cur_scanline, (u8 *)oam_tmp );

        if ( cnt >= 8 )
        {
            memory[PPU_STATUS] |= 0x10;
        }

        //draw background
        r = cur_scanline / 8;
        idx = cur_scanline % 8;

        for ( c = 0; c < 32; ++c )
        {
            bg = &tiles[bg_tile][ *( base_nt + r * 32 + c ) ];


            //get crospondding attribute table
            attribute = base_attr[ attribute_table[r][c] ];
            attribute_index = attribute_index_table[r][c];

            switch ( attribute_index )
            {
            case 0x0:
                set = attribute & 0x3;
                break;

            case 0x1:
                set = ( attribute & 0xc ) >> 2;
                break;

            case 0x2:
                set = ( attribute & 0x30 ) >> 4;
                break;

            case 0x3:
                set = ( attribute & 0xc0 ) >> 6;
                break;
            }


            for ( i = 0; i < 8; ++i )
            {
                *( b_line + 8 * c + 7 - i ) = palette[ (vram+0x3f00)[ (*bg)[idx][i] ? ( set * 4 + (*bg)[idx][i] ) : (*bg)[idx][i] ]];
            }
        }

        for ( c = 0; c < 32; ++c )
        {
            bg = &tiles[bg_tile][ *( next_nt + r * 32 + c ) ];


            //get crospondding attribute table
            attribute = next_attr[ attribute_table[r][c] ];
            attribute_index = attribute_index_table[r][c];

            switch ( attribute_index )
            {
            case 0x0:
                set = attribute & 0x3;
                break;

            case 0x1:
                set = ( attribute & 0xc ) >> 2;
                break;

            case 0x2:
                set = ( attribute & 0x30 ) >> 4;
                break;

            case 0x3:
                set = ( attribute & 0xc0 ) >> 6;
                break;
            }


            for ( i = 0; i < 8; ++i )
            {
                *( n_line + 8 * c + 7 - i ) = palette[ (vram+0x3f00)[ (*bg)[idx][i] ? ( set * 4 + (*bg)[idx][i] ) : (*bg)[idx][i] ]];
            }
        }

        //draw sprite

        if ( !( memory[PPU_CTRL_REG2] & 0x10 ) )
            goto finish;

        for ( i = 0; i < cnt; ++i )
        {
            spr_y = oam_tmp[i][0];
            attribute = oam_tmp[i][2];
            spr_x = oam_tmp[i][3];
            sprite = &tiles[sprite_tile][ oam_tmp[i][1] ];
            idx = cur_scanline - spr_y  + 1;

            set = attribute & 0x3;

            if ( !sprite0_hit )
            {
                for ( j = 0; j < 8; ++j )
                {
                    if ( (*sprite)[idx][ 7 - j] != 0 )
                    {
                        sprite0_hit = 1;
                        hit_line = cur_scanline + 7 - idx;
                        break;
                    }
                }
            }

            //low prority
            if ( attribute & 0x20 )
                continue;

            for ( j = 0; j < 8; ++j )
            {
                if ( spr_x + j < 256 )
                {
                    switch ( attribute & 0xc0 )
                    {
                    case 0x00:
                        if ( (*sprite)[idx][7 - j] != 0 )
                            *( line + cam_x + spr_x + j ) = palette[ (vram+0x3f10)[( set << 2 | (*sprite)[idx][7 - j] ) ] ];
                        break;

                    case 0xc0:
                        if ( (*sprite)[7 - idx][j] != 0 )
                            *( line + cam_x + spr_x + j ) = palette[ (vram+0x3f10)[( set << 2 | (*sprite)[7 - idx][j] ) ] ];

                        break;
                        //flip h
                    case 0x40:
                        if ( (*sprite)[idx][j] != 0 )
                            *( line + cam_x + spr_x + j ) = palette[ (vram+0x3f10)[( set << 2 | (*sprite)[idx][j] ) ] ];
                        break;
                    case 0x80:
                        if ( (*sprite)[7 - idx][7 - j] != 0 )
                            *( line + cam_x + spr_x + j ) = palette[ (vram+0x3f10)[( set << 2 | (*sprite)[7 - idx][7 - j] ) ] ];
                        break;
                    }
                }
            }
        }
        //draw back
        memcpy( &screen[cur_scanline], line + fine_x , 256 * 2 );
    }

finish:

    if ( cur_scanline == 0 )
    {
        memory[PPU_STATUS] &= ~0x40;
        memory[PPU_STATUS] |= 0x80;
        sprite0_hit = 0;
        hit_line = 255;
    }

    if ( cur_scanline == 20 )
    {
        memory[PPU_STATUS] &= ~0x80;
    }

    cur_scanline = ( cur_scanline + 1 ) % 240;

    return 1;
}
