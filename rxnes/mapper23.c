#include "types.h"
#include "ppu.h"
#include "cpu.h"
#include "ines.h"
#include <string.h>

extern ines_rom *c_rom;
extern u8 mirror;
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
		memcpy(vram + offset, c_rom->chr_banks + 1 * 1024 * chr_bank_no, 1 * 1024);
		ppu_build_tiles();
	}
}

void mapper23_handler(u16 addr, u8 data)
{
	static u8 prg_swap_mode = 0;
	// back select
	if (addr == 0x9004 || addr == 0x9006)
	{
		prg_swap_mode = data & 0x01;
	}
	else if (addr == 0x8000 ||
		addr == 0x8002 ||
		addr == 0x8004 ||
		addr == 0x8006)
	{
		u8 bank_no = data & 0x1f;
		if (prg_swap_mode)
		{
			memcpy(memory + 0xc000, c_rom->prg_banks + 8 * 1024 * bank_no, 8 * 1024);
		}
		else
		{
			memcpy(memory + 0x8000, c_rom->prg_banks + 8 * 1024 * bank_no, 8 * 1024);
		}
	}
	else if (addr == 0xa000 ||
		addr == 0xa002 ||
		addr == 0xa004 ||
		addr == 0xa006)
	{
		u8 bank_no = data & 0x1f;
		memcpy(memory + 0xa000, c_rom->prg_banks + 8 * 1024 * bank_no, 8 * 1024);
	}
	else if (addr == 0x9000 || addr == 0x9002)
	{
		mirror = data & 0x01;
		mirror = !mirror;
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