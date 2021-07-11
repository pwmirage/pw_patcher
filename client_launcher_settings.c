/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdbool.h>
#include <assert.h>
#include <windows.h>
#include <commctrl.h>

#include "gui.h"
#include "common.h"
#include "game_config.h"

#define CHECKBOX_D3D8 1
#define CHECKBOX_CFONT 2
#define CHECKBOX_MAX 3

static BOOL CALLBACK
hwnd_set_font(HWND child, LPARAM font)
{
        SendMessage(child, WM_SETFONT, font, TRUE);
        return TRUE;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM data, LPARAM ldata);

static HWND g_settings_win;

void
show_settings_win(bool show)
{
        WNDCLASS wc = {0};
	RECT rect;
	int w = 235;
	int h = 180;

	GetClientRect(g_win, &rect);
	MapWindowPoints(g_win, NULL, (LPPOINT)&rect, 2);
	rect.left += (rect.right - rect.left - w) / 2;
	rect.top += (rect.bottom - rect.top - h) / 2;

	if (g_settings_win) {
		SetWindowPos(g_settings_win, HWND_TOP, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
		return;
	}

        wc.lpszClassName = "Game Settings";
        wc.hInstance = g_instance;
        wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
        wc.lpfnWndProc = WndProc;
        wc.hCursor = LoadCursor(0, IDC_ARROW);

        RegisterClass(&wc);
        CreateWindow(wc.lpszClassName, wc.lpszClassName,
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                rect.left, rect.top, w, h, g_win, 0, g_instance, 0);
}

static void
init_gui(HWND hwnd, HINSTANCE hInst)
{
        g_settings_win = hwnd;

	HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(18253));
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

        CreateWindow("Static", "Changes are effective on next launch",
                WS_VISIBLE | WS_CHILD | WS_GROUP | SS_LEFT,
                10, 11, 210, 15, hwnd, (HMENU)0, hInst, 0);

        CreateWindow("button", "Run with legacy Direct3D8 (slower, and\r\n"
				"won't support any of Mirage's custom\r\n"
				"windows, but might be required to work\r\n"
				"on older PCs)",
                WS_VISIBLE | WS_CHILD | BS_CHECKBOX | BS_MULTILINE,
                10, 35, 210, 60,
                hwnd, (HMENU)CHECKBOX_D3D8, hInst, NULL);

        CreateWindow("button", "Use custom font for player/object names\r\n"
				"(More natural font for systems with\r\n"
				"Chinese language)",
                WS_VISIBLE | WS_CHILD | BS_CHECKBOX | BS_MULTILINE,
                10, 100, 210, 45,
                hwnd, (HMENU)CHECKBOX_CFONT, hInst, NULL);

	CheckDlgButton(hwnd, CHECKBOX_D3D8,
			game_config_get("d3d8", "0")[0] == '1' ?
			BST_CHECKED : BST_UNCHECKED);

	CheckDlgButton(hwnd, CHECKBOX_CFONT,
			game_config_get("custom_tag_font", "0")[0] == '1' ?
			BST_CHECKED : BST_UNCHECKED);

        EnumChildWindows(hwnd, (WNDENUMPROC)hwnd_set_font,
                (LPARAM)GetStockObject(DEFAULT_GUI_FONT));
}

static LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM data, LPARAM ldata)
{
        switch (msg) {
        case WM_CREATE:
                init_gui(hwnd, ((LPCREATESTRUCT)ldata)->hInstance);
                SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
                break;
        case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;
        case WM_COMMAND: {
                int checkbox_id = LOWORD(data);
                bool check = !IsDlgButtonChecked(hwnd, checkbox_id);
                CheckDlgButton(hwnd, checkbox_id, check ? BST_CHECKED : BST_UNCHECKED);
                switch (checkbox_id) {
                        case CHECKBOX_D3D8:
				game_config_set("d3d8", check ? "1" : "0");
                                break;
                        case CHECKBOX_CFONT:
				game_config_set("custom_tag_font", check ? "1" : "0");
                                break;
			default:
				break;
                }
                break;
        }
        case WM_DESTROY:
		g_settings_win = NULL;
                break;
        }

        return DefWindowProc(hwnd, msg, data, ldata);
}
