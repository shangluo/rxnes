#Makefile for rxnes under unix-like with GNU make
#2013-6-11

CC = gcc
LD = gcc
RM = rm -rf

HEADERS = cpu.h   \
  	  ines.h  \
	  input.h \
	  log.h   \
	  ppu.h   \
	  types.h

SOURCES = cpu.c     \
          ines.c    \
	        input.c   \
          log.c     \
          main.c    \
          mapper1.c \
          papu.c    \
          ppu.c

OBJS = cpu.o     \
       ines.o    \
       input.o   \
       log.o     \
       main.o    \
       mapper1.o \
       papu.o    \
       ppu.o

TARGET = rxnes

CFLAGS = -c -O3 -Wall
LDFLAGS = -lSDL

all: rxnes obj 

obj: $(OBJS)
	$(CC) -o $(TARGET) $? $(LDFLAGS)
	
rxnes: $(SOURCES)
	$(CC) $(CFLAGS) $?

clean:
	$(RM) $(OBJS) $(TARGET) *.gch

