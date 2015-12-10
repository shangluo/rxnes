// mapper.c

#include <stdlib.h>
#include <string.h>
#include "mapper.h"

#define MAPPER_HANDLER_VECTOR_SIZE 256
static mapper_handler *mapper_handler_vector[MAPPER_HANDLER_VECTOR_SIZE];
static int mapper_current;
static mapper_handler *mapper_get_handler(int index);

#define mapper_init_n(i)																								\
		do																												\
 		{																												\
			extern mapper_handler mapper_handler##i;																\
			mapper_handler_vector[i] = &mapper_handler##i;																\
		} while (0);

void mapper_init()
{	
	memset(mapper_handler_vector, 0, sizeof(mapper_handler_vector));
	mapper_init_n(1);
	mapper_init_n(2);
	mapper_init_n(3);
	mapper_init_n(4);
	mapper_init_n(23);
}

void mapper_uninit()
{
}

void mapper_reset()
{
	mapper_handler *handler = mapper_get_handler(mapper_current);
	if (handler)
	{
		handler->reset();
	}
}

void mapper_make_current(int mapper)
{
	mapper_current = mapper;
}

void mapper_write(u16 addr, u8 data)
{
	mapper_handler *handler = mapper_get_handler(mapper_current);
	if (handler)
	{
		handler->write(addr, data);
	}
}

static void mapper_register_handler(int index, mapper_handler *handler)
{
	if (index < MAPPER_HANDLER_VECTOR_SIZE)
	{
		mapper_handler_vector[index] = handler;
	}
}

mapper_handler *mapper_get_handler(int index)
{
	mapper_handler *handler = NULL;
	if (index < MAPPER_HANDLER_VECTOR_SIZE)
	{
		handler = mapper_handler_vector[index];
	}

	return handler;
}