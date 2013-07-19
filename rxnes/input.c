#include "input.h"
#include "cpu.h"

//joypad1
u8 joypad1;
//shift register
u8 shift;
//
u8 strobe;

//input handler
input_handler i_handler;

void input_init(  input_handler handler )
{
    i_handler = handler;
}

void input_reset( void )
{
    shift = 0;
    strobe = 0;
}

void input_set_strobe( u8 on )
{
    strobe = on;
}

u8 input_get_next_state( void )
{
    //return current state
    u8 state = ( joypad1 >> shift ) & 0x1;

    if ( strobe )
    {
        ++shift;
    }

    return state;
}


void input_button_up( u8 button )
{
    joypad1 &= (~button);
}

void input_button_down( u8 button )
{
    joypad1 |= button;
}

void input_check_state( void )
{
    //call back function
    i_handler();
}