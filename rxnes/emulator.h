#ifndef _EMULATOR_H_
#define _EMULATOR_H_

#include "papu.h"
#include "input.h"

#define EMULATOR_DEF_SOUND_SAMLE_RATE 44100
#define EMULATOR_DEF_SOUND_DURATION 16

void emulator_init();
void emulator_load(const char *rom);
void emulator_set_input_handler(input_handler handler);
void emulator_set_sound_callback(papu_buffer_callback callback);
void emulator_reset();
void emulator_run_loop();
void emulator_uninit();

#endif // !_EMULATOR_H_
