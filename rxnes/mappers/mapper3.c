#include "core/types.h"
#ifndef RX_NES_USE_LUA_MAPPERS
#include "core/mapper.h"
#include <string.h>

static void write(u16 addr, u8 data)
{
	u8 bank_number = data & 0x3 ;

	if (addr >= 0x8000 && addr <= 0xffff)
	{
		mapper_switch_chr(0, 8, bank_number);
	}

	return;
}

mapper_implement(3, write, NULL);
#endif