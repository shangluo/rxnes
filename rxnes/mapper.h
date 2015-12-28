// mapper.h

#ifndef _MAPPER_H
#define _MAPPER_H

#include "types.h"

typedef struct 
{
	void(*write)(u16, u8);
	void(*reset)();
}mapper_handler;

#define mapper_implement(i, w, r) \
		mapper_handler mapper_handler##i = { w, r };

void mapper_init();
void mapper_uninit();
void mapper_make_current(int mapper);
void mapper_reset();
void mapper_write(u16 addr, u8 data);

void mapper_switch_prg(u16 offset, int size_in_kb, int bank_no);
void mapper_switch_chr(u16 offset, int size_in_kb, int bank_no);
void mapper_set_mirror_mode(u8 mode);

#endif
