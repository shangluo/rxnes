#define RXNES_RENDER_DX9

#include "ines.h"
#include "mapper.h"
#include "cpu.h"
#include "ppu.h"
#include "log.h"
#include "input.h"
#include <stdio.h>
#include <time.h>
#include <windows.h>
#ifdef RXNES_RENDER_DX9
#include <d3d9.h>
#endif
#include <dinput.h>
#include <tchar.h>
#include "resource.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dinput8.lib")

#define CYCLE_PER_SCANLINE 114
#define RX_NES_WND_CLASS _T("RXNesWnd")
#define RX_NES_NAMETABLE_WND_CLASS _T("RXNesNameTableWnd")

static u16 screen2[480][512];

static int rom_loaded;

#ifdef RXNES_RENDER_DX9
LPDIRECT3D9 g_pD3D;
LPDIRECT3DDEVICE9 g_pD3DDevice;
LPDIRECT3DSURFACE9 g_pOffscreenBuffer;
LPDIRECT3DSURFACE9 g_pBackbuffer;
#else
LPVOID g_pScreenBuf;
#endif
LPDIRECTINPUT g_pDInput;
LPDIRECTINPUTDEVICE g_pDInputDevice;

HINSTANCE g_hInstance;
HWND g_hWndNameTable;
HWND g_hWndCpuDebugger;

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

LRESULT CALLBACK NameTableWndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	static int i = 0;
	static HDC hDC;
	static HDC hBackgroundDC[6] = { NULL };
	static HBITMAP hOldBitmap[6] = { NULL };
	static HBITMAP hBitmap[6] = { NULL };
	static VOID *pBits[6] = { NULL };
	static BITMAPINFO bmi = {0};
	PAINTSTRUCT ps;
	switch (nMessage)
	{
	case WM_CREATE:
		hDC = GetDC(hWnd);
		bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
		bmi.bmiHeader.biWidth = 256;
		bmi.bmiHeader.biHeight = -240;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 16;
		bmi.bmiHeader.biCompression = BI_RGB; 
		for (i = 0; i < 6; ++i)
		{
			if (i > 3)
			{
				bmi.bmiHeader.biWidth = 128;
				bmi.bmiHeader.biHeight = -128;
			}

			hBackgroundDC[i] = CreateCompatibleDC(hDC);
			hBitmap[i] = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, &pBits[i], NULL, 0);
			hOldBitmap[i] = SelectObject(hBackgroundDC[i], hBitmap[i]);
		}

		ReleaseDC(hWnd, hDC);

		SetTimer(hWnd, 1, 30, NULL);

	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);
		for (i = 0; i < 4; ++i)
		{
			BitBlt(hDC, 256 * (i & 0x1), 240 * ( i >> 1), 256 , 240, hBackgroundDC[i], 0, 0, SRCCOPY);
		}

		for (i = 0; i < 2; ++i)
		{
			//BitBlt(hDC, 256 * i, 480, 256, 256, hBackgroundDC[i + 4], 0, 0, SRCCOPY);
			StretchBlt(hDC, 256 * i, 480, 256, 256, hBackgroundDC[i + 4], 0, 0, 128, 128, SRCCOPY);
		}
		EndPaint(hWnd, &ps);
		break;

	case WM_TIMER:
		for (i = 0; i < 4; ++i)
		{
			ppu_fill_name_table(pBits[i], i);
		}
		for (i = 0; i < 2; ++i)
		{
			ppu_fill_pattern_table(pBits[i + 4], i);
		}
		InvalidateRect(hWnd, NULL, FALSE);
		break;

	case WM_DESTROY:
		for (i = 0; i < 6; ++i)
		{
			SelectObject(hBackgroundDC[i], hOldBitmap[i]);
			DeleteObject(hBitmap[i]);
			DeleteDC(hBackgroundDC[i]);
		}
		g_hWndNameTable = NULL;

		break;
		 
	default:
		break;
	}

	return DefWindowProc(hWnd, nMessage, wParam, lParam);
}

