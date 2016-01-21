extern "C"
{
#include "video\video.h"
}

#ifdef RX_NES_RENDER_DDRAW
#include <windows.h>
#include <ddraw.h>

#pragma comment(lib, "ddraw.lib")

#define SAFE_RELEASE(p) \
	if (p) p->Release(); p = NULL;

HWND g_hWnd;

LPDIRECTDRAW7  g_pDirectDraw;
LPDIRECTDRAWSURFACE7 g_pSurface;
LPDIRECTDRAWSURFACE7 g_pSurfaceDraw;
LPDIRECTDRAWCLIPPER g_pClipper;

void ddraw_init(int width, int height, void *user)
{
	HRESULT hr;
	g_hWnd = (HWND)user;
	hr = DirectDrawCreateEx(NULL, (LPVOID *)&g_pDirectDraw, IID_IDirectDraw7, NULL);

	hr = g_pDirectDraw->SetCooperativeLevel(g_hWnd, DDSCL_NORMAL);

	DDSURFACEDESC2 desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.dwFlags = /*DDSD_WIDTH | DDSD_HEIGHT*/DDSD_CAPS;
	//desc.dwWidth = width;
	//desc.dwHeight = height;
	desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	desc.dwSize = sizeof(desc);

	hr = g_pDirectDraw->CreateSurface(&desc, &g_pSurface, NULL);

	hr = g_pDirectDraw->CreateClipper(0, &g_pClipper, NULL);
	hr = g_pClipper->SetHWnd(0, g_hWnd);
	hr = g_pSurface->SetClipper(g_pClipper);

	ZeroMemory(&desc, sizeof(desc));
	desc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
	desc.dwWidth = width;
	desc.dwHeight = height;
	desc.dwSize = sizeof(desc);

	hr = g_pDirectDraw->CreateSurface(&desc, &g_pSurfaceDraw, NULL);
}

void ddraw_sw_stretch(u32 src[240][256], u32 dst[480][512])
{
	for (int y = 0; y < 256; ++y)
	{
		for (int x = 0; x < 240; ++x)
		{
			dst[x * 2][y * 2] = src[x][y];
			dst[x * 2 + 1][y * 2] = src[x][y];
			dst[x * 2][y * 2 + 1] = src[x][y];
			dst[x * 2 + 1][y * 2 + 1] = src[x][y];
		}
	}
}

void ddraw_render_frame(void *buffer, int buffer_width, int buffer_height, int bpp)
{
	HRESULT hr;
	DDSURFACEDESC2 desc = {sizeof(desc)};

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	ClientToScreen(g_hWnd, (LPPOINT)&rc);
	ClientToScreen(g_hWnd, (LPPOINT)&rc + 1);

	hr = g_pSurfaceDraw->Lock(NULL, &desc, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);

	static u32 buffer2[240][256];
	static u32 buffer3[480][512];
	video_rgb565_2_rgba888((u16(*)[256])buffer, buffer2, 255, 0);
	ddraw_sw_stretch(buffer2, buffer3);
	CopyMemory(desc.lpSurface, buffer3, sizeof(buffer3));

	g_pSurfaceDraw->Unlock(NULL);

	hr = g_pSurface->Blt(&rc, g_pSurfaceDraw, NULL, 0, NULL);
	g_pDirectDraw->WaitForVerticalBlank(1, NULL);
}

void ddraw_uninit()
{
	SAFE_RELEASE(g_pDirectDraw);
	SAFE_RELEASE(g_pSurface);
	SAFE_RELEASE(g_pSurfaceDraw);
	SAFE_RELEASE(g_pClipper);
}

video_init_block_impl(ddraw, ddraw_init, ddraw_render_frame, ddraw_uninit)

#endif