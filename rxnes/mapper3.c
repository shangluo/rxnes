#include "types.h"
#include "ppu.h"
#include "cpu.h"
#include "ines.h"
#include "mapper.h"
#include <string.h>

extern ines_rom *c_rom;

static void write(u16 addr, u8 data)
{
	u8 bank_number = data & 0x3 ;

	if (bank_number < c_rom->chr_cnt && addr >= 0x8000 && addr <= 0xffff)
	{
		memcpy(vram, c_rom->chr_banks + bank_number * 1024 * 8, 1024 * 8);
		ppu_build_tiles();
	}

	return;
}

mapper_implement(3, write, NULL);