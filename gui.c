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

HINSTANCE g_instance;
HWND g_win;
HWND g_changelog_lbl;
HWND g_status_left_lbl;
static HWND g_status_left_lbl_shadow[4];
HWND g_status_right_lbl;
static HWND g_status_right_lbl_shadow[4];
HWND g_version_lbl;
HWND g_progress_bar;

HWND g_quit_button;
HWND g_patch_button;
HWND g_play_button;
HWND g_repair_button;
HWND g_settings_button;

HBITMAP g_bmp;
HDC g_hdc;
unsigned g_win_tid;

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK WndProcChangelogBox(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK WndProcButton(HWND hwnd, UINT msg, WPARAM arg1, LPARAM arg2);

struct button_state_brush {
	HBRUSH bg_brush;
	COLORREF text_color;
};

struct button_brush {
	struct button_state_brush regular;
	struct button_state_brush hover;
	struct button_state_brush pressed;
	struct button_state_brush disabled;
};

static bool g_button_state[16];

static struct button_brush g_brush_btn;
static struct button_brush g_brush_btn_play;
static struct button_brush g_brush_btn_patch;

static HCURSOR g_cursor_hand;
static HCURSOR g_cursor_arrow;

static void
set_text_cb(void *_label, void *_txt)
{
	SetWindowText(_label, _txt);
	ShowWindow(_label, SW_HIDE);
	ShowWindow(_label, SW_SHOW);

	if (_label == g_status_right_lbl) {
		for (int i = 0; i < 4; i++) {
			set_text_cb(g_status_right_lbl_shadow[i], _txt);
		}
	} else if (_label == g_status_left_lbl) {
		for (int i = 0; i < 4; i++) {
			set_text_cb(g_status_left_lbl_shadow[i], _txt);
		}
	}
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
	int id = 0;

	if (_button == g_quit_button) {
		id = MG_GUI_ID_QUIT;
	} else if (_button == g_patch_button) {
		id = MG_GUI_ID_PATCH;
	} else if (_button == g_play_button) {
		id = MG_GUI_ID_PLAY;
	} else if (_button == g_repair_button) {
		id = MG_GUI_ID_REPAIR;
	} else if (_button == g_settings_button) {
		id = MG_GUI_ID_SETTINGS;
	} else {
		return;
	}

	g_button_state[id] = (bool)(uintptr_t)_enable;
	RedrawWindow(g_win, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
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
	wc.hInstance = hInst;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.lpfnWndProc = WndProc;
	wc.hCursor = LoadCursor(0, IDC_ARROW);

	RegisterClassW(&wc);

	RECT rect = { 0, 0, 900, 420 };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME, FALSE);
	g_win = CreateWindowW(wc.lpszClassName, L"PW Mirage Launcher",
			(WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX)) | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, 0, 0, hInst, 0);
}

static WNDPROC g_orig_changelog_event_handler;
static WNDPROC g_orig_button_handler;

