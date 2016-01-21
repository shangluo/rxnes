#include "core/ines.h"
#include "core/cpu.h"
#include "core/emulator.h"
#include "core/papu.h"
#include "core/ppu.h"
#include "core/input.h"
#include <stdio.h>
#include <time.h>
#include <locale.h>
#include <windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <Shlobj.h>
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

#ifdef RX_NES_RENDER_GDI
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

static void quit(void)
{
	running = 0;
}

void InputHandler(void)
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

INT_PTR CALLBACK ChoosePalleteDlgProc(HWND hDlg, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	int i;
	static u8 *pIndex = NULL;
	static RECT rtPaintArea;
	HDC hDC;
	PAINTSTRUCT ps;
	static HBRUSH hOldBrush;
	static HPEN hOldPen;
	static int nItemWidth;
	static int nItemHeight;
	static int x, y;
	static u16 c;
	static int nCurrentSelect;
	POINT pt;
	int r, g, b;

#define ROW_COUNT 4
#define COLUMN_COUNT 16 
	if (nMessage == WM_INITDIALOG)
	{
		pIndex = (u8 *)lParam;
		GetClientRect(hDlg, &rtPaintArea);

		nItemWidth = rtPaintArea.right / COLUMN_COUNT;
		nItemHeight = (rtPaintArea.bottom - 50) / ROW_COUNT;

		SetWindowText(hDlg, _T("Choose Pallete"));

		return TRUE;
	}
	else if (nMessage == WM_COMMAND)
	{
		switch (LOWORD(wParam))
		{
		case IDOK:
			if (pIndex)
			{
				*pIndex = nCurrentSelect;
			}
			EndDialog(hDlg, IDOK);
			break;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			break;
		}
	}
	else if (nMessage == WM_PAINT)
	{
		hDC = BeginPaint(hDlg, &ps);
		hOldBrush = SelectObject(hDC, GetStockObject(DC_BRUSH));
		hOldPen = SelectObject(hDC, GetStockObject(DC_PEN));
		for (i = 0; i < 64; ++i)
		{
			x = (i % COLUMN_COUNT) * nItemWidth;
			y = i / COLUMN_COUNT * nItemHeight;
			c = ppu_get_pallete_color(i);
			b = (c & 0x1f) * 255 / 0x1f;
			g = ((c >> 5) & 0x3f) * 255 / 0x3f;
			r = ((c >> 11) & 0x1f) * 255 / 0x1f;
			SetDCPenColor(hDC, i == nCurrentSelect ? RGB(255, 255, 255) : RGB(0, 0, 0));
			SetDCBrushColor(hDC, RGB(r, g, b));
			Rectangle(hDC, x, y, x + nItemWidth, y + nItemHeight);
		}
		SelectObject(hDC, hOldBrush);
		SelectObject(hDC, hOldPen);
		EndPaint(hDlg, &ps);
	}
	else if (nMessage == WM_LBUTTONDOWN)
	{
		pt.x = LOWORD(lParam);
		pt.y = HIWORD(lParam);
		if (pt.y < rtPaintArea.bottom - 50)
		{
			nCurrentSelect = pt.y / nItemHeight * COLUMN_COUNT + pt.x / nItemWidth;
			InvalidateRect(hDlg, &rtPaintArea, TRUE);
		}
	}
	else if (nMessage == WM_DESTROY)
	{
	}
	return FALSE;

#undef ROW_COUNT
#undef COLUMN_COUNT
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

	case WM_LBUTTONDOWN:
		GetCursorPos(&ptCursor);
		ScreenToClient(hWnd, &ptCursor);

		// name table
		if (ptCursor.x >= 0 && ptCursor.x <= 512 && ptCursor.y >= 0 && ptCursor.y <= 480)
		{
			int index = (ptCursor.y / 256 << 1) | (ptCursor.x / 256);
			//_stprintf(szInfomation, _T("Name Table #%d"), index);
		}
		// pattern table
		else if (ptCursor.x >= 512 && ptCursor.x <= 1024 && ptCursor.y >= 0 && ptCursor.y <= 256)
		{
			int indexPattern = (ptCursor.x - 512) / 256;
			//_stprintf(szInfomation, _T("Pattern Table #%d"), indexPattern);
		}
		// pallete table
		else if (ptCursor.x >= 512 && ptCursor.x <= 1024 && ptCursor.y >= 256 && ptCursor.y <= 320)
		{
			int index = (ptCursor.y - 256) / 32 * 16 + (ptCursor.x - 512) / 32;
			//_stprintf(szInfomation, _T("Pallete Table With Index #%02x"), ppu_vram[0x3f00 + index]);
			/*static COLORREF customColors[16];
			memset(customColors, 0xff, sizeof(customColors));
			CHOOSECOLORA chooseColor;
			ZeroMemory(&chooseColor, sizeof(chooseColor));
			chooseColor.lStructSize = sizeof(chooseColor);
			chooseColor.hwndOwner = hWnd;
			chooseColor.Flags = CC_ANYCOLOR | CC_FULLOPEN;
			chooseColor.lpCustColors = customColors;
			if (ChooseColorA(&chooseColor))
			{
			#define MAKE_RGB_565(r, g, b) ((((int)(r / 256.0 * 0x1f)) << 11) | (((int)(g / 256.0 * 0x3f)) << 5) | (int)(b / 256.0 * 0x1f) )
			ppu_set_custom_pallete(ppu_vram[0x3f00 + index], MAKE_RGB_565(GetRValue(chooseColor.rgbResult), GetGValue(chooseColor.rgbResult), GetBValue(chooseColor.rgbResult)));
			#undef MAKE_RGB_565
			}
			*/
			u8 pallete = -1;
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_POPUP), hWnd, ChoosePalleteDlgProc, (LPARAM)&pallete) == IDOK && pallete != -1)
			{
				ppu_vram[0x3f00 + index] = pallete;
			}
		}
		else
		{
			//_tcscpy(szInfomation, _T(""));
		}
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
			if ((BYTE *)ptr >(BYTE *)pvAudioPtr1 + dwAudioBytes1)
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

