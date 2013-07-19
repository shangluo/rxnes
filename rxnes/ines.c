//ines.c
//ines format handler

#include "ppu.h"
#include "cpu.h"
#include "ines.h"
#include <stdio.h>
#include <stdlib.h>

ines_rom *c_rom;
u8 mirror;

void ines_loadrom( char *filename )
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

    fread( &header, sizeof(header), 1, fp );

    if ( header.misc & 0x1 )
    {
        //vertical
        mirror = 1;
    }
    else
        //horizontal
        mirror = 0;

    c_rom = ( ines_rom * )malloc( sizeof( ines_rom ) );
    c_rom->prg_cnt = header.prg_rom;
    c_rom->chr_cnt = header.chr_rom;
    c_rom->mapper = ( ( header.misc & 0xf0 ) >> 4 ) | \
                    ( ( header.misc & 0xf000 ) >> 8 );

    c_rom->prg_banks = ( u8 * )malloc ( 16 * 1024 * header.prg_rom );
    fread( c_rom->prg_banks, 16 * 1024 * header.prg_rom, 1, fp );

    c_rom->chr_banks = ( u8 * )malloc ( 8 * 1024 * header.chr_rom );
    fread( c_rom->chr_banks, 8 * 1024 * header.chr_rom, 1, fp );

    //load game
    //first to 0x8000
    memcpy( memory + 0x8000, c_rom->prg_banks, 16 * 1024 );

    //last to 0xc000
    memcpy( memory + 0xc000, ( c_rom->prg_banks ) + ( header.prg_rom - 1 ) * 16 * 1024 , 16 * 1024 );

    //load vram
    memcpy( vram, c_rom->chr_banks, 8 * 1024 * 1 );

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