static void
init_gui(HWND hwnd, HINSTANCE hInst)
{
	g_instance = hInst;

	HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(18253));
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

	struct shadow_off {
		int x, y;
	} shadow_offs[] = {
		{ -1, 0 },
		{ 1, 0 },
		{ 0, -1 },
		{ 0, 1 },
	};

	for (int i = 0; i < 4; i++) {
		g_status_left_lbl_shadow[i] = CreateWindowW(L"Static", L"",
				WS_VISIBLE | WS_CHILD | WS_GROUP | SS_LEFT,
				20 + shadow_offs[i].x, 351 + shadow_offs[i].y,
				379, 21, hwnd, (HMENU)0, hInst, 0);
	}

	g_status_left_lbl = CreateWindowW(L"Static", L"",
			WS_VISIBLE | WS_CHILD | WS_GROUP | SS_LEFT,
			20, 351, 379, 21, hwnd, (HMENU)0, hInst, 0);

	for (int i = 0; i < 4; i++) {
		g_status_right_lbl_shadow[i] = CreateWindowW(L"Static", L"",
				WS_VISIBLE | WS_CHILD | WS_GROUP | SS_RIGHT,
				20 + shadow_offs[i].x, 351 + shadow_offs[i].y,
				379, 21, hwnd, (HMENU)0, hInst, 0);
	}

	g_status_right_lbl = CreateWindowW(L"Static", L"",
			WS_VISIBLE | WS_CHILD | WS_GROUP | SS_RIGHT,
			20, 351, 379, 21, hwnd, (HMENU)0, hInst, 0);

	g_changelog_lbl = CreateWindowW(L"Edit", L"[...]",
			WS_VISIBLE | WS_CHILD | WS_GROUP | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
			580, 50, 320, 360, hwnd, (HMENU)0, hInst, 0);

	g_version_lbl = CreateWindowW(L"Static", L"Patcher v1.15.0",
			WS_VISIBLE | WS_CHILD | WS_GROUP | SS_LEFT,
			102, 500, 162, 18, hwnd, (HMENU)0, hInst, 0);

	g_progress_bar = CreateWindow(PROGRESS_CLASS, (LPTSTR) NULL,
		WS_CHILD | WS_VISIBLE, 16, 372, 385, 32,
		hwnd, (HMENU) 0, hInst, NULL);

	g_quit_button = CreateWindowW(L"Button", L"Quit",
			WS_VISIBLE | WS_CHILD | WS_TABSTOP, 471, 260, 83, 23,
			hwnd, (HMENU)MG_GUI_ID_QUIT, hInst, 0);

	g_patch_button = CreateWindowW(L"Button", L"Patch",
			WS_VISIBLE | WS_CHILD | WS_TABSTOP | 0x00000001,
			418, 330, 136, 34, hwnd, (HMENU)MG_GUI_ID_PATCH, hInst, 0);

	g_play_button = CreateWindowW(L"Button", L"Play!",
			WS_VISIBLE | WS_CHILD | WS_TABSTOP | 0x00000001,
			418, 371, 136, 34, hwnd, (HMENU)MG_GUI_ID_PLAY, hInst, 0);

	g_repair_button = CreateWindowW(L"Button", L"Repair files",
			WS_VISIBLE | WS_CHILD | WS_TABSTOP | 0x00000001,
			471, 289, 83, 23, hwnd, (HMENU)MG_GUI_ID_REPAIR, hInst, 0);

	g_settings_button = CreateWindowW(L"Button", L"Settings",
			WS_VISIBLE | WS_CHILD | WS_TABSTOP, 471, 219, 83, 23,
			hwnd, (HMENU)MG_GUI_ID_SETTINGS, hInst, 0);

	reload_banner("patcher/banner");

	g_button_state[MG_GUI_ID_QUIT] = true;
	g_button_state[MG_GUI_ID_SETTINGS] = true;

	/* create bold font for labels */
	HFONT default_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	LOGFONT lf;
	GetObject(default_font, sizeof(lf), &lf);
	strcpy(lf.lfFaceName, "Microsoft Sans Serif");
	lf.lfWeight = FW_BOLD;
	HFONT bold_font = CreateFontIndirect(&lf);

	EnumChildWindows(hwnd, (WNDENUMPROC)hwnd_set_font, (LPARAM)bold_font);
	SendMessage(g_changelog_lbl, WM_SETFONT, (WPARAM)default_font, TRUE);

	g_orig_changelog_event_handler =
			(WNDPROC)SetWindowLongPtr(g_changelog_lbl,
			GWLP_WNDPROC, (LONG_PTR)WndProcChangelogBox);

	g_orig_button_handler =
			(WNDPROC)SetWindowLongPtr(g_play_button,
			GWLP_WNDPROC, (LONG_PTR)WndProcButton);

	SetWindowLongPtr(g_repair_button, GWLP_WNDPROC, (LONG_PTR)WndProcButton);
	SetWindowLongPtr(g_patch_button, GWLP_WNDPROC, (LONG_PTR)WndProcButton);
	SetWindowLongPtr(g_quit_button, GWLP_WNDPROC, (LONG_PTR)WndProcButton);
	SetWindowLongPtr(g_settings_button, GWLP_WNDPROC, (LONG_PTR)WndProcButton);
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

