#include "core/types.h"

#ifndef RX_NES_USE_LUA_MAPPERS
#include <string.h>
#include "core/mapper.h"
static void write(u16 addr, u8 data)
{
	u8 bank_number = data;

	if (addr >= 0x8000 && addr <= 0xffff)
	{
		mapper_switch_prg(0x8000, 16, bank_number);
	}

	return;
}

mapper_implement(2, write, NULL);
#endif