HWND CreateNameTableWnd()
{
	WNDCLASS wndClass;

	if (!GetClassInfo(g_hInstance, RX_NES_NAMETABLE_WND_CLASS, &wndClass))
	{
		ZeroMemory(&wndClass, sizeof(wndClass));
		wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndClass.hInstance = g_hInstance;
		wndClass.lpfnWndProc = NameTableWndProc;
		wndClass.style = CS_VREDRAW | CS_HREDRAW;
		wndClass.lpszClassName = RX_NES_NAMETABLE_WND_CLASS;

		if (!RegisterClass(&wndClass))
		{
			return NULL;
		}
	}

	RECT rt;
	SetRect(&rt, 0, 0, 512, 736);
	AdjustWindowRect(&rt, WS_OVERLAPPEDWINDOW, FALSE);

	HWND hWnd = CreateWindow(RX_NES_NAMETABLE_WND_CLASS, _T("Name Tables & Pattern Table"), WS_OVERLAPPEDWINDOW & (~(WS_SIZEBOX | WS_MAXIMIZEBOX)), CW_USEDEFAULT, CW_USEDEFAULT, rt.right - rt.left, rt.bottom - rt.top, NULL, NULL, g_hInstance, NULL);
	if (!hWnd)
	{
		return NULL;
	}

	ShowWindow(hWnd, SW_SHOW);

	return hWnd;
}

