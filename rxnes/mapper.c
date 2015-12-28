// mapper.c

#include <stdlib.h>
#include <string.h>
#include "ppu.h"
#include "mapper.h"
#include "cpu.h"
#include "ines.h"

#define MAPPER_HANDLER_VECTOR_SIZE 256
static mapper_handler *mapper_handler_vector[MAPPER_HANDLER_VECTOR_SIZE];
static int mapper_current;
static mapper_handler *mapper_get_handler(int index);

extern ines_rom *c_rom;

#define mapper_init_n(i)																								\
		do																												\
 		{																												\
			extern mapper_handler mapper_handler##i;																	\
			mapper_handler_vector[i] = &mapper_handler##i;																\
		} while (0);

void mapper_init()
{	
	memset(mapper_handler_vector, 0, sizeof(mapper_handler_vector));
	mapper_init_n(1);
	mapper_init_n(2);
	mapper_init_n(3);
	mapper_init_n(4);
	mapper_init_n(23);
	mapper_init_n(33);
}

void mapper_uninit()
{
}

void mapper_reset()
{
	mapper_handler *handler = mapper_get_handler(mapper_current);
	if (handler && handler->reset)
	{
		handler->reset();
	}
}

void mapper_make_current(int mapper)
{
	mapper_current = mapper;
}

void mapper_write(u16 addr, u8 data)
{
	mapper_handler *handler = mapper_get_handler(mapper_current);
	if (handler && handler->write)
	{
		handler->write(addr, data);
	}
}

static void mapper_register_handler(int index, mapper_handler *handler)
{
	if (index < MAPPER_HANDLER_VECTOR_SIZE)
	{
		mapper_handler_vector[index] = handler;
	}
}

mapper_handler *mapper_get_handler(int index)
{
	mapper_handler *handler = NULL;
	if (index < MAPPER_HANDLER_VECTOR_SIZE)
	{
		handler = mapper_handler_vector[index];
	}

	return handler;
}

void mapper_switch_prg(u16 offset, int size_in_kb, int bank_no)
{
	if (bank_no > c_rom->prg_cnt * 16  / size_in_kb)
	{
		bank_no &= (c_rom->prg_cnt * 16 / size_in_kb) -	1;
	}

	memcpy(memory + offset, c_rom->prg_banks + bank_no * 1024 * size_in_kb, 1024 * size_in_kb);
}

void mapper_switch_chr(u16 offset, int size_in_kb, int bank_no)
{
	if (bank_no > c_rom->chr_cnt * 8 / size_in_kb)
	{
		bank_no &= (c_rom->chr_cnt * 8 / size_in_kb) - 1;
	}
	if (c_rom->chr_cnt > 0)
		memcpy(ppu_vram + offset, c_rom->chr_banks + bank_no * 1024 * size_in_kb, 1024 * size_in_kb);
	ppu_build_tiles();
}

void mapper_set_mirror_mode(u8 mode)
{
	ppu_set_mirror_mode(mode);
}