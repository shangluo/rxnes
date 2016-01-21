#ifndef _PAPU_H
#define _PAPU_H

#include "types.h"
#include "global.h"

#define PAPU_CHANNEL_COUNT 5
#define PAPU_SAMPLE_RATE NES_MASTER_CLOCK_FREQUENCY / CPU_MASTER_CLOCK_DIVIDER

typedef void(*papu_buffer_callback)(double *buffer, int size);

void papu_init(int samplerate, int buffer_duration);
void papu_uninit();
void papu_register_write(u16 addr, u8 data);
u8 papu_register_read(u16 addr);
void papu_run_frame_counter();
void papu_run_loop(u32 cycles);
void papu_check_pending_irq();

void papu_set_buffer_callback(papu_buffer_callback callback);
u8 papu_get_sound_channel(int channel);
#endif
