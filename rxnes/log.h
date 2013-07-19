#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>

extern FILE *log_file;

//global log file pointer
#define LOG_FILE "neu.log"
#define LOG_INIT( )	log_file = fopen( LOG_FILE, "w" )
void LOG_TRACE( char * target, char * fmt, ... );
#define LOG_CLOSE( ) fclose( log_file )

#endif //_LOG_H
