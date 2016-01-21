// mapper.c

#include <stdlib.h>
#include <string.h>
#include "ppu.h"
#include "mapper.h"
#include "cpu.h"
#include "ines.h"

#if defined(RX_NES_USE_LUA_MAPPERS) && defined(_WIN32)
	#include "../win/rxnes/lua/lua.h"
	#include "../win/rxnes/lua/lauxlib.h"
#ifdef _WIN32
		#ifdef _DEBUG
			#pragma comment(lib, "../rxnes/lua/lua_d.lib")
		#else
			#pragma comment(lib, "../rxnes/lua/lua.lib")
		#endif
	#endif
#endif

extern ines_rom *c_rom;
static int mapper_current;
#define MAPPER_GET_INDEX_FROM_ROM_ADDR(addr) (((addr & 0xf000) - 0x8000) / (8 * 1024))
#define MAPPER_GET_OFFSET_FROM_ROM_ADDR(addr) (((addr) - 0x8000) % (8 * 1024))
#define MAPPER_GET_PG_NO_FROM_ADDR(addr, base) (((addr) - (base)) / (8 * 1024))
static u8* mapper_address_map[4];

#ifndef RX_NES_USE_LUA_MAPPERS
#define MAPPER_HANDLER_VECTOR_SIZE 256
static mapper_handler *mapper_handler_vector[MAPPER_HANDLER_VECTOR_SIZE];
static mapper_handler *mapper_get_handler(int index);


#define mapper_init_n(i)																								\
		do																												\
 		{																												\
			extern mapper_handler mapper_handler##i;																	\
			mapper_handler_vector[i] = &mapper_handler##i;																\
		} while (0);

#else
#define MAPPER_LUA_WRITE_FUNC "write"
#define MAPPER_LUA_RESET_FUNC "reset"
#define MAPPER_LUA_A12_FUNC "a12"
#define MAPPER_LUA_RUNLOOP_FUNC "runloop"
static lua_State *mapper_lua_state;
#endif

static u8 mapper_irq_pending;
static void mapper_ppu_a12_listener();
static void mapper_set_irq_pending(u8 pending);

void mapper_init()
{	
#ifndef RX_NES_USE_LUA_MAPPERS
	memset(mapper_handler_vector, 0, sizeof(mapper_handler_vector));
	mapper_init_n(1);
	mapper_init_n(2);
	mapper_init_n(3);
	mapper_init_n(4);
	mapper_init_n(23);
	mapper_init_n(33);
#else
	mapper_lua_state = luaL_newstate();
#endif
}

void mapper_uninit()
{
#ifdef RX_NES_USE_LUA_MAPPERS
	if (mapper_lua_state)
	{
		lua_close(mapper_lua_state);
		mapper_lua_state = NULL;
	}
#endif
}

void mapper_reset()
{
	mapper_irq_pending = 0;

#ifndef RX_NES_USE_LUA_MAPPERS
	mapper_handler *handler = mapper_get_handler(mapper_current);
	if (handler && handler->reset)
	{
		handler->reset();
	}
#else
	if (mapper_lua_state)
	{
		lua_getglobal(mapper_lua_state, MAPPER_LUA_RESET_FUNC);
		if(lua_pcall(mapper_lua_state, 0, 0, 0))
		{
			DEBUG_PRINT("%s\n", lua_tostring(mapper_lua_state, -1));
		}
	}
#endif
}


#ifdef RX_NES_USE_LUA_MAPPERS
static int mapper_lua_switch_chr(lua_State *L)
{
	int cnt;
	u16 offset;
	int size_in_kb;
	int bank_no;

	cnt = lua_gettop(L);
	if (cnt == 3)
	{
		offset = (u16)lua_tointeger(L, 1);
		size_in_kb = (int)lua_tointeger(L, 2);
		bank_no = (int)lua_tointeger(L, 3);
		mapper_switch_chr(offset, size_in_kb, bank_no);
	}
	return 0;
}

static int mapper_lua_switch_prg(lua_State *L)
{
	int cnt; 
	u16 offset;
	int size_in_kb;
	int bank_no;
	
	cnt = lua_gettop(L);
	if (cnt == 3)
	{
		offset = (u16)lua_tointeger(L, 1);
		size_in_kb = (int)lua_tointeger(L, 2);
		bank_no = (int)lua_tointeger(L, 3);
		mapper_switch_prg(offset, size_in_kb, bank_no);
	}
	return 0;
}

