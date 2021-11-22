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
static HWND g_status_left_lbl_shadow[4];
static HWND g_status_right_lbl_shadow[4];

HWND g_patch_button;
HWND g_play_button;
HWND g_repair_button;
HWND g_settings_button;

HBITMAP g_bmp;
HDC g_hdc;
unsigned g_win_tid;

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK WndProcChangelogBox(HWND, UINT, WPARAM, LPARAM);

/* red play */
static struct button_brush g_brush_red = {
	.regular = { .bg = 0xcf4545, .text = 0xffffff },
	.hover = { .bg = 0xac3838, .text = 0xffffff },
	.pressed = { .bg = 0x8e3030, .text = 0xffffff },
	.disabled = { .bg = 0xdfdfdf, .text = 0xa5a5a5 }
};

/* yellow patch */
static struct button_brush g_brush_yellow = {
	.regular = { .bg = 0xcfc845, .text = 0x212121 },
	.hover = { .bg = 0xb1ab3b, .text = 0x000000 },
	.pressed = { .bg = 0x8e3030, .text = 0xffffff },
	.disabled = { .bg = 0xdfdfdf, .text = 0xa5a5a5 }
};

static struct button_brush g_brush_settings = {
	.regular = { .bg_res_id = 18254 },
	.hover = { .bg_res_id = 18255 },
	.pressed = { .bg_res_id = 18256 },
	.disabled = { .bg_res_id = 18256 }
};

HCURSOR g_cursor_hand;
HCURSOR g_cursor_arrow;

static void
set_hwnd_text(HWND hwnd, const char *txt)
{
	SetWindowText(hwnd, txt);
	ShowWindow(hwnd, SW_HIDE);
	ShowWindow(hwnd, SW_SHOW);
}

static void
set_text_cb(void *_hwnd_id, void *_txt)
{
	int hwnd_id = (int)(uintptr_t)_hwnd_id;

	HWND hwnd = GetDlgItem(g_win, hwnd_id);
	set_hwnd_text(hwnd, _txt);

	if (hwnd_id == MG_GUI_ID_STATUS_RIGHT) {
		for (int i = 0; i < 4; i++) {
			set_hwnd_text(g_status_right_lbl_shadow[i], _txt);
		}
	} else if (hwnd_id == MG_GUI_ID_STATUS_LEFT) {
		for (int i = 0; i < 4; i++) {
			set_hwnd_text(g_status_left_lbl_shadow[i], _txt);
		}
	}
}

void
set_text(int hwnd_id, const char *txt)
{
	ui_thread(set_text_cb, (void *)(uintptr_t)hwnd_id, (void *)txt);
}

static void
set_progress_cb(void *_percent, void *arg2)
{
	int percent = (int)(uintptr_t)_percent;
	HWND bar = GetDlgItem(g_win, MG_GUI_ID_PROGRESS);

	SendMessage(bar, PBM_SETPOS, percent, 0);
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
	HWND bar = GetDlgItem(g_win, MG_GUI_ID_PROGRESS);

	SendMessage(bar, (WM_USER + 16), state, 0);
}

void
set_progress_state(unsigned state)
{
	ui_thread(set_progress_state_cb, (void *)(uintptr_t)state, NULL);
}

