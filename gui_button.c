/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <windows.h>
#include <commctrl.h>
#include <assert.h>

#include "gui.h"

#define MAX_BUTTONS 16
static struct gui_button *g_buttons[MAX_BUTTONS];

/* regular gray */
static struct button_brush g_brush_regular = {
	.regular = { .bg = 0xdccfcf, .text = 0x212121 },
	.hover = { .bg = 0xbfb5b5, .text = 0x000000 },
	.pressed = { .bg = 0xa29a9a, .text = 0x000000 },
	.disabled = { .bg = 0xdfdfdf, .text = 0xa5a5a5 }
};

static WNDPROC g_orig_button_handler;
static LRESULT CALLBACK WndProcButton(HWND hwnd, UINT msg, WPARAM arg1, LPARAM arg2);

struct gui_button *
gui_button_init(int id, const char *text,
		int x, int y, int w, int h,
		HWND p_hwnd, HINSTANCE p_hinst)
{
	assert(!g_buttons[id]);
	struct gui_button *btn = calloc(1, sizeof(*btn));

	assert(btn != NULL);
	btn->id = id;
	btn->text = strdup(text);
	assert(btn->text != NULL);

	btn->hwnd = CreateWindow("Button", text,
			WS_VISIBLE | WS_CHILD | WS_TABSTOP,
			x, y, w, h,
			p_hwnd, (HMENU)id, p_hinst, 0);

	btn->draw_cb = gui_button_draw_brush;
	btn->draw_ctx = &g_brush_regular;

	g_orig_button_handler =
			(WNDPROC)SetWindowLongPtr(btn->hwnd,
			GWLP_WNDPROC, (LONG_PTR)WndProcButton);

	g_buttons[id] = btn;
	return btn;
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
			item->rc.right - item->rc.left,
			item->rc.bottom - item->rc.top,
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

struct gui_button *gui_button_get(int id)
{
	if (id >= MAX_BUTTONS) {
		return NULL;
	}

	return g_buttons[id];
}

#define HEX2COLORREF(hex) (((hex & 0xff) << 16) | (((hex >> 8) & 0xff) << 8) | ((hex >> 16) & 0xff))

unsigned
gui_button_draw_brush(struct gui_button *btn, LPNMCUSTOMDRAW item)
{
	struct button_brush *brush = btn->draw_ctx;
	struct button_state_brush *sbrush = NULL;
	struct button_state_brush *sbrushes[] = { &brush->regular, &brush->hover, &brush->pressed, &brush->disabled };

	if (!brush->regular.calc.bg_brush) {
		for (int i = 0; i < sizeof(sbrushes) / sizeof(sbrushes[0]); i++) {
			HDC src_hdcmem = NULL;

			sbrush = sbrushes[i];
			if (sbrush->bg_res_id) {
				HINSTANCE hinst = (HINSTANCE)GetWindowLong(
						btn->hwnd, GWL_HINSTANCE);

				HBITMAP src_bitmap = (HBITMAP)LoadImage(hinst,
						MAKEINTRESOURCE(sbrush->bg_res_id),
						IMAGE_BITMAP, 0, 0, 0);
				src_hdcmem = CreateCompatibleDC(item->hdc);
				SelectObject(src_hdcmem, src_bitmap);
			}

			sbrush->calc.bg_brush = create_pattern_brush(
					HEX2COLORREF(sbrush->bg), src_hdcmem, item);
			sbrush->calc.text_color = HEX2COLORREF(sbrush->text);
		}
	}

	if (!btn->enabled) {
		/* disabled */
		sbrush = &brush->disabled;
	} else if (item->uItemState & CDIS_SELECTED) {
		/* pressed */
		sbrush = &brush->pressed;
	} else if (item->uItemState & CDIS_HOT) {
		/* mouse hover */
		sbrush = &brush->hover;
	} else {
		/* default state */
		sbrush = &brush->regular;
	}

	HGDIOBJ old_brush;
	old_brush = SelectObject(item->hdc, sbrush->calc.bg_brush);
	FillRect(item->hdc, &item->rc, sbrush->calc.bg_brush);
	SelectObject(item->hdc, old_brush);

	SetBkMode(item->hdc, TRANSPARENT);
	SetTextColor(item->hdc, sbrush->calc.text_color);

	char buf[64];
	GetWindowText(btn->hwnd, buf, sizeof(buf));
	DrawTextA(item->hdc, buf, -1, &item->rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	return CDRF_SKIPDEFAULT;
}

static LRESULT CALLBACK
WndProcButton(HWND hwnd, UINT msg, WPARAM arg1, LPARAM arg2)
{
	switch(msg) {
		case WM_SETCURSOR: {
			int id = GetDlgCtrlID(hwnd);
			struct gui_button *btn = gui_button_get(id);

			if (!btn) {
				return CallWindowProc(g_orig_button_handler,
						hwnd, msg, arg1, arg2);
			}

			if (btn->enabled) {
				SetCursor(g_cursor_hand);
			} else {
				SetCursor(g_cursor_arrow);
			}
			return TRUE;
		}
	}

	return CallWindowProc(g_orig_button_handler, hwnd, msg, arg1, arg2);
}

