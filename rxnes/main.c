#include "ines.h"
#include "cpu.h"
#include "ppu.h"
#include "log.h"
#include "input.h"
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <d3d9.h>
#include <tchar.h>
#include <dinput.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dinput8.lib")

#define CYCLE_PER_SCANLINE 114
#define RX_NES_WND_CLASS _T("RXNesWnd")

static u16 screen2[480][512];

LPDIRECT3D9 g_pD3D;
LPDIRECT3DDEVICE9 g_pD3DDevice;
LPDIRECT3DSURFACE9 g_pBackbuffer;
LPDIRECTINPUT g_pDInput;
LPDIRECTINPUTDEVICE g_pDInputDevice;

HINSTANCE g_hInstance;

int running;
int pause;

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

static void quit( void )
{
	running = 0;
}

void handle_key_event( void )
{
	#define KEYDOWN(name, key) (name[key] & 0x80)

	char fDIKeyboardState[256];
	HRESULT hr;
	// get the keyboard state
	hr = IDirectInputDevice_GetDeviceState(
		g_pDInputDevice,
		sizeof(fDIKeyboardState),
		(LPVOID)&fDIKeyboardState
		);
	if (FAILED(hr))
	{
		IDirectInputDevice_Acquire(g_pDInputDevice);
		return;
	}

	struct KeyJOYPAD
	{
		int key;
		int joypad;
	};

	struct KeyJOYPAD keyJoypad[] =
	{
		DIK_1, JOYPAD_SELECT,
		DIK_2, JOYPAD_START, 
		DIK_W, JOYPAD_UP,
		DIK_A, JOYPAD_LEFT,
		DIK_S, JOYPAD_DOWN,
		DIK_D, JOYPAD_RIGHT,
		DIK_K, JOYPAD_A,
		DIK_J, JOYPAD_B
	};

	//process key event
	for (int i = 0; i < sizeof(keyJoypad) / sizeof(keyJoypad[0]); ++i)
	{
		if (KEYDOWN(fDIKeyboardState, keyJoypad[i].key))
		{
			input_button_down(keyJoypad[i].joypad);
		}
		else
		{
			input_button_up(keyJoypad[i].joypad);
		}
	}
}

void powerup( void )
{
	regs.FLAGS = 0x34;
	regs.SP = 0xfd;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	switch (nMessage)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		break;
	}

	return DefWindowProc(hWnd, nMessage, wParam, lParam);
}

BOOL InitDX(HWND hWnd)
{
	g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!g_pD3D)
	{
		return 0;
	}

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.Windowed = TRUE;
	d3dpp.hDeviceWindow = hWnd;
	d3dpp.BackBufferCount = 1;
	d3dpp.BackBufferFormat = D3DFMT_R5G6B5;
	d3dpp.EnableAutoDepthStencil = TRUE;//TRUE;//FALSE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

	IDirect3D9_CreateDevice(g_pD3D, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &g_pD3DDevice);
	if (!g_pD3DDevice)
	{
		return FALSE;
	}

	IDirect3DDevice9_GetBackBuffer(g_pD3DDevice, 0, 0, D3DBACKBUFFER_TYPE_MONO, &g_pBackbuffer);
	if (!g_pBackbuffer)
	{
		return FALSE;
	}

	DirectInput8Create(g_hInstance, DIRECTINPUT_VERSION, &IID_IDirectInput8, &g_pDInput, NULL);
	if (!g_pDInput)
	{
		return FALSE;
	}

	IDirectInput_CreateDevice(g_pDInput, &GUID_SysKeyboard, &g_pDInputDevice, NULL);
	if (!g_pDInputDevice)
	{
		return FALSE;
	}


	HRESULT hr = IDirectInputDevice_SetDataFormat(g_pDInputDevice, &c_dfDIKeyboard);
	if (FAILED(hr))
	{
		return FALSE;
	}

	hr = IDirectInputDevice_SetCooperativeLevel(g_pDInputDevice, hWnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
	if (FAILED(hr))
	{
		return FALSE;
	}

	hr = IDirectInputDevice_Acquire(g_pDInputDevice);
	if (FAILED(hr))
	{
		return FALSE;
	}

	return TRUE;
}

void DestroyDX()
{
	if (g_pD3D)
	{
		IDirect3D9_Release(g_pD3D);
		g_pD3D = NULL;
	}

	if (g_pD3DDevice)
	{
		IDirect3DDevice9_Release(g_pD3DDevice);
		g_pD3DDevice = NULL;
	}

	if (g_pDInput)
	{
		IDirectInput_Release(g_pDInput);
		g_pDInput = NULL;
	}
	if (g_pDInputDevice)
	{
		IDirectInputDevice_Release(g_pDInputDevice);
		g_pDInputDevice = NULL;
	}

}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpszCmdLine, int nCmdShow)
{
	g_hInstance = hInstance;

	WNDCLASS wndClass;
	ZeroMemory(&wndClass, sizeof(wndClass));
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hInstance = hInstance;
	wndClass.lpfnWndProc = WndProc;
	wndClass.style = CS_VREDRAW | CS_HREDRAW;
	wndClass.lpszClassName = RX_NES_WND_CLASS;

	if (!RegisterClass(&wndClass))
	{
		return EXIT_FAILURE;
	}

	HWND hWnd = CreateWindow(RX_NES_WND_CLASS, _T("RxNes"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 512, 480, NULL, NULL, hInstance, NULL);
	if (!hWnd)
	{
		return EXIT_FAILURE;
	}

	ShowWindow(hWnd, SW_SHOW);
	
	InitDX(hWnd);


	int scan_line = 0;

	LOG_INIT();
	powerup();

	ines_loadrom("Super Mario Bros. (E).nes");

	cpu_reset();
	ppu_init();

	input_init(handle_key_event);

	running = 1;

	while (running)
	{
		MSG msg;
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				running = 0;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (!pause)
			{
				cpu_execute_translate(CYCLE_PER_SCANLINE);
				ppu_render_scanline(CYCLE_PER_SCANLINE * 3);
				scan_line = (scan_line + 1) % 240;
				if (scan_line == 0)
				{
					scale2(screen, screen2);

					// copy
					IDirect3DDevice9_Clear(g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
					D3DLOCKED_RECT lockedRect;
					HRESULT hr = IDirect3DSurface9_LockRect(g_pBackbuffer, &lockedRect, NULL, D3DLOCK_DISCARD);

					if (SUCCEEDED(hr))
					{
						CopyMemory(lockedRect.pBits, screen2, 480 * 512 * 2);
						IDirect3DSurface9_UnlockRect(g_pBackbuffer);
					}
					IDirect3DDevice9_Present(g_pD3DDevice, NULL, NULL, NULL, NULL);
				}
			}
		}
	}

	DestroyDX();

	ines_unloadrom();

	LOG_CLOSE();

	return EXIT_SUCCESS;
}