static int mapper_lua_set_mirror_mode(lua_State *L)
{
	int cnt;
	u8 mode;

	cnt = lua_gettop(L);
	if (cnt == 1)
	{
		mode = (u8)lua_tointeger(L, 1);
		mapper_set_mirror_mode(mode);
	}
	return 0;
}

static void mapper_lua_ppu_a12_listener()
{
	if (mapper_lua_state)
	{
		lua_getglobal(mapper_lua_state, MAPPER_LUA_A12_FUNC);
		if (lua_pcall(mapper_lua_state, 0, 0, 0))
		{
			DEBUG_PRINT("%s\n", lua_tostring(mapper_lua_state, -1));
		}
	}
}

static int mapper_lua_set_irq_pending(lua_State *L)
{
	int cnt; 
	u8 irq_pending = 0;
	if (mapper_lua_state)
	{
		cnt = lua_gettop(L);
		if (cnt == 1)
		{
			irq_pending = (u8)lua_tointeger(mapper_lua_state, cnt);
			mapper_set_irq_pending(irq_pending);
		}
	}

	return 0;
}


static int mapper_lua_get_prg_cnt(lua_State *L)
{
	int cnt = 0;
	if (c_rom)
	{
		cnt = c_rom->prg_cnt;
		lua_pushinteger(L, cnt);
	}
	return 1;
}

static int mapper_lua_get_chr_cnt(lua_State *L)
{
	int cnt = 0;
	if (c_rom)
	{
		cnt = c_rom->chr_cnt;
		lua_pushinteger(L, cnt);
	}
	return 1;
}

static int mapper_lua_assert_irq(lua_State *L)
{
	mapper_assert_irq();
	return 0;
}


static void mapper_lua_reset_global()
{
	if (mapper_lua_state)
	{
		lua_register(mapper_lua_state, "mapper_switch_chr", mapper_lua_switch_chr);
		lua_register(mapper_lua_state, "mapper_switch_prg", mapper_lua_switch_prg);
		lua_register(mapper_lua_state, "mapper_set_mirror_mode", mapper_lua_set_mirror_mode);
		lua_register(mapper_lua_state, "mapper_get_prg_cnt", mapper_lua_get_prg_cnt);
		lua_register(mapper_lua_state, "mapper_get_chr_cnt", mapper_lua_get_chr_cnt);
		lua_register(mapper_lua_state, "mapper_assert_irq", mapper_lua_assert_irq);
		lua_register(mapper_lua_state, "mapper_set_irq_pending", mapper_lua_set_irq_pending);
	}
}

static void mapper_lua_load_mapper(int mapper)
{
	char prog_path[MAX_PATH];
	char mapper_file[MAX_PATH];
	if (mapper_lua_state)
	{
		lua_close(mapper_lua_state);
	}

#ifdef _WIN32
	GetModuleFileNameA(NULL, prog_path, MAX_PATH);
	char *p = strrchr(prog_path, '\\');
	if (p) *p = '\0';
#endif
	mapper_lua_state = luaL_newstate();
	sprintf(mapper_file, "%s/mappers/mapper%03d.lua", prog_path, mapper);
	if (luaL_dofile(mapper_lua_state, mapper_file))
	{
		// error
		DEBUG_PRINT("%s\n", lua_tostring(mapper_lua_state, -1));
		lua_close(mapper_lua_state);
		mapper_lua_state = NULL;
	}
	else
	{
		mapper_lua_reset_global();
	}
}
#endif

void mapper_make_current(int mapper)
{
	mapper_current = mapper;

	// set default memory map
	mapper_switch_prg(0x8000, 16, c_rom->prg_cnt - 2);
	mapper_switch_prg(0xc000, 16, c_rom->prg_cnt - 1);

	ppu_set_a12_listener(mapper == 4 || mapper == 194 ? mapper_ppu_a12_listener : NULL);

#ifdef RX_NES_USE_LUA_MAPPERS
	mapper_lua_load_mapper(mapper_current);
#endif
}

void mapper_write(u16 addr, u8 data)
{
#ifndef RX_NES_USE_LUA_MAPPERS
	mapper_handler *handler = mapper_get_handler(mapper_current);
	if (handler && handler->write)
	{
		handler->write(addr, data);
	}
#else
	if (mapper_lua_state)
	{
		lua_getglobal(mapper_lua_state, MAPPER_LUA_WRITE_FUNC);
		lua_pushinteger(mapper_lua_state, addr);
		lua_pushinteger(mapper_lua_state, data);
		if (lua_pcall(mapper_lua_state, 2, 0, 0))
		{
			DEBUG_PRINT("%s\n", lua_tostring(mapper_lua_state, -1));
		}
	}
#endif
}

