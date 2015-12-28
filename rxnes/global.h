#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <stdarg.h>
#include <stdio.h>

#define NES_MASTER_CLOCK_FREQUENCY 21477272
#define PPU_MASTER_CLOCK_DIVIDER 4
#define MASTER_CYCLE_PER_SCANLINE 341 * PPU_MASTER_CLOCK_DIVIDER
#define CPU_MASTER_CLOCK_DIVIDER 12
#define NES_NTSC_REFRESH_RATE 60

#ifdef _WIN32
#include <Windows.h>
#define DEBUG_PRINT(fmt, ...)			\
do										\
{										\
	char errBuf[1024];					\
	sprintf(errBuf, fmt, __VA_ARGS__);	\
	OutputDebugStringA(errBuf);			\
} while (0)
#endif

//#define RX_NES_DISABLE_SOUND
#define RX_NES_RENDER_DX9

#endif