static void
enable_button_cb(void *_btn_id, void *_enable)
{
	struct gui_button *btn;
	int id = (int)(uintptr_t)_btn_id;

	btn = gui_button_get(id);
	if (!btn) {
		return;
	}

	btn->enabled = (bool)(uintptr_t)_enable;
	RedrawWindow(g_win, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void
enable_button(int btn_id, bool enable)
{
	ui_thread(enable_button_cb, (void *)(uintptr_t)btn_id, (void *)(uintptr_t)enable);
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

static void
init_gui(HWND hwnd, HINSTANCE hInst)
{
	struct gui_button *btn;
	HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(18253));

	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	g_instance = hInst;

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

	CreateWindowW(L"Static", L"",
			WS_VISIBLE | WS_CHILD | WS_GROUP | SS_LEFT,
			20, 351, 379, 21, hwnd,
			(HMENU)MG_GUI_ID_STATUS_LEFT, hInst, 0);

	for (int i = 0; i < 4; i++) {
		g_status_right_lbl_shadow[i] = CreateWindowW(L"Static", L"",
				WS_VISIBLE | WS_CHILD | WS_GROUP | SS_RIGHT,
				20 + shadow_offs[i].x, 351 + shadow_offs[i].y,
				379, 21, hwnd, (HMENU)0, hInst, 0);
	}

	CreateWindowW(L"Static", L"",
			WS_VISIBLE | WS_CHILD | WS_GROUP | SS_RIGHT,
			20, 351, 379, 21, hwnd,
			(HMENU)MG_GUI_ID_STATUS_RIGHT, hInst, 0);

	g_changelog_lbl = CreateWindowW(L"Edit", L"[...]",
			WS_VISIBLE | WS_CHILD | WS_GROUP | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
			580, 50, 320, 360, hwnd, (HMENU)MG_GUI_ID_CHANGELOG, hInst, 0);

	CreateWindowW(L"Static", L"Patcher v1.15.0",
			WS_VISIBLE | WS_CHILD | WS_GROUP | SS_LEFT,
			102, 500, 162, 18, hwnd, (HMENU)MG_GUI_ID_VERSION, hInst, 0);

	CreateWindow(PROGRESS_CLASS, (LPTSTR) NULL,
		WS_CHILD | WS_VISIBLE, 16, 372, 385, 32,
		hwnd, (HMENU)MG_GUI_ID_PROGRESS, hInst, NULL);

	btn = gui_button_init(MG_GUI_ID_SETTINGS, "",
			526, 12, 32, 32, hwnd, hInst);
	btn->draw_ctx = &g_brush_settings;
	btn->enabled = true;

	btn = gui_button_init(MG_GUI_ID_QUIT, "Quit",
			471, 260, 83, 23, hwnd, hInst);
	btn->enabled = true;

	btn = gui_button_init(MG_GUI_ID_PATCH, "Patch",
			418, 330, 136, 34, hwnd, hInst);
	btn->draw_ctx = &g_brush_yellow;

	btn = gui_button_init(MG_GUI_ID_PLAY, "Play!",
			418, 371, 136, 34, hwnd, hInst);
	btn->draw_ctx = &g_brush_red;

	btn = gui_button_init(MG_GUI_ID_REPAIR, "Repair files",
			471, 289, 83, 23, hwnd, hInst);

	reload_banner("patcher/banner");

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
		case WM_COMMAND: {
			int btn_id = (int)LOWORD(arg1);
			struct gui_button *btn = gui_button_get(btn_id);

			if (btn && btn->enabled) {
				on_button_click((int)LOWORD(arg1));
			}
			break;
		 }
		case MG_CB_MSG: {
			struct ui_thread_ctx ctx, *org_ctx = (void *)arg2;

			ctx = *org_ctx;
			free(org_ctx);
			ctx.cb(ctx.arg1, ctx.arg2);
			break;
		}
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC: {
			int id = GetDlgCtrlID((HWND)arg2);

			if (id == MG_GUI_ID_CHANGELOG) {
				SetBkColor((HDC)arg1, RGB(250, 250, 250));
				SetTextColor((HDC)arg1, RGB(80,44,44));
				return (LRESULT)GetStockObject(HOLLOW_BRUSH);
			} else if (id == MG_GUI_ID_STATUS_LEFT ||
					id == MG_GUI_ID_STATUS_RIGHT ||
					id == MG_GUI_ID_VERSION) {
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
		}
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
			LPNMCUSTOMDRAW item = (LPNMCUSTOMDRAW)lpnmhdr;
			struct gui_button *btn;
			unsigned ret = CDRF_DODEFAULT;

			if (lpnmhdr->code != NM_CUSTOMDRAW) {
				return CDRF_DODEFAULT;
			}

			btn = gui_button_get(lpnmhdr->idFrom);
			if (btn) {
				assert(btn->draw_cb);
				ret = btn->draw_cb(btn, item);
			}

			return ret;
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

static void
parse_patcher_msg(enum mg_patcher_msg_command type, unsigned param)
{
	int rc;
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
				set_text(MG_GUI_ID_STATUS_LEFT, tmpbuf);
			} else {
				set_text(MG_GUI_ID_STATUS_RIGHT, tmpbuf);
			}
			break;
		case MGP_MSG_ENABLE_BUTTON:
		case MGP_MSG_DISABLE_BUTTON:
			enable_button(param, type == MGP_MSG_ENABLE_BUTTON);
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
