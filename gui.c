/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#include <assert.h>
#include <windows.h>
#include <commctrl.h>
#include <stdbool.h>
#include <locale.h>

#include "gui.h"
#include "common.h"
#include "client_ipc.h"

#define MG_CB_MSG (WM_USER + 165)

HWND g_win;
HWND g_changelog_lbl;
HWND g_status_left_lbl;
HWND g_status_right_lbl;
HWND g_version_lbl;
HWND g_progress_bar;
HWND g_image;

HWND g_quit_button;
HWND g_patch_button;
HWND g_play_button;
HWND g_repair_button;

HBITMAP g_bmp;
HDC g_hdc;
unsigned g_win_tid;

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

static void
set_text_cb(void *_label, void *_txt)
{
	SetWindowText(_label, _txt);
	ShowWindow(_label, SW_HIDE);
	ShowWindow(_label, SW_SHOW);
}

void
set_text(HWND label, const char *txt)
{
	ui_thread(set_text_cb, label, (void *)txt);
}

static void
set_progress_cb(void *_percent, void *arg2)
{
	int percent = (int)(uintptr_t)_percent;

	SendMessage(g_progress_bar, PBM_SETPOS, percent, 0);
}

void
set_progress(int percent)
{
	ui_thread(set_progress_cb, (void *)(uintptr_t)percent, NULL);
}

static void
set_progress_state_cb(void *_state, void *arg2)
{
	unsigned state = (int)(uintptr_t)_state;

	SendMessage(g_progress_bar, (WM_USER + 16), state, 0);

}

void
set_progress_state(unsigned state)
{
	ui_thread(set_progress_state_cb, (void *)(uintptr_t)state, NULL);
}

static void
enable_button_cb(void *_button, void *_enable)
{
	EnableWindow(_button, (bool)(uintptr_t)_enable);
}

void
enable_button(HWND button, bool enable)
{
	ui_thread(enable_button_cb, button, (void *)(uintptr_t)enable);
}

static void
show_ui_cb(void *ui, void *_enable)
{
	ShowWindow(ui, (bool)(uintptr_t)_enable ? SW_SHOW : SW_HIDE);
}

void
show_ui(HWND ui, bool enable)
{
	ui_thread(show_ui_cb,ui, (void *)(uintptr_t)enable);
}

void
quit_cb(void *arg1, void *arg2)
{
	PostQuitMessage(0);
}

static void
set_banner_cb(void *path, void *arg2)
{
	reload_banner(path);
}

void
set_banner(const char *path)
{
	ui_thread(set_banner_cb, (void *)path, NULL);
}

struct ui_thread_ctx {
	mg_callback cb;
	void *arg1;
	void *arg2;
};

void
ui_thread(mg_callback cb, void *arg1, void *arg2)
{
	struct ui_thread_ctx *ctx = malloc(sizeof(*ctx));
	assert(ctx != NULL);

	ctx->cb = cb;
	ctx->arg1 = arg1;
	ctx->arg2 = arg2;
	SendMessage(g_win, MG_CB_MSG, 0, (LPARAM)ctx);
}

void
reload_banner(const char *path)
{
	g_bmp = (HBITMAP)LoadImage(NULL, path,
			IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	g_hdc = CreateCompatibleDC(GetDC(0));
	SelectObject(g_hdc, g_bmp);
	RedrawWindow(g_win, NULL, NULL, RDW_INVALIDATE | RDW_INTERNALPAINT);
}

static BOOL CALLBACK
hwnd_set_font(HWND child, LPARAM font)
{
	SendMessage(child, WM_SETFONT, font, TRUE);
	return TRUE;
}

static void
init_win(HINSTANCE hInst)
{
	WNDCLASSW wc = {0};

	wc.lpszClassName = L"MGLauncher";
	wc.hInstance		 = hInst;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.lpfnWndProc	 = WndProc;
	wc.hCursor			 = LoadCursor(0, IDC_ARROW);

	RegisterClassW(&wc);

	g_win = CreateWindowW(wc.lpszClassName, L"PW Mirage Launcher",
			(WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX) | WS_VISIBLE),
			CW_USEDEFAULT, CW_USEDEFAULT, 720, 420, 0, 0, hInst, 0);
}

