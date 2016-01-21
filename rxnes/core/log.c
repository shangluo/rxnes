#include "log.h"
#include <stdarg.h>

//close debug
//#define _NES_DEBUG

FILE *log_file = NULL;

#ifdef _NES_DEBUG

void LOG_TRACE( char *target, char *fmt, ... )
{
    va_list list;
    va_start( list ,fmt );

    fprintf( log_file, "%s : ", target );
    vfprintf( log_file, fmt, list );
    fputc( '\n', log_file );

    va_end( list );

    //fflush
    fflush( log_file );
}

#else
void LOG_TRACE( char *target, char *fmt, ... )
{
}

#endif
