#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>

#ifdef _NES_DEBUG

extern FILE *log_file;

//global log file pointer
#define LOG_FILE "neu.log"
#define LOG_INIT( )	log_file = fopen( LOG_FILE, "w" )
void LOG_TRACE( char * target, char * fmt, ... );
#define LOG_CLOSE( ) fclose( log_file )

#else

#define LOG_FILE 
#define LOG_INIT( )
void LOG_TRACE(char * target, char * fmt, ...);
#define LOG_CLOSE( )

#endif

#endif //_LOG_H
