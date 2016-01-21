
#include "core/types.h"
#ifndef RX_NES_USE_LUA_MAPPERS
#include "core/mapper.h"
#include "core/ines.h"
#include <string.h>

static u8 chr_bank_no = 0;

void mapper23_handle_chr_switch(u16 addr, u8 data, u16 addr_start, u16 addr_end, u16 offset)
{
	if (addr == addr_start)
	{
		chr_bank_no |= data & 0x0f;
	}
	else if (addr == addr_end)
	{
		chr_bank_no |= ((data & 0x1f) << 4);
		mapper_switch_chr(0, 1, chr_bank_no);
	}
}

static void write(u16 addr, u8 data)
{
	static u8 prg_swap_mode = 0;
	// back select
	if (addr == 0x9004 || addr == 0x9006)
	{
		prg_swap_mode = data & 0x01;
		mapper_switch_prg(prg_swap_mode ? 0x8000 : 0xc000, 8, c_rom->prg_cnt * 2 - 2);
	}
	else if (addr == 0x8000 ||
		addr == 0x8002 ||
		addr == 0x8004 ||
		addr == 0x8006)
	{
		mapper_switch_prg(prg_swap_mode ? 0xc000 : 0x8000, 8, data & 0x1f);
	}
	else if (addr == 0xa000 ||
		addr == 0xa002 ||
		addr == 0xa004 ||
		addr == 0xa006)
	{
		u8 bank_no = data & 0x1f;
		mapper_switch_prg(0xa000, 8, bank_no);
	}
	else if (addr == 0x9000 || addr == 0x9002)
	{

		mapper_set_mirror_mode(!(data & 0x01));
	}

	mapper23_handle_chr_switch(addr, data, 0xb000, 0xb002, 0x0);
	mapper23_handle_chr_switch(addr, data, 0xb004, 0xb006, 0x0400);
	mapper23_handle_chr_switch(addr, data, 0xc000, 0xc002, 0x0800);
	mapper23_handle_chr_switch(addr, data, 0xc004, 0xc006, 0x0c00);
	mapper23_handle_chr_switch(addr, data, 0xd000, 0xd002, 0x1000);
	mapper23_handle_chr_switch(addr, data, 0xd004, 0xd006, 0x1400);
	mapper23_handle_chr_switch(addr, data, 0xe000, 0xe002, 0x1800);
	mapper23_handle_chr_switch(addr, data, 0xe004, 0xe006, 0x1c00);

	return;
}

mapper_implement(23, write, NULL);

#endif