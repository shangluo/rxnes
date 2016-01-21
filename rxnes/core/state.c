#include "state.h"
#include "ines.h"
#include "cpu.h"
#include "ppu.h"
#include "mapper.h"

#define STATE_FILE_EXT ".sav"

static void state_get_fn_from_current_rom(char *statefile, int len)
{
	char *p;

	ines_current_rom_file_name(statefile, len);
	p = strrchr(statefile, '.');
	if (p != NULL)
	{
		*p = '\0';
	}

	strcat(statefile, STATE_FILE_EXT);
}

static FILE *state_open_save_file(int load)
{
	FILE *fp;
	char statefile[MAX_PATH] = "\0";

	state_get_fn_from_current_rom(statefile, MAX_PATH);
	fp = fopen(statefile, load ? "rb" : "wb");
	if (!fp)
	{
		return NULL;
	}

	return fp;
}

static void state_close_save_file(FILE *fp)
{
	if (fp)
	{
		fclose(fp);
	}
}

void state_save()
{
	FILE *fp;

	fp = state_open_save_file(0);
	if (!fp)
	{
		return;
	}

	// cpu
	cpu_state_write(fp);
	// ppu
	ppu_state_write(fp);

	// mapper
	mapper_state_write(fp);

	state_close_save_file(fp);
}

void state_load()
{
	FILE *fp;
	fp = state_open_save_file(1);
	if (!fp)
	{
		return;
	}

	// cpu
	cpu_state_read(fp);

	// ppu
	ppu_state_read(fp);

	// mapper
	mapper_state_read(fp);

	state_close_save_file(fp);
}