#ifndef RX_NES_USE_LUA_MAPPERS
mapper_handler *mapper_get_handler(int index)
{
	mapper_handler *handler = NULL;
	if (index < MAPPER_HANDLER_VECTOR_SIZE)
	{
		handler = mapper_handler_vector[index];
	}

	return handler;
}
#endif

void mapper_switch_prg(u16 offset, int size_in_kb, int bank_no)
{
	int i, k;
	bank_no &= (c_rom->prg_cnt * 16 / size_in_kb - 1);
	//memcpy(memory + offset, c_rom->prg_banks + bank_no * 1024 * size_in_kb, 1024 * size_in_kb);
	
	k = MAPPER_GET_INDEX_FROM_ROM_ADDR(offset);
	for (i = 0; i < size_in_kb / 8; ++i)
	{
		mapper_address_map[k++] = c_rom->prg_banks + bank_no * 1024 * size_in_kb + ( 8 * 1024 * i);
	}
}

void mapper_switch_chr(u16 offset, int size_in_kb, int bank_no)
{
	bank_no &= (c_rom->chr_cnt * 8 / size_in_kb - 1);
	if (c_rom->chr_cnt > 0)
		memcpy(ppu_vram + offset, c_rom->chr_banks + bank_no * 1024 * size_in_kb, 1024 * size_in_kb);
	//ppu_build_tiles();
}

void mapper_set_mirror_mode(u8 mode)
{
	//DEBUG_PRINT("set mirror %d\n", mode);

	ppu_set_mirror_mode(mode);
}

static void mapper_ppu_a12_listener()
{
#ifndef RX_NES_USE_LUA_MAPPERS
	mapper_handler *handler = mapper_get_handler(mapper_current);
	if (handler && handler->a12)
	{
		handler->a12();
	}
#else
	mapper_lua_ppu_a12_listener();
#endif
}

void mapper_assert_irq()
{
	cpu_set_irq_pending();
}

static void mapper_set_irq_pending(u8 pending)
{
	u8 old_pending = mapper_irq_pending;
	mapper_irq_pending = pending;
	if (!mapper_irq_pending && old_pending)
	{
		// acknowledged
		mapper_assert_irq();
	}
}

void mapper_check_pending_irq()
{
#ifndef RX_NES_USE_LUA_MAPPERS
	mapper_handler *handler = mapper_get_handler(mapper_current);
	if (handler && handler->check_pending_irq)
	{
		handler->check_pending_irq();
	}
#else
	// run in native code
	if (mapper_irq_pending)
	{ 
		mapper_assert_irq();
	}
#endif
}

void mapper_run_loop(u32 cycles)
{
#ifdef RX_NES_USE_LUA_MAPPERS
	if (mapper_lua_state && (mapper_current == 85 || mapper_current == 65))
	{
		lua_getglobal(mapper_lua_state, MAPPER_LUA_RUNLOOP_FUNC);
		lua_pushinteger(mapper_lua_state, cycles);
		if (lua_pcall(mapper_lua_state, 1, 0, 0))
		{
			DEBUG_PRINT("%s\n", lua_tostring(mapper_lua_state, -1));
		}
	}
#endif
}

void mapper_read(u16 addr, u8 *buf, u16 len)
{
	int i;
	int k;
	u8 *addr_in_rom;

	if (addr >= 0x5000 && addr < 0x6000 && len == 1)
	{

		return;
	}
	
	for (i = 0; i < len; ++i)
	{
		k = MAPPER_GET_INDEX_FROM_ROM_ADDR(addr);
		addr_in_rom = mapper_address_map[k] + MAPPER_GET_OFFSET_FROM_ROM_ADDR(addr);
		*buf++ = *addr_in_rom;
		++addr;
	}
}

void mapper_state_write(FILE *fp)
{
	int i;
	int pg_no[4];

	for (i = 0; i < 4; ++i)
	{
		pg_no[i] = MAPPER_GET_PG_NO_FROM_ADDR(mapper_address_map[i], c_rom->prg_banks);
	}

	fwrite(pg_no, sizeof(pg_no), 1, fp);
}

void mapper_state_read(FILE *fp)
{
	int i;
	int pg_no[4];

	fread(pg_no, sizeof(pg_no), 1, fp);
	
	for (i = 0; i < 4; ++i)
	{
		mapper_address_map[i] = c_rom->prg_banks + pg_no[i] * 1024 * 8;
	}
}