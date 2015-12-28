#include "emulator.h"

#include "cpu.h"
#include "papu.h"
#include "ppu.h"
#include "papu.h"
#include "global.h"
#include "ines.h"
#include "mapper.h"
#include "log.h"
#include <Windows.h>

static u32 emulator_last_tick = 0;
static u32 emulator_extra_cycle = 0;
static u32 emulator_scanline_passed = 0;

void emulator_init()
{
	LOG_INIT();
	mapper_init();
	emulator_last_tick = 0;
}

void emulator_load(const char *rom)
{
	ines_unloadrom();
	ines_loadrom(rom);

	mapper_make_current(c_rom->mapper);
	mapper_reset();

	cpu_init();
	ppu_init();
	papu_init(EMULATOR_DEF_SOUND_SAMLE_RATE, EMULATOR_DEF_SOUND_DURATION);

	emulator_last_tick = 0;
}

void emulator_reset()
{
	cpu_init();
	ppu_init();
	papu_init(EMULATOR_DEF_SOUND_SAMLE_RATE, EMULATOR_DEF_SOUND_DURATION);
}

void emulator_set_sound_callback(papu_buffer_callback callback)
{
	papu_set_buffer_callback(callback);
}

void emulator_set_input_handler(input_handler handler)
{
	input_init(handler);
}

static u32 emulator_get_tick()
{
	return GetTickCount();
}

static u32 emulator_get_tick_per_frame()
{
	double ticks = 1000.0 / NES_MASTER_CLOCK_FREQUENCY * PPU_MASTER_CLOCK_DIVIDER * 341 * 262;
	return (u32)(ticks);
}

static void emulator_delay(u32 ms)
{
	Sleep(ms);
}

void emulator_run_loop()
{
	int i;
	int cycle_cnts = 1;
	u32 cpu_to_execute = MASTER_CYCLE_PER_SCANLINE / CPU_MASTER_CLOCK_DIVIDER;

	if (!emulator_last_tick)
	{
		emulator_last_tick = emulator_get_tick();
	}

	u32 cycles = cpu_execute_translate(cpu_to_execute);
	
#ifndef RX_NES_DISABLE_SOUND
	// papu
	papu_run_loop(cycles);
#endif

	emulator_extra_cycle += (cycles - cpu_to_execute);
	while (emulator_extra_cycle >= cpu_to_execute)
	{
		emulator_extra_cycle -= cpu_to_execute;
		++cycle_cnts;
	}

	for (i = 0; i < cycle_cnts; ++i)
	{
		ppu_render_scanline(MASTER_CYCLE_PER_SCANLINE / PPU_MASTER_CLOCK_DIVIDER);
		++emulator_scanline_passed;

#ifndef RX_NES_DISABLE_SOUND
		if (emulator_scanline_passed % 65 == 0)
		{
			papu_run_frame_counter();
		}
#endif

		// sync to refresh rate
		if (emulator_scanline_passed % 262 == 0)
		{
			u32 expected = emulator_last_tick + emulator_get_tick_per_frame();
			u32 current = emulator_get_tick();
			//DEBUG_PRINT("last = %d, current = %d\n", emulator_last_tick, current);
			if (current < expected)
			{
			//	DEBUG_PRINT("Delayed!\n");
				emulator_delay(expected - current);
			}
			
			emulator_last_tick = emulator_get_tick();
		}
	}
}

void emulator_uninit()
{
	LOG_CLOSE();
	papu_uninit();
}