static HBRUSH
create_pattern_brush(COLORREF color, HDC hdc, LPNMCUSTOMDRAW item)
{
	HDC hdcmem = CreateCompatibleDC(item->hdc);
	HBITMAP hbitmap = CreateCompatibleBitmap(item->hdc, item->rc.right-item->rc.left, item->rc.bottom-item->rc.top);
	HBRUSH pattern;

	SelectObject(hdcmem, hbitmap);

	if (hdc) {
		BitBlt(hdcmem, 0, 0,
			item->rc.right-item->rc.right - item->rc.right-item->rc.left,
			item->rc.right-item->rc.bottom - item->rc.right-item->rc.top,
			hdc, 0, 0, SRCCOPY);
	} else {
		HBRUSH brush = CreateSolidBrush(color);
		FillRect(hdcmem, &item->rc, brush);
		DeleteObject(brush);
	}

	pattern = CreatePatternBrush(hbitmap);

	DeleteDC(hdcmem);
	DeleteObject(hbitmap);

	return pattern;
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
			if (g_button_state[(int)LOWORD(arg1)]) {
				on_button_click((int)LOWORD(arg1));
			}
			break;
		case MG_CB_MSG: {
			struct ui_thread_ctx ctx, *org_ctx = (void *)arg2;

			ctx = *org_ctx;
			free(org_ctx);
			ctx.cb(ctx.arg1, ctx.arg2);
			break;
		}
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC:
			if ((HWND)arg2 == g_changelog_lbl) {
				SetBkColor((HDC)arg1, RGB(250, 250, 250));
				SetTextColor((HDC)arg1, RGB(80,44,44));
				return (LRESULT)GetStockObject(HOLLOW_BRUSH);
			} else if ((HWND)arg2 == g_status_left_lbl ||
					(HWND)arg2 == g_status_right_lbl ||
					(HWND)arg2 == g_version_lbl) {
				SetBkMode((HDC)arg1, TRANSPARENT);
				SetTextColor((HDC)arg1, RGB(250, 250, 250));
				return (LRESULT)GetStockObject(HOLLOW_BRUSH);
			}

			for (int i = 0; i < 4; i++) {
				if ((HWND)arg2 == g_status_left_lbl_shadow[i] ||
						(HWND)arg2 == g_status_right_lbl_shadow[i]) {
					SetBkMode((HDC)arg1, TRANSPARENT);
					return (LRESULT)GetStockObject(HOLLOW_BRUSH);
				}
			}
			break;
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdcDestination;

			hdcDestination = BeginPaint(hwnd, &ps);
			BitBlt(hdcDestination, 0, 0, 900, 420, g_hdc, 0, 0, SRCCOPY);
			EndPaint(hwnd, &ps);
			break;
		}
		case WM_NOTIFY: {
			LPNMHDR lpnmhdr = (LPNMHDR)arg2;
			struct button_brush *btn_brush = NULL;
			struct button_state_brush *btn_brush_state = NULL;
			HWND btn;

			if (lpnmhdr->code != NM_CUSTOMDRAW) {
				return CDRF_DODEFAULT;
			}

			LPNMCUSTOMDRAW item = (LPNMCUSTOMDRAW)lpnmhdr;
			HGDIOBJ old_brush;

			if (!g_brush_btn.regular.bg_brush) {
				g_brush_btn.regular.bg_brush = create_pattern_brush(RGB(220, 207, 207), NULL, item);
				g_brush_btn.regular.text_color = RGB(33, 33, 33);
				g_brush_btn.hover.bg_brush = create_pattern_brush(RGB(191, 181, 181), NULL, item);
				g_brush_btn.hover.text_color = RGB(0, 0, 0);
				g_brush_btn.pressed.bg_brush = create_pattern_brush(RGB(162, 154, 154), NULL, item);
				g_brush_btn.pressed.text_color = RGB(0, 0, 0);
				g_brush_btn.disabled.bg_brush = create_pattern_brush(RGB(223, 223, 223), NULL, item);
				g_brush_btn.disabled.text_color = RGB(165, 165, 165);


				g_brush_btn_play.regular.bg_brush = create_pattern_brush(RGB(207, 69, 69), NULL, item);
				g_brush_btn_play.regular.text_color = RGB(255, 255, 255);
				g_brush_btn_play.hover.bg_brush = create_pattern_brush(RGB(172, 56, 56), NULL, item);
				g_brush_btn_play.hover.text_color = RGB(255, 255, 255);
				g_brush_btn_play.pressed.bg_brush = create_pattern_brush(RGB(142, 48, 48), NULL, item);
				g_brush_btn_play.pressed.text_color = RGB(255, 255, 255);
				g_brush_btn_play.disabled.bg_brush = create_pattern_brush(RGB(223, 223, 223), NULL, item);
				g_brush_btn_play.disabled.text_color = RGB(165, 165, 165);


				g_brush_btn_patch.regular.bg_brush = create_pattern_brush(RGB(207, 200, 69), NULL, item);
				g_brush_btn_patch.regular.text_color = RGB(33, 33, 33);
				g_brush_btn_patch.hover.bg_brush = create_pattern_brush(RGB(177, 171, 59), NULL, item);
				g_brush_btn_patch.hover.text_color = RGB(0, 0, 0);
				g_brush_btn_patch.pressed.bg_brush = create_pattern_brush(RGB(140, 135, 45), NULL, item);
				g_brush_btn_patch.pressed.text_color = RGB(255, 255, 255);
				g_brush_btn_patch.disabled.bg_brush = create_pattern_brush(RGB(223, 223, 223), NULL, item);
				g_brush_btn_patch.disabled.text_color = RGB(165, 165, 165);
			}

			switch (lpnmhdr->idFrom) {
				case MG_GUI_ID_PLAY:
					btn_brush = &g_brush_btn_play;
					btn = g_play_button;
					break;
				case MG_GUI_ID_QUIT:
					btn = g_quit_button;
					btn_brush = &g_brush_btn;
					break;
				case MG_GUI_ID_PATCH:
					btn = g_patch_button;
					btn_brush = &g_brush_btn_patch;
					break;
				case MG_GUI_ID_REPAIR:
					btn = g_repair_button;
					btn_brush = &g_brush_btn;
					break;
				case MG_GUI_ID_SETTINGS:
					btn = g_settings_button;
					btn_brush = &g_brush_btn;
					break;
			}

			if (!btn_brush || lpnmhdr->idFrom >= sizeof(g_button_state) / sizeof(g_button_state[0])) {
				return CDRF_DODEFAULT;
			}

			if (!g_button_state[lpnmhdr->idFrom]) {
				/* disabled */
				btn_brush_state = &btn_brush->disabled;
			} else if (item->uItemState & CDIS_SELECTED) {
				/* pressed */
				btn_brush_state = &btn_brush->pressed;
			} else if (item->uItemState & CDIS_HOT) {
				/* mouse hover */
				btn_brush_state = &btn_brush->hover;
			} else {
				/* default state */
				btn_brush_state = &btn_brush->regular;
			}

			old_brush = SelectObject(item->hdc, btn_brush_state->bg_brush);
			FillRect(item->hdc, &item->rc, btn_brush_state->bg_brush);
			SelectObject(item->hdc, old_brush);

			SetBkMode(item->hdc, TRANSPARENT);
			SetTextColor(item->hdc, btn_brush_state->text_color);

			char buf[64];
			GetWindowText(btn, buf, sizeof(buf));
			DrawTextA(item->hdc, buf, -1, &item->rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
			return CDRF_SKIPDEFAULT;
	       }
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
	}

	return DefWindowProcW(hwnd, msg, arg1, arg2);
}

