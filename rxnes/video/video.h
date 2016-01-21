//video.h

#ifndef _VIDEO_H
#define _VIDEO_H

#include "core/types.h"

struct video_context;

struct video_context *video_context_create(
	char *name,
	void(*init)(int width, int height, void *user),
	void(*render)(void *buffer, int buffer_width, int buffer_height, int bpp),
	void(*uninit)()
);

void video_context_destroy(struct video_context *c);
void video_context_make_current(struct video_context *c);
void video_init(int width, int height, void *user);
void video_unint();
void video_render_frame(void *buffer, int buffer_width, int buffer_height, int bpp);

// util
void video_rgb565_2_rgba888(u16 sc[][256], u32 sc2[][256], u8 alpha, int flip_y);

#ifdef __cplusplus
#define video_init_block_impl(n, i, r, u)	\
static const char video_name[] = #n;		\
static struct n##_init_block				\
{											\
	video_context *_context;				\
	n##_init_block()						\
	{										\
		video_context *_context = video_context_create(#n, i, r, u);\
		video_context_make_current(_context);						\
	}																\
																	\
	~n##_init_block()												\
	{																\
		video_context_make_current(NULL);							\
		video_context_destroy(_context);							\
	}																\
}n##_video_block;
#endif

#endif //_VIDEO_H