static void
init_gui(HWND hwnd, HINSTANCE hInst)
{
	HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(18253));
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

	g_status_left_lbl = CreateWindowW(L"Static", L"",
			WS_VISIBLE | WS_CHILD | WS_GROUP | SS_LEFT,
			14, 327, 275, 23, hwnd, (HMENU)0, hInst, 0);

	g_status_right_lbl = CreateWindowW(L"Static", L"",
			WS_VISIBLE | WS_CHILD | WS_GROUP | SS_RIGHT,
			300, 327, 400, 21, hwnd, (HMENU)0, hInst, 0);

	g_image = CreateWindowW(L"Static", NULL,
			WS_VISIBLE | WS_CHILD | SS_BLACKFRAME,
			11, 11, 392, 268, hwnd, (HMENU)0, hInst, 0);

	g_changelog_lbl = CreateWindowW(L"Static", L"[...]",
			WS_VISIBLE | WS_CHILD | WS_GROUP | SS_LEFT,
			413, 11, 287, 270, hwnd, (HMENU)0, hInst, 0);

	g_version_lbl = CreateWindowW(L"Static", L"Patcher v1.15.0",
			WS_VISIBLE | WS_CHILD | WS_GROUP | SS_LEFT,
			102, 367, 162, 18, hwnd, (HMENU)0, hInst, 0);

	g_progress_bar = CreateWindowEx(0, PROGRESS_CLASS, (LPTSTR) NULL,
		WS_CHILD | WS_VISIBLE, 12, 296, 689, 23,
		hwnd, (HMENU) 0, hInst, NULL);

	g_quit_button = CreateWindowW(L"Button", L"Quit",
			WS_VISIBLE | WS_CHILD | WS_TABSTOP, 618, 358, 83, 23,
			hwnd, (HMENU)MG_GUI_ID_QUIT, hInst, 0);

	g_patch_button = CreateWindowW(L"Button", L"Patch",
			WS_VISIBLE | WS_CHILD | WS_TABSTOP | 0x00000001,
			515, 359, 83, 23, hwnd, (HMENU)MG_GUI_ID_PATCH, hInst, 0);

	g_play_button = CreateWindowW(L"Button", L"Play!",
			WS_VISIBLE | WS_CHILD | WS_TABSTOP | 0x00000001,
			412, 359, 83, 23, hwnd, (HMENU)MG_GUI_ID_PLAY, hInst, 0);

	g_repair_button = CreateWindowW(L"Button", L"Repair files",
			WS_VISIBLE | WS_CHILD | WS_TABSTOP | 0x00000001,
			12, 363, 83, 23, hwnd, (HMENU)MG_GUI_ID_REPAIR, hInst, 0);

	reload_banner("patcher/banner");

	EnableWindow(g_patch_button, FALSE);
	EnableWindow(g_play_button, FALSE);
	EnableWindow(g_repair_button, FALSE);

	EnumChildWindows(hwnd, (WNDENUMPROC)hwnd_set_font,
			(LPARAM)GetStockObject(DEFAULT_GUI_FONT));
}

static char *g_cmdline;
static char g_exe_name[32] = "pwmirage.exe";

static void
on_init_cb(void *arg1, void *arg2)
{
	char *argv[64] = {0};
	int argc = 0;
	char *str = g_cmdline;
	char *c = str;

	PWLOG(LOG_INFO, "Flags: %s\n", g_cmdline);
	argv[argc++] = g_exe_name;

	while (c && *c) {
		if (*c == ' ') {
			*c = 0;
			argv[argc++] = str;
			str = c + 1;
		}
		c++;
	}

	if (str && *str) {
		argv[argc++] = str;
	}

	on_init(argc, argv);
}

static LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM arg1, LPARAM arg2)
{
	switch(msg) {
		case WM_CREATE:
		{
			HINSTANCE hInst = ((LPCREATESTRUCT)arg2)->hInstance;
			init_gui(hwnd, hInst);
			SetWindowPos(hwnd, HWND_TOP, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
			task(on_init_cb, NULL, NULL);
			break;
		}
		case WM_COMMAND:
			on_button_click((int)LOWORD(arg1));
			break;
		case MG_CB_MSG: {
			struct ui_thread_ctx ctx, *org_ctx = (void *)arg2;

			ctx = *org_ctx;
			free(org_ctx);
			ctx.cb(ctx.arg1, ctx.arg2);
			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdcDestination;

			hdcDestination = BeginPaint(hwnd, &ps);
			BitBlt(hdcDestination, 12, 12, 390, 266, g_hdc, 0, 0, SRCCOPY);
			EndPaint(hwnd, &ps);
			break;
		}
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
	}

	return DefWindowProcW(hwnd, msg, arg1, arg2);
}

static void
on_fini_cb(void *arg1, void *arg2) {
	on_fini();
}

static void
parse_patcher_msg(enum mg_patcher_msg_command type, unsigned param)
{
	int rc;
	bool bval;
	char tmpbuf[512];

	fprintf(stderr, "patcher msg: %u, %u\n", (unsigned)type, param);
	fflush(stderr);

	switch (type) {
		case 0:
			fprintf(stderr, "Patcher Initialized\n");
			break;
		case MGP_MSG_SET_STATUS_LEFT:
		case MGP_MSG_SET_STATUS_RIGHT:
			rc = mg_patcher_msg_get_status(type, param, tmpbuf, sizeof(tmpbuf));
			if (rc) {
				fprintf(stderr, "unknown status: %u\n", param);
			}

			if (type == MGP_MSG_SET_STATUS_LEFT) {
				set_text(g_status_left_lbl, tmpbuf);
			} else {
				set_text(g_status_right_lbl, tmpbuf);
			}
			break;
		case MGP_MSG_ENABLE_BUTTON:
		case MGP_MSG_DISABLE_BUTTON:
			bval = type == MGP_MSG_ENABLE_BUTTON;

			switch (param) {
				case MG_GUI_ID_QUIT:
					enable_button(g_quit_button, bval);
					break;
				case MG_GUI_ID_PATCH:
					enable_button(g_patch_button, bval);
					break;
				case MG_GUI_ID_PLAY:
					enable_button(g_play_button, bval);
					break;
				case MG_GUI_ID_REPAIR:
					enable_button(g_repair_button, bval);
					break;
				default:
					break;
			}
			break;
		case MGP_MSG_SET_PROGRESS:
			set_progress(param);
			break;
	}
}

int WINAPI
WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance,
				LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;

	setlocale(LC_ALL, "en_US.UTF-8");
	freopen("patcher/launcher.log", "w", stderr);

	g_win_tid = GetCurrentThreadId();
	g_cmdline = lpCmdLine;

	init_win(hInst);
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);

		if (msg.message == MG_PATCHER_STATUS_MSG) {
			parse_patcher_msg((enum mg_patcher_msg_command)msg.wParam, (unsigned)msg.lParam);
		} else {
			DispatchMessage(&msg);
		}
	}

	task(on_fini_cb, NULL, NULL);
	return (int) msg.wParam;
}
