#include "types.h"
#include "ppu.h"
#include "cpu.h"
#include "ines.h"
#include <string.h>

extern ines_rom *c_rom;
extern u8 mirror;
u8 mmc3_irq_reload_value = 0;
u8 mmc3_irq_counter = 0;
u8 mmc3_irq_enabled = 0;

void mapper4_handler(u16 addr, u8 data)
{
	static u8 bank_selector = 0;
	static u8 chr_a12_inversion = 0;
	static u8 prg_rom_bank_mode = 0;
	static u8 irq_reload = 0;
	// back select
	if (addr >= 0x8000 && addr <= 0x9ffe && addr % 2 == 0)
	{
		bank_selector = data & 0x07;
		prg_rom_bank_mode = (data >> 6) & 0x01;
		chr_a12_inversion = (data >> 7) & 0x01;
	}
	else if (addr >= 0x8001 && addr <= 0x9fff && addr % 2 == 1)
	{
		u8 bank_no = 0;
		if (bank_selector >= 6)
		{
			bank_no = data & 0x3f;
			bank_no >= 1;
		}
		else if (bank_selector <= 1)
		{
			bank_no = data >> 1;
		} 
		else
		{
			bank_no = data ;
		}

		if (bank_selector <= 1)
		{
			memcpy(vram + chr_a12_inversion * 0x1000 + bank_selector * 0x800, c_rom->chr_banks + 2 * 1024 * bank_no, 2 * 1024);
		}
		else if (bank_selector > 1 && bank_selector <= 5)
		{
			memcpy(vram + !chr_a12_inversion * 0x1000 + (bank_selector - 2) * 0x400, c_rom->chr_banks + 1 * 1024 * bank_no, 1 * 1024);
		}
		else if (bank_selector == 6)
		{
			if (prg_rom_bank_mode == 0)
			{
				memcpy(memory + 0x8000, c_rom->prg_banks + 8 * 1024 * bank_no, 8 * 1024);
			}
			else
			{
				memcpy(memory + 0xc000, c_rom->prg_banks + 8 * 1024 * bank_no, 8 * 1024);
			}
		}
		else if (bank_selector == 7)
		{
			memcpy(memory + 0xa000, c_rom->prg_banks + 8 * 1024 * bank_no, 8 * 1024);
		}

		if (bank_selector < 6)
		{
			ppu_build_tiles();
		}
	}
	else if (addr >= 0xa000 && addr <= 0xbffe && addr % 2 == 0)
	{
		mirror = !(data & 0x01);
	}
	else if (addr >= 0xa001 && addr <= 0xbfff && addr % 2 == 1)
	{
		// ram protect
	}
	else if (addr >= 0xc000 && addr <= 0xdffe && addr % 2 == 0)
	{
		//if (irq_reload)
		{
			mmc3_irq_reload_value = data;
		}
	}
	else if (addr >= 0xc001 && addr <= 0xdfff && addr % 2 == 1)
	{
		irq_reload = 1;
	}
	else if (addr >= 0xe000 && addr <= 0xfffe && addr % 2 == 0)
	{
		mmc3_irq_enabled = 0;
	}
	else if (addr >= 0xe001 && addr <= 0xffff && addr % 2 == 1)
	{
		mmc3_irq_enabled = 1;
	}

	return;
}