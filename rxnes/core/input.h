#ifndef _INPUT_H
#define _INPUT_H

#include "types.h"

#define JOYPAD_1 0x4016
#define JOYPAD_2 0x4017

#define JOYPAD_A			0x1
#define JOYPAD_B			0x2
#define JOYPAD_SELECT		0x4
#define JOYPAD_START		0x8
#define JOYPAD_UP			0x10
#define JOYPAD_DOWN			0x20
#define JOYPAD_LEFT			0x40
#define JOYPAD_RIGHT		0x80

typedef void ( *input_handler ) ( void );

void input_init(  input_handler handler );
void input_reset( void );
void input_set_strobe( u8 on );
u8 input_get_next_state( void );

void input_button_up( u8 button );
void input_button_down( u8 button );
void input_check_state( void );

#endif