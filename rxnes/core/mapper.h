// mapper.h

#ifndef _MAPPER_H
#define _MAPPER_H

#include "types.h"

#ifndef RX_NES_USE_LUA_MAPPERS
typedef struct 
{
	void(*write)(u16, u8);
	void(*reset)();
	void(*a12)();
	void(*check_pending_irq)();
}mapper_handler;

#define mapper_implement_a12_check_pending_irq(i, w, r, a, l) \
		mapper_handler mapper_handler##i = { w, r, a, l };


#define mapper_implement_a12(i, w, r, a) \
		mapper_implement_a12_check_pending_irq(i, w, r, a, NULL)

#define mapper_implement(i, w, r) \
		mapper_implement_a12(i, w, r, NULL);
#endif

void mapper_init();
void mapper_uninit();
void mapper_make_current(int mapper);
void mapper_reset();
void mapper_write(u16 addr, u8 data);
void mapper_run_loop(u32 cycles);
void mapper_read(u16 addr, u8 *buf, u16 len);

void mapper_switch_prg(u16 offset, int size_in_kb, int bank_no);
void mapper_switch_chr(u16 offset, int size_in_kb, int bank_no);
void mapper_set_mirror_mode(u8 mode);
void mapper_assert_irq();
void mapper_check_pending_irq();

void mapper_state_write(FILE *fp);
void mapper_state_read(FILE *fp);

#endif
