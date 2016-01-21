//ines.c
//ines format handler

#include "ppu.h"
#include "cpu.h"
#include "mapper.h"
#include "ines.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

ines_rom *c_rom;
static char ines_current_rom_file[MAX_PATH];

void ines_loadrom( const char *filename )
{
    ines_header header;
    FILE *fp;

    if ( c_rom )
    {
        ines_unloadrom();
    }

    fp = fopen( filename, "rb" );
    if ( !fp )
    {
        return;
    }

	strncpy(ines_current_rom_file, filename, MAX_PATH);

    fread( &header, sizeof(header), 1, fp );

	ppu_set_mirror_mode(header.misc & 0x1);

    c_rom = ( ines_rom * )malloc( sizeof( ines_rom ) );
    c_rom->prg_cnt = header.prg_rom;
    c_rom->chr_cnt = header.chr_rom;
    c_rom->mapper = ( ( header.misc & 0xf0 ) >> 4 ) | \
                    ( ( header.misc & 0xf000 ) >> 8 );

    c_rom->prg_banks = ( u8 * )malloc ( 16 * 1024 * header.prg_rom );
	fread( c_rom->prg_banks, 1, 16 * 1024 * header.prg_rom, fp );

	if (c_rom->chr_cnt > 0)
	{
		c_rom->chr_banks = (u8 *)malloc(8 * 1024 * header.chr_rom);
		fread(c_rom->chr_banks, 8 * 1024 * header.chr_rom, 1, fp);
		//load vram
	    memcpy( ppu_vram, c_rom->chr_banks, 8 * 1024 * 1 );
	}
	else
	{
		c_rom->chr_banks = NULL;
	}

    fclose( fp );
}

void ines_unloadrom( void )
{
    if ( c_rom != NULL )
    {
        free( c_rom->prg_banks );
        free( c_rom->chr_banks );
        free( c_rom );

        c_rom = NULL;
    }
}

void ines_current_rom_file_name(char *buf, int len)
{
	strncpy(buf, ines_current_rom_file, len);
}

int ines_get_mapper(const char *filename)
{
	ines_header header;
	FILE *fp;
	int mapper;

	fp = fopen(filename, "rb");
	if (!fp)
	{
		return -1;
	}

	strncpy(ines_current_rom_file, filename, MAX_PATH);

	fread(&header, sizeof(header), 1, fp);

	ppu_set_mirror_mode(header.misc & 0x1);

	mapper = ((header.misc & 0xf0) >> 4) | \
		((header.misc & 0xf000) >> 8);

	fclose(fp);

	return mapper;
}
