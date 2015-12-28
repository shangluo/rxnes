//ines.h
//ines type defines

#ifndef _INES_H
#define _INES_H

#include "types.h"

typedef struct
{
    s8 magic[3]; //must be "NES"
    u8 sig;		 //must be 0x1A
    u8 prg_rom;	 //16K PRG-ROM page count
    u8 chr_rom;  //8K CHR-ROM page count
    u16 misc;
    u8 nram;
    u8 type;	// 1 for PAL, otherwise assume NTFS
    u8 end[6];		 //must be zero
} ines_header;

typedef struct
{
    u8 mapper;
    u8 prg_cnt;
    u8 chr_cnt;
    u8 *prg_banks;
    u8 *chr_banks;
} ines_rom;

void ines_loadrom(const char *filename );
void ines_unloadrom( void );

extern ines_rom *c_rom;

#endif //_INES_H