INT_PTR CALLBACK CPUDebuggerCallback(HWND hDlg, UINT nMessage, WPARAM wParam, LPARAM lParam)
{

#define WM_SCROLL_TO_ADDR WM_USER + 1

	static HWND hWndInstructionList;
	static char *disasmblyText;
	unsigned int disasmblyTextLen = 256;
	if (nMessage == WM_INITDIALOG)
	{
		u32 addr = 0x8000;

		if (rom_loaded)
		{
			//memcpy(&addr, &memory[0xfffc], 2);
		}

		u8 bytes;
		char buf[128];
		char itemText[128];
		char opcode[128];
		char tmp[3];
		int i;

		while (addr <= 0xffff)
		{
			bytes = cpu_disassemble_intruction((u16)addr, buf, 128);
			memset(opcode, 0, 128);
			for (i = 0; i < bytes; ++i)
			{
				if (i != 0)
				{
					strcat(opcode, " ");
				}
				sprintf(tmp, "%02X", memory[addr + i]);
				strcat(opcode, tmp);
			}
			sprintf(itemText, " %X: %s \t%s\r\n", addr, opcode, buf);
			addr += bytes;
			if (!disasmblyText)
			{
				disasmblyText = (char *)malloc(disasmblyTextLen);
				ZeroMemory(disasmblyText, disasmblyTextLen);
			}
			
			while (strlen(disasmblyText) + strlen(itemText) >= disasmblyTextLen)
			{
				disasmblyTextLen *= 2;
				disasmblyText = (char *)realloc(disasmblyText, disasmblyTextLen);
				if (!disasmblyText)
				{
					DebugBreak();
				}
			}

			strcat(disasmblyText, itemText);

		}
		
		SendDlgItemMessageA(hDlg, IDC_EDIT_DISASSMEBLY, WM_SETTEXT, 0, (LPARAM)disasmblyText);
		SetTimer(hDlg, 1, 30, NULL);
		return TRUE;
	}
	else if (nMessage == WM_TIMER)
	{
		TCHAR szText[10];

	#define MODIFY_REG_VALUE(reg_name) \
		_stprintf(szText, _T("%02X"), regs.##reg_name); \
		SetDlgItemText(hDlg, IDC_EDIT_REG_##reg_name, szText);

		MODIFY_REG_VALUE(A);
		MODIFY_REG_VALUE(X);
		MODIFY_REG_VALUE(Y);
		MODIFY_REG_VALUE(SP);
		MODIFY_REG_VALUE(PC);
	#undef MODIFY_REG_VALUE

	#define MODIFY_CHECKBOX(reg_name) \
		SendDlgItemMessage(hDlg, IDC_CHECK_REG_FLAG_##reg_name, BM_SETCHECK, regs.SR.##reg_name == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
	
		MODIFY_CHECKBOX(C);
		MODIFY_CHECKBOX(Z);
		MODIFY_CHECKBOX(I);
		MODIFY_CHECKBOX(D);
		MODIFY_CHECKBOX(B);
		MODIFY_CHECKBOX(R);
		MODIFY_CHECKBOX(V);
		MODIFY_CHECKBOX(N);

	#undef MODIFY_CHECKBOX

		PostMessage(hDlg, WM_SCROLL_TO_ADDR, regs.PC, 0);
		return TRUE;
	}
	else if (nMessage == WM_SCROLL_TO_ADDR)
	{
		int lineCount = 0;
		u32 addr = (u32)wParam;
		char szText[128];

		sprintf(szText, "%04X:", addr);

		char *prev = disasmblyText;
		char *ptr = strstr(disasmblyText, szText);
		if (ptr)
		{
			while (prev < ptr && prev)
			{
				prev = strstr(prev, "\r\n");
				if (prev)
				{
					++lineCount;
					prev += 2;
				}
			}

			SendDlgItemMessage(hDlg, IDC_EDIT_DISASSMEBLY, WM_KEYDOWN, VK_HOME, 0);
			SendDlgItemMessage(hDlg, IDC_EDIT_DISASSMEBLY, EM_LINESCROLL, 0, lineCount - 1);
		}
	}
	else if (nMessage == WM_CLOSE)
	{
		if (disasmblyText)
		{
			free(disasmblyText);
			disasmblyText = NULL;
		}

		disasmblyTextLen = 0;

		g_hWndCpuDebugger = NULL;
		DestroyWindow(hDlg);
		return TRUE;
	}
	else if (nMessage == WM_COMMAND)
	{
		WORD wCommandID = LOWORD(wParam);
		if (wCommandID == IDC_BUTTON_SEEK_TO)
		{
			
			u32 addr;
			char szText[128];
			GetDlgItemTextA(hDlg, IDC_EDIT_PC, szText, 128);
			sscanf(szText, "%X", &addr);
			
			PostMessage(hDlg, WM_SCROLL_TO_ADDR, addr, 0);
		}
		else if (wCommandID == IDC_BUTTON_RESET)
		{
			powerup();

			cpu_reset();
			ppu_init();
		}
	}

	#undef WM_SCROLL_TO_ADDR

	return FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
#ifndef RXNES_RENDER_DX9
	static HDC hDC = NULL;
	static HDC hBackgroundDC;
	static HBITMAP hOldBitmap;
	static HBITMAP hBitmap;
	static BITMAPINFO bmi;
	PAINTSTRUCT ps;
	RECT rt;
#endif // !RX

	WORD wCommondID = 0;
	switch (nMessage)
	{
	case WM_CREATE:
#ifndef RXNES_RENDER_DX9
		ZeroMemory(&bmi, sizeof(bmi));
		hDC = GetDC(hWnd);
		bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
		bmi.bmiHeader.biWidth = 256;
		bmi.bmiHeader.biHeight = -240;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 16;
		bmi.bmiHeader.biCompression = BI_RGB;
		hBackgroundDC = CreateCompatibleDC(hDC);
		hBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, &g_pScreenBuf, NULL, 0);
		hOldBitmap = SelectObject(hBackgroundDC, hBitmap);

		ReleaseDC(hWnd, hDC);
#endif
		break;

#ifndef RXNES_RENDER_DX9
	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &rt);
		StretchBlt(hDC, 0, 0, rt.right, rt.bottom, hBackgroundDC, 0, 0, 256, 240, SRCCOPY);
		EndPaint(hWnd, &ps);
		break;
#endif

	case WM_DESTROY:
#ifndef RXNES_RENDER_DX9
		SelectObject(hDC, hOldBitmap);
		DeleteObject(hBitmap);
		DeleteDC(hBackgroundDC);
#endif
		running = 0;
		PostQuitMessage(0);
		break;

	case WM_COMMAND:
		wCommondID = LOWORD(wParam);
		if (wCommondID == ID_FILE_OPEN)
		{
			if (rom_loaded)
			{
				ines_unloadrom();
			}

			char fileName[MAX_PATH] = "";

			OPENFILENAMEA  ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFilter = "Nes File(*.nes)\0*.NES\0\0\0";
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;

			if (GetOpenFileNameA(&ofn))
			{
				DWORD err = CommDlgExtendedError();
				ines_loadrom(fileName);
			
				rom_loaded = 1;

				powerup();

				cpu_reset();
				ppu_init();

				input_init(handle_key_event);
			}
		}
		else if (wCommondID == ID_DEBUG_NAMETABLES)
		{
			if (g_hWndNameTable)
			{
				ShowWindow(g_hWndNameTable, SW_SHOWNORMAL);
			}
			else
			{
				g_hWndNameTable = CreateNameTableWnd();
			}
		}
		else if (wCommondID == ID_DEBUG_CPUDEBUGGER)
		{
			if (!g_hWndCpuDebugger)
			{
				g_hWndCpuDebugger = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_DEBUGGER), NULL, CPUDebuggerCallback);
			}
			ShowWindow(g_hWndCpuDebugger, SW_SHOWNORMAL);
		}

	default:
		break;
	}

	return DefWindowProc(hWnd, nMessage, wParam, lParam);
}

