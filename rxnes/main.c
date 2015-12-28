//#define RXNES_RENDER_DX9

#include "ines.h"
#include "cpu.h"
#include "emulator.h"
#include "papu.h"
#include "ppu.h"
#include "input.h"
#include <stdio.h>
#include <time.h>
#include <windows.h>
#ifdef RXNES_RENDER_DX9
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")
#endif
#include <dinput.h>
#include <dsound.h>
#include <tchar.h>
#include "resource.h"

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dsound.lib")

#define RX_NES_WND_CLASS _T("RXNESWnd")
#define RX_NES_PPU_STATUS_WND_CLASS _T("RXNESPPUStatus_Wnd")
#define RX_NES_APU_STATUS_WND_CLASS _T("RXNESAPUStatus_Wnd")

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

LPDIRECTSOUND8 g_pDSound;
LPDIRECTSOUNDBUFFER g_pDSoundBuffer;

HINSTANCE g_hInstance;
HWND g_hWndPPUStatus;
HWND g_hWndCpuDebugger;
HWND g_hWndApuStatus;

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

void InputHandler( void )
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

LRESULT CALLBACK PPUStatusWndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	static int i = 0;
	static HDC hDC;
	static HDC hBackgroundDC[7] = { NULL };
	static HBITMAP hOldBitmap[7] = { NULL };
	static HBITMAP hBitmap[7] = { NULL };
	static VOID *pBits[7] = { NULL };
	static LPBITMAPINFOHEADER pBMIHeader = NULL;
	static LPDWORD pColorMask = NULL;
	static POINT ptCursor;
	static TCHAR szInfomation[256];
	static RECT rtInfomation;
	static HBRUSH hBlackBrush;
	PAINTSTRUCT ps;
	switch (nMessage)
	{
	case WM_CREATE:
		hDC = GetDC(hWnd);
		pBMIHeader = (LPBITMAPINFOHEADER)malloc(sizeof(*pBMIHeader) + 3 * sizeof(DWORD));
		ZeroMemory(pBMIHeader, sizeof(*pBMIHeader));
		pBMIHeader->biSize = sizeof(*pBMIHeader);
		pBMIHeader->biWidth = 256;
		pBMIHeader->biHeight = -240;
		pBMIHeader->biPlanes = 1;
		pBMIHeader->biBitCount = 16;
		pBMIHeader->biCompression = BI_BITFIELDS;
		pColorMask = (LPDWORD)(pBMIHeader + 1);
		*pColorMask++ = 0x0000f800;
		*pColorMask++ = 0x000007e0;
		*pColorMask++ = 0x0000001f;
		for (i = 0; i < 7; ++i)
		{
			if (i > 3 && i <= 5)
			{
				pBMIHeader->biWidth = 128;
				pBMIHeader->biHeight = -128;
			}
			else if (i > 5)
			{
				pBMIHeader->biWidth = 16;
				pBMIHeader->biHeight = -2;
			}

			hBackgroundDC[i] = CreateCompatibleDC(hDC);
			hBitmap[i] = CreateDIBSection(hDC, (const BITMAPINFO *)pBMIHeader, DIB_RGB_COLORS, &pBits[i], NULL, 0);
			hOldBitmap[i] = SelectObject(hBackgroundDC[i], hBitmap[i]);
		}

		free(pBMIHeader);
		pBMIHeader = NULL;
		ReleaseDC(hWnd, hDC);

		hBlackBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
		SetRect(&rtInfomation, 512, 320, 1024, 480);

		SetTimer(hWnd, 1, 30, NULL);

	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);

		// name table
		for (i = 0; i < 4; ++i)
		{
			BitBlt(hDC, 256 * (i & 0x1), 240 * (i >> 1), 256, 240, hBackgroundDC[i], 0, 0, SRCCOPY);
		}

		// pattern table
		for (i = 4; i < 6; ++i)
		{
			StretchBlt(hDC, 256 * (i - 2), 0, 256, 256, hBackgroundDC[i], 0, 0, 128, 128, SRCCOPY);
		}

		// pallete
		StretchBlt(hDC, 256 * 2, 256, 512, 64, hBackgroundDC[6], 0, 0, 16, 2, SRCCOPY);

		// Infomation
		FillRect(hDC, &rtInfomation, hBlackBrush);
		SetBkMode(hDC, TRANSPARENT);
		SetTextColor(hDC, RGB(0xff, 0xff, 0xff));
		DrawText(hDC, szInfomation, -1, &rtInfomation, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

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

		ppu_fill_pallete_table(pBits[6]);

		// 
		GetCursorPos(&ptCursor);
		ScreenToClient(hWnd, &ptCursor);

		// name table
		if (ptCursor.x >= 0 && ptCursor.x <= 512 && ptCursor.y >= 0 && ptCursor.y <= 480)
		{
			int index = (ptCursor.y / 256 << 1) | (ptCursor.x / 256);
			_stprintf(szInfomation, _T("Name Table #%d"), index);
		}
		// pattern table
		else if (ptCursor.x >= 512 && ptCursor.x <= 1024 && ptCursor.y >= 0 && ptCursor.y <= 256)
		{
			int indexPattern = (ptCursor.x - 512) / 256;
			_stprintf(szInfomation, _T("Pattern Table #%d"), indexPattern);
		}
		// pallete table
		else if (ptCursor.x >= 512 && ptCursor.x <= 1024 && ptCursor.y >= 256 && ptCursor.y <= 320)
		{
			int index = (ptCursor.y - 256) / 32 * 16 + (ptCursor.x - 512) / 32;
			_stprintf(szInfomation, _T("Pallete Table With Index #%02x"), ppu_vram[0x3f00 + index]);
		}
		else
		{
			_tcscpy(szInfomation, _T(""));
		}

		InvalidateRect(hWnd, NULL, FALSE);
		break;

	case WM_DESTROY:
		for (i = 0; i < 7; ++i)
		{
			SelectObject(hBackgroundDC[i], hOldBitmap[i]);
			DeleteObject(hBitmap[i]);
			DeleteDC(hBackgroundDC[i]);
		}
		DeleteObject(hBlackBrush);
		g_hWndPPUStatus = NULL;

		break;

	default:
		break;
	}

	return DefWindowProc(hWnd, nMessage, wParam, lParam);
}