struct RomCompParam
{
	HWND hListView;
	int iSubItem;
};

int CALLBACK RomCompareFunc(LPARAM nIndex1, LPARAM nIndex2, LPARAM lParam)
{
	HWND hLB;
	int iSubItem;
	TCHAR szBuffer1[MAX_PATH];
	TCHAR szBuffer2[MAX_PATH];
	LVITEM lvItem1, lvItem2;
	struct RomCompParam *param = (struct RomCompParam *)lParam;
	hLB = param->hListView;
	iSubItem = param->iSubItem;

	lvItem1.iSubItem = iSubItem;
	lvItem2.iSubItem = iSubItem;
	lvItem1.cchTextMax = lvItem2.cchTextMax = MAX_PATH;
	lvItem1.pszText = szBuffer1;
	lvItem2.pszText = szBuffer2;

	SendMessage(hLB, LVM_GETITEMTEXT, nIndex1, &lvItem1);
	SendMessage(hLB, LVM_GETITEMTEXT, nIndex2, &lvItem2);

	return _tcscmp(szBuffer1, szBuffer2);
}

INT_PTR CALLBACK RomListDlgProc(HWND hDlg, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	int i, j;
	LVCOLUMN lvColumn;
	LVITEM lvItem;
	TCHAR *szLabels[] =
	{
		_T("File"),
		_T("Size"),
		_T("Mapper")
	};
	static char *pszParam = NULL;
	static TCHAR path[MAX_PATH] = _T("\0");
	RECT rc;
	BROWSEINFO info;
	static PIDLIST_ABSOLUTE pidl = NULL;
	WIN32_FIND_DATA data;
	HANDLE hFindFile;
	TCHAR tmp[10];

	TCHAR path2[MAX_PATH];
	char path3[MAX_PATH];

	if (nMessage == WM_INITDIALOG)
	{
		pszParam = (TCHAR *)lParam;

		GetClientRect(GetDlgItem(hDlg, IDC_LIST_ROMS), &rc);

		SendDlgItemMessage(hDlg, IDC_LIST_ROMS, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

		ZeroMemory(&lvColumn, sizeof(lvColumn));
		lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;
		lvColumn.cx = rc.right / ARRAY_LENGTH(szLabels);

		// refresh
		for (i = 0; i < ARRAY_LENGTH(szLabels); ++i)
		{
			lvColumn.pszText = szLabels[i];
			SendDlgItemMessage(hDlg, IDC_LIST_ROMS, LVM_INSERTCOLUMN, i, &lvColumn);
		}
		return TRUE;
	}
	else if (nMessage == WM_COMMAND)
	{
		switch (LOWORD(wParam))
		{
		case IDOK:
			i = SendDlgItemMessage(hDlg, IDC_LIST_ROMS, LVM_GETSELECTIONMARK, 0, 0);
			ZeroMemory(&lvItem, sizeof(lvItem));
			lvItem.mask = LVIF_PARAM;
			lvItem.iItem = i;
			SendDlgItemMessage(hDlg, IDC_LIST_ROMS, LVM_GETITEM, 0, &lvItem);
			strcpy(pszParam, lvItem.lParam);
			EndDialog(hDlg, IDOK);
			break;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			break;
		case IDC_BUTTON_SELECT_FOLDER:		
			ZeroMemory(&info, sizeof(info));
			info.hwndOwner = hDlg;
			info.pszDisplayName = path;
			info.lpszTitle = _T("Select Rom Folder");

			if (pidl = SHBrowseForFolder(&info))
			{
				SHGetPathFromIDList(pidl, path);
				PostMessage(hDlg, WM_COMMAND, IDC_BUTTON_RESCAN, 0);
			}
			break;
		case IDC_BUTTON_RESCAN:
			SendDlgItemMessage(hDlg, IDC_LIST_ROMS, LVM_DELETEALLITEMS, 0, 0);

			_stprintf(path2, _T("%s\\%s"), path, _T("*.nes"));
			hFindFile = FindFirstFile(path2, &data);

			setlocale(LC_ALL, "zh-CN");

		if (hFindFile != INVALID_HANDLE_VALUE)
		{
			do
			{
				ZeroMemory(&lvItem, sizeof(lvItem));

					_stprintf(path2, _T("%s\\%s"), path, data.cFileName);
					wcstombs(path3, path2, MAX_PATH);

					lvItem.iItem = 0;
					lvItem.iSubItem = 0;
					lvItem.lParam = strdup(path3);
					lvItem.mask |= LVIF_PARAM;
					SendDlgItemMessage(hDlg, IDC_LIST_ROMS, LVM_INSERTITEM, 0, &lvItem);

					lvItem.mask &= ~LVIF_PARAM;
					lvItem.lParam = NULL;
					for (j = 0; j < ARRAY_LENGTH(szLabels); ++j)
					{
						switch (j)
						{
						case 0:
							lvItem.pszText = data.cFileName;
							break;
						case 1:
							lvItem.pszText = _tcscat(_itot(data.nFileSizeLow / 1024, tmp, 10), _T("KB"));
							break;
						case 2:
							lvItem.pszText = _itot(ines_get_mapper(path3), tmp, 10);
							break;
						}
						lvItem.iSubItem = j;
						SendDlgItemMessage(hDlg, IDC_LIST_ROMS, LVM_SETITEMTEXT, 0, &lvItem);
					}
				} while (FindNextFile(hFindFile, &data));
				FindClose(hFindFile);
			}
			break;
		}
	}
	else if (nMessage == WM_NOTIFY)
	{
		if (LOWORD(wParam) == IDC_LIST_ROMS)
		{
			LPNMLISTVIEW lstV = (LPNMLISTVIEW)lParam;
			if (((LPNMHDR)lParam)->code == LVN_COLUMNCLICK)
			{
				struct RomCompParam param;
				param.hListView = GetDlgItem(hDlg, IDC_LIST_ROMS);
				param.iSubItem = lstV->iSubItem;
				SendDlgItemMessage(hDlg, IDC_LIST_ROMS, LVM_SORTITEMSEX, &param, RomCompareFunc);
			}
			else if (((LPNMHDR)lParam)->code == LVN_DELETEITEM)
			{
				if (lstV->lParam)
				{
					free(lstV->lParam);
				}
			}
		}
	}
	return FALSE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	static HDC hDC = NULL;
#if defined (RX_NES_RENDER_GDI) || defined (RX_NES_RENDER_GL)
#ifdef RX_NES_RENDER_GDI
	static HDC hBackgroundDC;
	static HBITMAP hOldBitmap;
	static HBITMAP hBitmap;
	static LPBITMAPINFOHEADER pBMIHeader = NULL;
	static LPDWORD pColorMask = NULL;

	PAINTSTRUCT ps;
	RECT rt;
#endif

#ifdef RX_NES_RENDER_GL
	static POINT ptLast;
	static BOOL bLButtonDown;
	static BOOL bRButtonDown;
	static BOOL bMButtonDown;
#endif
#endif // !RX

	static char fileName[MAX_PATH] = "";
	WORD wCommondID = 0;
	switch (nMessage)
	{
	case WM_CREATE:
		hDC = GetDC(hWnd);
#ifdef RX_NES_RENDER_GDI
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

#endif
		ReleaseDC(hWnd, hDC);
		break;

#ifdef RX_NES_RENDER_GDI
	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &rt);
		StretchBlt(hDC, 0, 0, rt.right, rt.bottom, hBackgroundDC, 0, 0, 256, 240, SRCCOPY);
		EndPaint(hWnd, &ps);
		break;
#endif

//#ifdef RX_NES_RENDER_GL
//	case WM_LBUTTONDOWN:
//		ptLast.x = GET_X_LPARAM(lParam);
//		ptLast.y = GET_Y_LPARAM(lParam);
//		bLButtonDown = TRUE;
//		SetCapture(hWnd);
//		break;
//	case WM_LBUTTONUP:
//		bLButtonDown = FALSE;
//		ReleaseCapture();
//		break;
//
//	case WM_MBUTTONDOWN:
//		ptLast.x = GET_X_LPARAM(lParam);
//		ptLast.y = GET_Y_LPARAM(lParam);
//		bMButtonDown = TRUE;
//		SetCursor(LoadCursor(NULL, MAKEINTRESOURCE(IDC_HAND)));
//		SetCapture(hWnd);
//		break;
//	case WM_MBUTTONUP:
//		bMButtonDown = FALSE;
//		SetCursor(LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW)));
//		ReleaseCapture();
//		break;
//
//
//	case WM_RBUTTONDOWN:
//		ptLast.x = GET_X_LPARAM(lParam);
//		ptLast.y = GET_Y_LPARAM(lParam);
//		SetCursor(LoadCursor(NULL, MAKEINTRESOURCE(IDC_CROSS)));
//		bRButtonDown = TRUE;
//		SetCapture(hWnd);
//		break;
//	case WM_RBUTTONUP:
//		bRButtonDown = FALSE;
//		SetCursor(LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW)));
//		ReleaseCapture();
//		break;
//
//	case WM_MOUSEMOVE:
//		//if (bLButtonDown)
//		{
//			int x = GET_X_LPARAM(lParam);
//			int y = GET_Y_LPARAM(lParam);
//
//			float deltaX = x - ptLast.x;
//			if (bLButtonDown)
//			{
//				g_AngleY += deltaX;
//			}
//			else if (bRButtonDown)
//			{
//				g_zDinstance += deltaX;
//			}
//			else if (bMButtonDown)
//			{
//				g_XOffset += deltaX;
//			}
//
//			float deltaY = y - ptLast.y;
//			if (bLButtonDown)
//			{
//				g_AngleX += deltaY;
//			}
//			else if (bMButtonDown)
//			{
//				g_YOffset -= deltaY;
//			}
//
//			ptLast.x = x;
//			ptLast.y = y;
//		}
//		break;
//#endif

	case WM_DESTROY:
#ifdef RX_NES_RENDER_GDI
		SelectObject(hBackgroundDC, hOldBitmap);
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
		else if (wCommondID == ID_FILE_OPENROMLIST)
		{
			char szPath[MAX_PATH];
			if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_ROMLIST), hWnd, RomListDlgProc, szPath) == IDOK)
			{
				emulator_load(szPath);
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
		else if (wCommondID == ID_STATE_SAVE)
		{
			emulator_save_state();
		}
		else if (wCommondID == ID_STATE_LOAD)
		{
			emulator_load_state();
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

	emulator_init(512, 480, hWnd);
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
#ifdef RX_NES_RENDER_GDI
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