BOOL InitDX(HWND hWnd)
{
#ifdef RXNES_RENDER_DX9
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

	IDirect3DDevice9_CreateOffscreenPlainSurface(g_pD3DDevice, 512, 480, d3dpp.BackBufferFormat, D3DPOOL_SYSTEMMEM, &g_pOffscreenBuffer, NULL);
	if (!g_pOffscreenBuffer)
	{
		return FALSE;
	}

	IDirect3DDevice9_GetBackBuffer(g_pD3DDevice, 0, 0, D3DBACKBUFFER_TYPE_MONO, &g_pBackbuffer);
	if (!g_pBackbuffer)
	{
		return FALSE;
	}
#endif

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
#ifdef RXNES_RENDER_DX9
	if (g_pBackbuffer)
	{
		IDirect3DSurface9_Release(g_pBackbuffer);
		g_pBackbuffer = NULL;
	}

	if (g_pOffscreenBuffer)
	{
		IDirect3DSurface9_Release(g_pOffscreenBuffer);
		g_pOffscreenBuffer = NULL;
	}

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
#endif

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
	wndClass.hbrBackground = GetStockObject(BLACK_BRUSH);
	wndClass.lpszMenuName = MAKEINTRESOURCE(IDR_MENU_MAIN);

	if (!RegisterClass(&wndClass))
	{
		return EXIT_FAILURE;
	}

	RECT rt;
	SetRect(&rt, 0, 0, 512, 480);
	AdjustWindowRect(&rt, WS_OVERLAPPEDWINDOW, TRUE);

	HWND hWnd = CreateWindow(RX_NES_WND_CLASS, _T("RxNes"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rt.right - rt.left, rt.bottom - rt.top, NULL, NULL, hInstance, NULL);
	if (!hWnd)
	{
		return EXIT_FAILURE;
	}


	ShowWindow(hWnd, SW_SHOW);
	
	InitDX(hWnd);


	int scan_line = 0;
	rom_loaded = 0;
	running = 1;

	LOG_INIT();
	mapper_init();

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
			if (!pause && rom_loaded && running)
			{
				for (int i = 0; i < 3; ++i)
					cpu_execute_translate(CYCLE_PER_SCANLINE);
				ppu_render_scanline(CYCLE_PER_SCANLINE * 3);
				//scan_line = (scan_line + 1) % 240;
				extern u16 cur_scanline;
				scan_line = cur_scanline;
				if (scan_line == 0)
				{
#ifdef RXNES_RENDER_DX9
					scale2(screen, screen2);


					// copy
					IDirect3DDevice9_Clear(g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
					D3DLOCKED_RECT lockedRect;
					HRESULT hr = IDirect3DSurface9_LockRect(g_pOffscreenBuffer, &lockedRect, NULL, D3DLOCK_DISCARD);

					if (SUCCEEDED(hr))
					{
						CopyMemory(lockedRect.pBits, screen2, 480 * 512 * sizeof(u16));
						IDirect3DSurface9_UnlockRect(g_pOffscreenBuffer);
					}
					IDirect3DDevice9_UpdateSurface(g_pD3DDevice, g_pOffscreenBuffer, NULL, g_pBackbuffer, NULL);
					IDirect3DDevice9_Present(g_pD3DDevice, NULL, NULL, NULL, NULL);
#else
					if (g_pScreenBuf)
					{
						memcpy(g_pScreenBuf, screen, 256 * 240 * sizeof(u16));
						InvalidateRect(hWnd, NULL, FALSE);
					}
#endif
				}
			}
		}
	}

	DestroyDX();

	ines_unloadrom();

	LOG_CLOSE();

	return EXIT_SUCCESS;
}
