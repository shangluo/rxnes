#include "types.h"
#include "ppu.h"
#include "cpu.h"
#include "ines.h"
#include <string.h>

extern ines_rom *c_rom;

void mapper2_handler(u16 addr, u8 data)
{
	u8 bank_number = data;

	if (bank_number < c_rom->prg_cnt && addr >= 0x8000 && addr <= 0xffff)
	{
		memcpy(memory + 0x8000, c_rom->prg_banks + bank_number * 1024 * 16, 1024 * 16);
	}

	return;
}