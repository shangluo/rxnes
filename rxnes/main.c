#include "ines.h"
#include "cpu.h"
#include "ppu.h"
#include "log.h"
#include "input.h"
#include <stdio.h>
#include <time.h>
#include <SDL/SDL.h>

#define CYCLE_PER_SCANLINE 114

SDL_Surface *sdl_screen;

static u16 screen2[480][512];


int running;
int pause;

void scale2( u16 sc[][256], u16 sc2[][512] )
{
    int i, j;
    int w, h;

    for ( i = 0; i < 240; ++i )
    {
        w = i * 2;
        for ( j = 0; j < 256 ; ++ j )
        {
            h = j * 2;
            sc2[w][h] = sc[i][j];
            sc2[w][h + 1] = sc[i][j];
            sc2[w + 1][h] = sc[i][j];
            sc2[w + 1][h + 1] = sc[i][j];
        }
    }
}

void init_sdl( void )
{
    SDL_Init( SDL_INIT_VIDEO );
    sdl_screen = SDL_SetVideoMode( 512, 480, 16, SDL_DOUBLEBUF | SDL_HWSURFACE );
}

static void quit( void )
{
    exit( 0 );
}

void handle_key_event( void )
{
    int ret;
    SDL_Event e;

    SDL_PumpEvents();
    ret = SDL_PeepEvents( &e, 1, SDL_GETEVENT, SDL_KEYEVENTMASK );

    if ( ret < 1 )
        return;

    //process key event
    if ( e.type == SDL_KEYDOWN )
    {
        switch ( e.key.keysym.sym )
        {
        case SDLK_1:
            input_button_down( JOYPAD_SELECT );
            break;

        case SDLK_2:
            input_button_down( JOYPAD_START );
            break;

        case SDLK_a:
            input_button_down( JOYPAD_LEFT );
            break;

        case SDLK_d:
            input_button_down( JOYPAD_RIGHT );
            break;

        case SDLK_w:
            input_button_down( JOYPAD_UP );
            break;

        case SDLK_s:
            input_button_down( JOYPAD_DOWN );
            break;

        case SDLK_j:
            input_button_down( JOYPAD_B );
            break;

        case SDLK_k:
            input_button_down( JOYPAD_A );
            break;

        case SDLK_ESCAPE:
            running = !running;
            break;

        case SDLK_SPACE:
            pause = 1;
            break;

        default:
            break;
        }
    }

    if ( e.type == SDL_KEYUP )
    {
        switch ( e.key.keysym.sym )
        {
        case SDLK_1:
            input_button_up( JOYPAD_SELECT );
            break;

        case SDLK_2:
            input_button_up( JOYPAD_START );
            break;

        case SDLK_a:
            input_button_up( JOYPAD_LEFT );
            break;

        case SDLK_d:
            input_button_up( JOYPAD_RIGHT );
            break;

        case SDLK_w:
            input_button_up( JOYPAD_UP );
            break;


        case SDLK_s:
            input_button_up( JOYPAD_DOWN );
            break;

        case SDLK_j:
            input_button_up( JOYPAD_B );
            break;

        case SDLK_k:
            input_button_up( JOYPAD_A );
            break;

        default:
            break;
        }
    }
}

void powerup( void )
{
    regs.FLAGS = 0x34;
    regs.SP = 0xfd;
}

int main ( void )
{
    SDL_Event e;
    int scan_line = 0;

    LOG_INIT();
    powerup();

    ines_loadrom( "c.nes" );

    cpu_reset();
    ppu_init();
    init_sdl();

    input_init( handle_key_event );

    running = 1;

    while ( running )
    {
        if ( !pause )
        {
            cpu_execute_translate( CYCLE_PER_SCANLINE );
            ppu_render_scanline( CYCLE_PER_SCANLINE * 3 );
        }



        scan_line = ( scan_line + 1 ) % 240;
        if ( scan_line == 0 )
        {
            scale2( screen, screen2 );
            SDL_LockSurface( sdl_screen );

            SDL_memcpy( sdl_screen->pixels, screen2, 480 * 512 * 2 );

            SDL_UnlockSurface( sdl_screen );

            SDL_Flip( sdl_screen );
        }

        SDL_PumpEvents();
        if ( SDL_PeepEvents( &e, 1, SDL_GETEVENT, SDL_QUITMASK ) > 0 )
        {
            quit();
        }

        if ( pause )
        {
            if ( SDL_PeepEvents( &e, 1, SDL_GETEVENT, SDL_KEYEVENTMASK ) > 0 )
            {
                if ( e.type == SDL_KEYDOWN )
                {
                    if ( e.key.keysym.sym == SDLK_SPACE )
                    {
                        pause = 0;
                    }
                }
            }
        }

    }

    ines_unloadrom();

    LOG_CLOSE();

    return 0;
}
