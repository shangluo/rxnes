#include "core/types.h"

#ifndef RX_NES_USE_LUA_MAPPERS
#include "core/mapper.h"
#include "core/ines.h"
#include <string.h>

static u8 reset;
static u8 load_register;
static u8 cotrol_register;
static u8 chr_bank0;
static u8 chr_bank1;
static u8 prg_bank;

extern ines_rom *c_rom;

static void write( u16 addr, u8 data )
{
    if ( data & 0x80 )
    {
        reset = 0;
        load_register = 0;
        cotrol_register |= 0x0c;
		mapper_switch_prg(0xc000, 16, c_rom->prg_cnt - 1);
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
			mapper_set_mirror_mode((cotrol_register & 0x3) == 0x3);
        }
        //chr_bank0
        else if ( addr >= 0xa000 && addr <= 0xbfff )
        {
            chr_bank0 = load_register;
			if (c_rom->chr_cnt > 0)
			{
				if ( !( cotrol_register & 0x10 ) )
				{
					mapper_switch_chr(0, 8, (chr_bank0 & 0x1e));
				} else
				{
					mapper_switch_chr(0, 4, (chr_bank0 & 0x1e));
				}
			}
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
			mapper_switch_chr(0x1000, 4, chr_bank1);
        }
        //prg_bank
        else if ( addr >= 0xe000 && addr <= 0xffff )
        {
            prg_bank = load_register & 0x0f;

            switch ( cotrol_register & 0x0c )
            {
            case 0x0:
            case 0x4:
                //switch 32, igore low bit
				mapper_switch_prg(0x8000, 32, prg_bank & 0x0e);
                break;

            case 0x8:
				mapper_switch_prg(0xc000, 16, prg_bank);
                break;

            case 0xc:
				mapper_switch_prg(0x8000, 16, prg_bank);
                break;
            }

        }

        reset = 0;
    }

    return;
}

mapper_implement(1, write, NULL);
#endif