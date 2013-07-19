#include "ines.h"
#include "cpu.h"
#include "ppu.h"
#include "log.h"
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

//android
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <fcntl.h>

#define CYCLE_PER_SCANLINE 114

static u16 screen2x[480][512];
static u32 screen2[480][512];

int b_running;
int b_pause;
int g_fbfd;
u8 *screen_buf;

u32 init_framebuffer( )
{
    u32 size;
    struct fb_var_screeninfo vinfo;

    g_fbfd = open( "/dev/graphics/fb0", O_RDWR );

    ioctl( g_fbfd, FBIOGET_VSCREENINFO, &vinfo );

    size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

    screen_buf = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, g_fbfd, 0 );

    return size;
}

void close_framebuffer( u32 size )
{
    munmap( screen_buf, size );
    close( g_fbfd );
}

void rgb565_to_rgba8888( u16 sc[][512], u32 sc2[][512] )
{
    u32 i = 0;
    u16 c1;
    u32 c2;
    u16 *ptr1 = (u16 *)sc;
    u32 *ptr2 = (u32 *)sc2;

    while ( i < 512 * 480 )
    {
        c2 = 0;
        c1 = *ptr1++;
        c2 = ( c1 & 0x1f ) | ( ( ( c1 >> 5 ) & 0x3f ) << 8 ) | ( (( c1 >> 11 ) & 0x1f ) << 16 );
        c2 |= 0xff000000;

        *ptr2++ = c2;

        ++i;
    }
}

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

void fill_test_screen(u32 sc[][512])
{
    //clear screen
    int i = 0;
    u8  *ptr1 = (u8 *)sc;
    u8  *ptr2 = screen_buf + 480 * 240;

    memset( screen_buf, 0x0, 854 * 480 * 4 );

    while ( i < 480 )
    {
        memcpy( ptr2, ptr1, 512 * 4 );
        ptr1 += 512 * 4;
        ptr2 += 480 * 4;

        ++i;
    }
}


//scale 2x and rotate 90, then blit to screen
void rotate_blit( )
{
    int i = 0;
    int j = 0;
    u32  *ptr1 = (u32 *)screen2;
    u32  *ptr2 = (u32 *)screen_buf;
    u32  *start;

    scale2( screen, screen2x );
    rgb565_to_rgba8888( screen2x, screen2 );

    //now blit to screen
    //fill_test_screen( screen2 );

    //skip 171 lines
    start = ptr2 + 480 * 171;

    //locate to last pixel
    while ( i < 480 * 512 )
    {
        if ( i % 512 == 0 )
        {
            ptr2 = start + ( 479 - j + 512 * 480 );
            --j;
        }

        *ptr2 = *ptr1++;

        ptr2 -= 480;
        ++i;
    }
}

static void quit( void )
{
    exit( 0 );
}


void powerup( void )
{
    regs.FLAGS = 0x34;
    regs.SP = 0xfd;
}

void handle_input( void )
{

}

int main ( void )
{
    int size;
    int scan_line = 0;


    //
    chdir( "/sdcard/rxnes" );
    LOG_INIT();
    powerup();

    input_init( handle_input );

    ines_loadrom( "mario.nes" );

    cpu_reset();
    ppu_init();

    b_running = 1;

    //
    size = init_framebuffer();

    while ( b_running )
    {
        if ( !b_pause )
        {
            cpu_execute_translate( CYCLE_PER_SCANLINE );
            ppu_render_scanline( CYCLE_PER_SCANLINE * 3 );
        }



        scan_line = ( scan_line + 1 ) % 240;
        if ( scan_line == 0 )
        {
            rotate_blit();
        }
    }

    ines_unloadrom();
    LOG_CLOSE();

    close_framebuffer( size );

    return 0;
}
