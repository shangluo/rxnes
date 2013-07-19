#include "types.h"
#include "ppu.h"
#include "cpu.h"
#include "ines.h"
#include <string.h>

static u8 reset;
static u8 load_register;
static u8 cotrol_register;
static u8 chr_bank0;
static u8 chr_bank1;
static u8 prg_bank;

extern u8 mirror;

extern ines_rom *c_rom;

void mapper1_handler( u16 addr, u8 data )
{
    if ( data & 0x80 )
    {
        reset = 0;
        load_register = 0;
        cotrol_register |= 0x0c;
        memcpy( memory + 0xc000, c_rom->prg_banks + ( c_rom->prg_cnt - 1 ) * 1024 * 16, 1024 * 16 );
        return;
    }

    if ( !reset )
    {
        load_register = 0;
    }

    load_register |=  ( ( data & 0x1 ) << reset );
    ++reset;

    if ( reset == 5 )
    {
        //control
        if ( addr >= 0x8000 && addr <= 0x9fff )
        {
            cotrol_register = load_register;
            if ( ( cotrol_register & 0x3 ) == 0x2 )
            {
                mirror = 1;
            }
            else
            {
                mirror = 0;
            }

        }
        //chr_bank0
        else if ( addr >= 0xa000 && addr <= 0xbfff )
        {
            chr_bank0 = load_register;

            if ( !( cotrol_register & 0x10 ) )
            {
                memcpy( vram , c_rom->chr_banks + ( chr_bank0 & 0x1e ) * 1024 * 8, 1024 * 8 );
            } else
            {
                memcpy( vram , c_rom->chr_banks + chr_bank0 * 1024 * 4, 1024 * 4 );
            }

            //rebuild tiles
            ppu_build_tiles();
        }
        //chr_bank1
        else if ( addr >= 0xc000 && addr <= 0xdfff )
        {
            chr_bank1 = load_register;
            if ( !( cotrol_register & 0x10 ) )
            {
                //ignore
                reset = 0;
                return;
            }

            //switch
            memcpy( vram + 0x1000, c_rom->chr_banks + chr_bank1 * 1024 * 4, 1024 * 4 );

            //rebuild tiles
            ppu_build_tiles();
        }
        //prg_bank
        else if ( addr >= 0xe000 && addr <= 0xffff )
        {
            prg_bank = load_register;

            switch ( cotrol_register & 0x0c )
            {
            case 0x0:
            case 0x4:
                //switch 32, igore low bit
                memcpy( memory + 0x8000, c_rom->prg_banks + ( prg_bank & 0x1e ) * 1024 * 32, 1024 * 32 );
                break;

            case 0x8:
                memcpy( memory + 0x8000, c_rom->prg_banks, 1024 * 16 );
                memcpy( memory + 0xc000, c_rom->prg_banks + prg_bank * 1024 * 16, 1024 * 16 );
                break;

            case 0xc:
                memcpy( memory + 0x8000, c_rom->prg_banks + prg_bank * 1024 * 16, 1024 * 16 );
                memcpy( memory + 0xc000, c_rom->prg_banks + ( c_rom->prg_cnt - 1 ) * 1024 * 16, 1024 * 16 );
                break;
            }

        }

        reset = 0;
    }

    return;
}