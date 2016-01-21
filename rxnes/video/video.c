#include "video.h"

struct video_context
{
	char *name;
	void(*init)(int width, int height, void *user);
	void(*render)(void *buffer, int buffer_width, int buffer_height, int bpp);
	void(*uninit)();
};

static struct video_context *video_context_current;
void video_context_make_current(struct video_context *c)
{
	video_context_current = c;
}

struct video_context *video_context_create(
	char *name,
	void(*init)(int width, int height, void *user),
	void(*render)(void *buffer, int buffer_width, int buffer_height, int bpp),
	void(*uninit)()
	)
{
	struct video_context *p = (struct video_context *)malloc(sizeof(struct video_context));
	p->name = name;
	p->init = init;
	p->render = render;
	p->uninit = uninit;

	return p;
}

void video_context_destroy(struct video_context *c)
{
	if (c)
	{
		free(c);
	}
}


void video_init(int width, int height, void *user)
{
	if (video_context_current)
	{
		video_context_current->init(width, height, user);
	}
}

void video_unint()
{
	if (video_context_current)
	{
		video_context_current->uninit();
	}
}

void video_render_frame(void *buffer, int buffer_width, int buffer_height, int bpp)
{
	if (video_context_current)
	{
		video_context_current->render(buffer, buffer_width, buffer_height, bpp);
	}
}

void video_rgb565_2_rgba888(u16 sc[][256], u32 sc2[][256], u8 alpha, int flip_y)
{
	int i, j;
	u16 c;
	u8 *ptr;

	for (i = 0; i < 240; ++i)
	{
		for (j = 0; j < 256; ++j)
		{
			c = sc[i][j];
			ptr = (u8 *)&sc2[flip_y ? (239 - i) : i][j];
			*ptr++ = (c & 0x1f) * 255.0f / 0x1f;
			*ptr++ = ((c >> 5) & 0x3f) * 255.0f / 0x3f;
			*ptr++ = ((c >> 11) & 0x1f) * 255.0f / 0x1f;
			*ptr++ = alpha;
		}
	}
}