LRESULT CALLBACK APUStatusWndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	static int i, j, k;
	static HDC hDC;
	PAINTSTRUCT ps;
	static u8 soundBuffer1[PAPU_CHANNEL_COUNT];
	static u8 soundBuffer2[PAPU_CHANNEL_COUNT];
	static int step = 0xff;
	static u16 sample;
	static RECT rtWindow, rtUpdate;
	static HBRUSH hBlackBrush;
	static HDC hBackgroundDC = NULL;
	static HBITMAP hBitmap;
	static HBITMAP hOldBitmap;
	static int nTimeStamp;
	BOOL bInitial = FALSE;
	static COLORREF colorChannel[PAPU_CHANNEL_COUNT] =
	{
		RGB(255, 0, 0), //Pulse 1
		RGB(0, 255, 0), //Pulse 2
		RGB(0, 0, 255), //Triangle
		RGB(0, 255, 255), //Noise
		RGB(255, 255, 255), //DMC
	};
	static HPEN hColoredPens[PAPU_CHANNEL_COUNT];
	static HPEN hOldPen = NULL;
	int x1, y1, x2, y2;
	switch (nMessage)
	{
	case WM_CREATE:
		hDC = GetDC(hWnd);
		GetClientRect(hWnd, &rtWindow);
		hBlackBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
		hBackgroundDC = CreateCompatibleDC(hDC);
		hBitmap = CreateCompatibleBitmap(hDC, rtWindow.right, rtWindow.bottom);
		hOldBitmap = SelectObject(hBackgroundDC, hBitmap);
		ReleaseDC(hWnd, hDC);

		for (i = 0; i < PAPU_CHANNEL_COUNT; ++i)
		{
			hColoredPens[i] = CreatePen(PS_SOLID, 1, colorChannel[i]);
		}
		hOldPen = SelectObject(hBackgroundDC, hColoredPens[0]);
		SetTimer(hWnd, 1, 30, NULL);

	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);
		ScrollDC(hBackgroundDC, /*-1000 / 512 **/ -30, 0, NULL, NULL, NULL, &rtUpdate);
		FillRect(hBackgroundDC, &rtUpdate, hBlackBrush);
		for (i = 0; i < 4; ++i)
		{
			bInitial = TRUE;
			soundBuffer1[i] = papu_get_sound_channel(i);

			//for (int j = 0; j < 1; j += step)
			//{
			//	x = i /*% 2 * 256*/ + j / step * 2;
				x1 = rtUpdate.left;

			//	sample = 0;
			//	for (k = 0; k < step; ++k)
			//	{
			//		sample += soundBuffer[j + k];
			//	}
			//	sample /= step;

				y1 = (int)(i /*/ 2*/ * (rtWindow.bottom / 4) + rtWindow.bottom / 8 + (*(char *)&soundBuffer2[i] / 256.0) * 128 * (256 / 32));
				x2 = rtUpdate.right;
				y2 = (int)(i /*/ 2*/ * (rtWindow.bottom / 4) + rtWindow.bottom / 8 + (*(char *)&soundBuffer1[i] / 256.0) * 128 * (256 / 32));
			//	//SetPixelV(hBackgroundDC, x, y, colorChannel[i]);
			//	if (bInitial)
				{
					SelectObject(hBackgroundDC, hColoredPens[i]);
					MoveToEx(hBackgroundDC, x1, y1, NULL);
					bInitial = FALSE;
				}
				//else
				{
					LineTo(hBackgroundDC, x2, y2);
				}
			//}
			memcpy(soundBuffer2, soundBuffer1, sizeof(soundBuffer1));
		}

		BitBlt(hDC, 0, 0, rtWindow.right, rtWindow.bottom, hBackgroundDC, 0, 0, SRCCOPY);

		EndPaint(hWnd, &ps);
		break;

	case WM_TIMER:
		nTimeStamp += 30;
		InvalidateRect(hWnd, NULL, FALSE);
		break;

	case WM_DESTROY:
		DeleteObject(hBlackBrush);
		SelectObject(hBackgroundDC, hOldPen);
		SelectObject(hBackgroundDC, hOldBitmap);
		DeleteObject(hBitmap);
		for (i = 0; i < PAPU_CHANNEL_COUNT; ++i)
		{
			DeleteObject(hColoredPens[i]);
		}
		DeleteDC(hBackgroundDC);
		g_hWndApuStatus = NULL;
		break;

	default:
		break;
	}

	return DefWindowProc(hWnd, nMessage, wParam, lParam);
}