static LRESULT CALLBACK
WndProcChangelogBox(HWND hwnd, UINT msg, WPARAM arg1, LPARAM arg2)
{
	LRESULT ret;

	switch(msg) {
		case EM_REPLACESEL:
		case EM_SETSEL:
		case EM_GETSEL:
			return 0;
	}

	ret = CallWindowProc(g_orig_changelog_event_handler, hwnd, msg, arg1, arg2);
	switch(msg) {
		case WM_SETFOCUS:
			HideCaret(hwnd);
			break;
	}

	return ret;
}

static LRESULT CALLBACK
WndProcButton(HWND hwnd, UINT msg, WPARAM arg1, LPARAM arg2)
{
	switch(msg) {
		case WM_SETCURSOR: {
			int id = GetDlgCtrlID(hwnd);
			if (id >= sizeof(g_button_state) / sizeof(g_button_state[0]) ||
					g_button_state[id]) {
				SetCursor(g_cursor_hand);
			} else {
				SetCursor(g_cursor_arrow);
			}
			return TRUE;
		}
	}

	return CallWindowProc(g_orig_button_handler, hwnd, msg, arg1, arg2);
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
		case MGP_MSG_INIT:
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
				case MG_GUI_ID_SETTINGS:
					enable_button(g_settings_button, bval);
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

	g_cursor_arrow = LoadCursor(0, IDC_ARROW);
	g_cursor_hand = LoadCursor(0, IDC_HAND);

	init_win(hInst);
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);

		if (msg.message == MG_PATCHER_STATUS_MSG) {
			parse_patcher_msg((enum mg_patcher_msg_command)msg.wParam, (unsigned)msg.lParam);
		} else {
			DispatchMessage(&msg);
		}
	}

	on_fini();
	return (int) msg.wParam;
}
