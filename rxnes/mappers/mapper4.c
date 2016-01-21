#include "core/types.h"

#ifndef RX_NES_USE_LUA_MAPPERS
#include "core/mapper.h"
#include "core/ines.h"
#include <string.h>

static u8 irq_reload_value = 0;
static u8 irq_reload_request = 0;
static u8 irq_counter = 0;
static u8 irq_enabled = 0;
static u8 irq_pending = 0;

static void write(u16 addr, u8 data)
{
	static u8 bank_selector = 0;
	static u8 chr_a12_inversion = 0;
	static u8 prg_rom_bank_mode = 0;
	// back select
	if (addr >= 0x8000 && addr <= 0x9ffe && addr % 2 == 0)
	{
		bank_selector = data & 0x07;
		prg_rom_bank_mode = (data >> 6) & 0x01;
		chr_a12_inversion = (data >> 7) & 0x01;

		mapper_switch_prg(prg_rom_bank_mode ? 0x8000 : 0xc000, 8, c_rom->prg_cnt * 2 - 2);
	}
	else if (addr >= 0x8001 && addr <= 0x9fff && addr % 2 == 1)
	{
		u8 bank_no = 0;
		if (bank_selector >= 6)
		{
			bank_no = data & 0x3f;
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
			mapper_switch_chr(chr_a12_inversion * 0x1000 + bank_selector * 0x800, 2, bank_no);
		}
		else if (bank_selector > 1 && bank_selector <= 5)
		{
			mapper_switch_chr(!chr_a12_inversion * 0x1000 + (bank_selector - 2) * 0x400, 1, bank_no);
		}
		else if (bank_selector == 6)
		{
			if (prg_rom_bank_mode == 0)
			{
				mapper_switch_prg(0x8000, 8, bank_no);
			}
			else
			{
				mapper_switch_prg(0xc000, 8, bank_no);
			}
		}
		else if (bank_selector == 7)
		{
			mapper_switch_prg(0xa000, 8, bank_no);
		}
	}
	else if (addr >= 0xa000 && addr <= 0xbffe && addr % 2 == 0)
	{
		mapper_set_mirror_mode(!(data & 0x01));
	}
	else if (addr >= 0xa001 && addr <= 0xbfff && addr % 2 == 1)
	{
		// ram protect
	}
	else if (addr >= 0xc000 && addr <= 0xdffe && addr % 2 == 0)
	{
		irq_reload_value = data;
	}
	else if (addr >= 0xc001 && addr <= 0xdfff && addr % 2 == 1)
	{
		irq_reload_request = 1;
		//irq_counter = 0;
	}
	else if (addr >= 0xe000 && addr <= 0xfffe && addr % 2 == 0)
	{
		if (irq_pending)
		{
			mapper_assert_irq();
			irq_pending = 0;
		}
		irq_enabled = 0;
	}
	else if (addr >= 0xe001 && addr <= 0xffff && addr % 2 == 1)
	{
		irq_enabled = 1;
	}

	return;
}

static void a12()
{
	u8 old_counter = irq_counter;

	if (irq_reload_request || irq_counter == 0)
	{
		irq_counter = irq_reload_value;
	}
	else if (irq_counter > 0)
	{
		irq_counter = irq_counter - 1;
	}

	if (irq_counter == 0/* && old_counter != 0 */&& irq_enabled)
	{
		//mapper_assert_irq();
		irq_pending = 1;
	}
	
	irq_reload_request = 0;
}

static void check_pending_irq()
{
	if (irq_pending)
	{
		mapper_assert_irq();
	}
}

mapper_implement_a12_check_pending_irq(4, write, NULL, a12, check_pending_irq);

#endif