HWND RXCreateWindow(LPCTSTR szClassName, LPCTSTR szWindowTitle, UINT nWidowWidth, UINT nWindowHeight, WNDPROC wndProc)
{
	WNDCLASS wndClass;

	if (!GetClassInfo(g_hInstance, szClassName, &wndClass))
	{
		ZeroMemory(&wndClass, sizeof(wndClass));
		wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndClass.hInstance = g_hInstance;
		wndClass.lpfnWndProc = wndProc;
		wndClass.style = CS_VREDRAW | CS_HREDRAW;
		wndClass.lpszClassName = szClassName;

		if (!RegisterClass(&wndClass))
		{
			return NULL;
		}
	}

	RECT rt;
	SetRect(&rt, 0, 0, nWidowWidth, nWindowHeight);
	AdjustWindowRect(&rt, WS_OVERLAPPEDWINDOW, FALSE);
	
	HWND hWnd = CreateWindow(szClassName, szWindowTitle, WS_OVERLAPPEDWINDOW & (~(WS_SIZEBOX | WS_MAXIMIZEBOX)), CW_USEDEFAULT, CW_USEDEFAULT, rt.right - rt.left, rt.bottom - rt.top, NULL, NULL, g_hInstance, NULL);
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
		_stprintf(szText, _T("%02X"), cpu_read_register_value(#reg_name)); \
		SetDlgItemText(hDlg, IDC_EDIT_REG_##reg_name, szText);

		MODIFY_REG_VALUE(A);
		MODIFY_REG_VALUE(X);
		MODIFY_REG_VALUE(Y);
		MODIFY_REG_VALUE(SP);
		MODIFY_REG_VALUE(PC);
	#undef MODIFY_REG_VALUE

	#define MODIFY_CHECKBOX(reg_name) \
		SendDlgItemMessage(hDlg, IDC_CHECK_REG_FLAG_##reg_name, BM_SETCHECK, cpu_read_register_value("SR."#reg_name) == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
	
		MODIFY_CHECKBOX(C);
		MODIFY_CHECKBOX(Z);
		MODIFY_CHECKBOX(I);
		MODIFY_CHECKBOX(D);
		MODIFY_CHECKBOX(B);
		MODIFY_CHECKBOX(R);
		MODIFY_CHECKBOX(V);
		MODIFY_CHECKBOX(N);

	#undef MODIFY_CHECKBOX

		//PostMessage(hDlg, WM_SCROLL_TO_ADDR, regs.PC, 0);
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
			emulator_reset();
		}
	}

	#undef WM_SCROLL_TO_ADDR

	return FALSE;
}
static int write_cursor;
static int buffer_size;
static int playing = 0;
static int buffer_delay = 1;
void APUBufferCallback(double *pBuffer, int nSize)
{
	if (g_pDSoundBuffer)
	{
		LPVOID pvAudioPtr1;
		DWORD  dwAudioBytes1;
		LPVOID pvAudioPtr2;
		DWORD dwAudioBytes2;
		short *ptr;
		HRESULT hr;
		int i;
		int targetSample;
		int nSizeToWrite = nSize * sizeof(short);

		//hr = IDirectSoundBuffer8_Stop(g_pDSoundBuffer);
		//if (FAILED(hr))
		//{
		//	return;
		//}

		hr = IDirectSoundBuffer8_Lock(g_pDSoundBuffer, write_cursor, nSizeToWrite, &pvAudioPtr1, &dwAudioBytes1, &pvAudioPtr2, &dwAudioBytes2, 0);
		if (FAILED(hr))
		{
			return;
		}

		write_cursor = (write_cursor + nSizeToWrite) % buffer_size;

		ptr = pvAudioPtr1;
		for (i = 0; i < nSize; ++i)
		{
			targetSample = (int)(pBuffer[i] * USHRT_MAX + SHRT_MIN);
			if (targetSample > SHRT_MAX)
			{
				targetSample = SHRT_MAX;
			}
			if (targetSample < SHRT_MIN)
			{
				targetSample = SHRT_MIN;
			}
			*ptr = targetSample;
			++ptr;
			if ((BYTE *)ptr > (BYTE *)pvAudioPtr1 + dwAudioBytes1)
			{
				ptr = pvAudioPtr2;
			}
		}

		hr = IDirectSoundBuffer8_Unlock(g_pDSoundBuffer, pvAudioPtr1, dwAudioBytes1, pvAudioPtr2, dwAudioBytes2);
		if (FAILED(hr))
		{
			return;
		}

		if (buffer_delay > 0)
		{
			--buffer_delay;
			return;
		}
		if (!playing)
		{
			hr = IDirectSoundBuffer8_Play(g_pDSoundBuffer, 0, 0, DSBPLAY_LOOPING);
			if (FAILED(hr))
			{
				return;
			}
			playing = 1;
		}
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
#ifndef RXNES_RENDER_DX9
	static HDC hDC = NULL;
	static HDC hBackgroundDC;
	static HBITMAP hOldBitmap;
	static HBITMAP hBitmap;
	static LPBITMAPINFOHEADER pBMIHeader = NULL;
	static LPDWORD pColorMask = NULL;
	static char fileName[MAX_PATH] = "";

	PAINTSTRUCT ps;
	RECT rt;
#endif // !RX

	WORD wCommondID = 0;
	switch (nMessage)
	{
	case WM_CREATE:
#ifndef RXNES_RENDER_DX9
		pBMIHeader = (LPBITMAPINFOHEADER)malloc(sizeof(*pBMIHeader) + 3 * sizeof(DWORD));
		ZeroMemory(pBMIHeader, sizeof(*pBMIHeader));
		pBMIHeader->biSize = sizeof(*pBMIHeader);
		pBMIHeader->biWidth = 256;
		pBMIHeader->biHeight = -240;
		pBMIHeader->biPlanes = 1;
		pBMIHeader->biBitCount = 16;
		pBMIHeader->biCompression = BI_BITFIELDS;
		pColorMask = (LPDWORD)(pBMIHeader + 1);
		*pColorMask++ = 0x0000f800;
		*pColorMask++ = 0x000007e0;
		*pColorMask++ = 0x0000001f;
		hBackgroundDC = CreateCompatibleDC(hDC);
		hBitmap = CreateDIBSection(hDC, (const BITMAPINFO *)pBMIHeader, DIB_RGB_COLORS, &g_pScreenBuf, NULL, 0);
		hOldBitmap = SelectObject(hBackgroundDC, hBitmap);

		free(pBMIHeader);
		pBMIHeader = NULL;

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
				emulator_load(fileName);
				rom_loaded = 1;
			}
		}
		else if (wCommondID == ID_DEBUG_NAMETABLES)
		{
			if (g_hWndPPUStatus)
			{
				ShowWindow(g_hWndPPUStatus, SW_SHOWNORMAL);
			}
			else
			{
				g_hWndPPUStatus = RXCreateWindow(RX_NES_PPU_STATUS_WND_CLASS, _T("PPU Status"), 256 * 4, 480, PPUStatusWndProc);
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
		else if (wCommondID == ID_DEBUG_APUSTATUS)
		{
			if (g_hWndApuStatus)
			{
				ShowWindow(g_hWndApuStatus, SW_SHOWNORMAL);
			}
			else
			{
				g_hWndApuStatus = RXCreateWindow(RX_NES_APU_STATUS_WND_CLASS, _T("APU Status"), 256 * 2, 256 * 2, APUStatusWndProc);
			}
		}
		break;

	case WM_DROPFILES:
		if (DragQueryFileA((HDROP)wParam, 0, fileName, MAX_PATH))
		{
			DragFinish((HDROP)wParam);
			emulator_load(fileName);
			rom_loaded = 1;
		}
		break;

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

	IDirect3DDevice9_CreateOffscreenPlainSurface(g_pD3DDevice, 256, 240, d3dpp.BackBufferFormat, D3DPOOL_DEFAULT, &g_pOffscreenBuffer, NULL);
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

	hr = DirectSoundCreate8(&DSDEVID_DefaultPlayback, &g_pDSound, NULL);
	if (FAILED(hr))
	{
		return FALSE;
	}

	hr = IDirectSound8_SetCooperativeLevel(g_pDSound, hWnd, DSSCL_PRIORITY);
	if (FAILED(hr))
	{
		return FALSE;
	}

	WAVEFORMATEX wfx;
	ZeroMemory(&wfx, sizeof(wfx));
	wfx.cbSize = sizeof(wfx);
	wfx.nChannels = 1;
	wfx.nSamplesPerSec = EMULATOR_DEF_SOUND_SAMLE_RATE;
	wfx.wBitsPerSample = sizeof(u16) * 8;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	DSBUFFERDESC dsbd;
	ZeroMemory(&dsbd, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_GLOBALFOCUS;
	buffer_size = dsbd.dwBufferBytes = EMULATOR_DEF_SOUND_SAMLE_RATE * EMULATOR_DEF_SOUND_DURATION * sizeof(short) / 1000 * 10;
	dsbd.lpwfxFormat = &wfx;
	dsbd.guid3DAlgorithm = GUID_NULL;

	hr = IDirectSound8_CreateSoundBuffer(g_pDSound, &dsbd, &g_pDSoundBuffer, NULL);
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
	
	if (g_pDSoundBuffer)
	{
		IDirectSoundBuffer_Release(g_pDSoundBuffer);
		g_pDSoundBuffer = NULL;
	}
	if (g_pDSound)
	{
		IDirectSound_Release(g_pDSound);
		g_pDSound = NULL;
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

	HWND hWnd = CreateWindowEx(WS_EX_ACCEPTFILES, RX_NES_WND_CLASS, _T("rxNES"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rt.right - rt.left, rt.bottom - rt.top, NULL, NULL, hInstance, NULL);
	if (!hWnd)
	{
		return EXIT_FAILURE;
	}


	ShowWindow(hWnd, SW_SHOW);
	
	InitDX(hWnd);


	int scan_line = 0;
	rom_loaded = 0;
	running = 1;

	emulator_init();
	emulator_set_sound_callback(APUBufferCallback);
	emulator_set_input_handler(InputHandler);

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
				emulator_run_loop();
				//scan_line = (scan_line + 1) % 240;
				extern u16 ppu_current_scanline;
				scan_line = ppu_current_scanline;
				if (scan_line == 0)
				{
#ifdef RXNES_RENDER_DX9
					//scale2(screen, screen2);


					// copy
					IDirect3DDevice9_Clear(g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
					D3DLOCKED_RECT lockedRect;
					HRESULT hr = IDirect3DSurface9_LockRect(g_pOffscreenBuffer, &lockedRect, NULL, D3DLOCK_DISCARD);

					if (SUCCEEDED(hr))
					{
						CopyMemory(lockedRect.pBits, ppu_screen_buffer, 256 * 240 * sizeof(u16));
						IDirect3DSurface9_UnlockRect(g_pOffscreenBuffer);
					}
					//hr = IDirect3DDevice9_UpdateSurface(g_pD3DDevice, g_pOffscreenBuffer, NULL, g_pBackbuffer, NULL);
					hr = IDirect3DDevice9_StretchRect(g_pD3DDevice, g_pOffscreenBuffer, NULL, g_pBackbuffer, NULL, D3DTEXF_POINT);
					IDirect3DDevice9_Present(g_pD3DDevice, NULL, NULL, NULL, NULL);
#else
					if (g_pScreenBuf)
					{
						memcpy(g_pScreenBuf, ppu_screen_buffer, sizeof(ppu_screen_buffer));
						InvalidateRect(hWnd, NULL, FALSE);
					}
#endif
					static int frameCount;
					static LARGE_INTEGER last;
					static LARGE_INTEGER frequency;
					static TCHAR szWindowTitle[128];
					QueryPerformanceFrequency(&frequency);

					LARGE_INTEGER current;
					QueryPerformanceCounter(&current);

					if (current.QuadPart - last.QuadPart > frequency.QuadPart)
					{
						_stprintf(szWindowTitle, _T("rxNES FPS : %d"), frameCount);
						SetWindowText(hWnd, szWindowTitle);
						
						frameCount = 0;
						last = current;
					}
					++frameCount;
				}
			}
		}
	}

	DestroyDX();

	emulator_uninit();

	return EXIT_SUCCESS;
}
