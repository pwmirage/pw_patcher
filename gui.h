/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_GUI_H
#define PW_GUI_H

#ifndef PBST_PAUSED
#define PBST_PAUSED 0x0003
#define PBST_ERROR 0x0002
#define PBST_NORMAL 0x0001
#endif

typedef void (*mg_callback)(void *arg1, void *arg2);

extern HINSTANCE g_instance;

extern HWND g_win;

extern HCURSOR g_cursor_hand;
extern HCURSOR g_cursor_arrow;

extern HBITMAP g_bmp;
extern HDC g_hdc;
extern unsigned g_win_tid;

struct task_ctx {
	mg_callback cb;
	void *arg1;
	void *arg2;
};

void on_init(int argc, char *argv[]);
void on_fini();
void ui_thread(mg_callback cb, void *arg1, void *arg2);
void task(mg_callback cb, void *arg1, void *arg2);
void reload_banner(const char *path);
void on_button_click(int btn);

void set_text(int hwnd_id, const char *txt);
void set_progress(int percent);
void set_progress_state(unsigned state);
void enable_button(int btn_id, bool enable);
void show_ui(HWND ui, bool enable);
void set_banner(const char *path);
void quit_cb(void *arg1, void *arg2);

struct button_brush {
	struct button_state_brush {
		unsigned bg;
		unsigned bg_res_id;
		unsigned text;
		struct {
			COLORREF text_color;
			HBRUSH bg_brush;
		} calc;
	} regular, hover, pressed, disabled;
};

struct gui_button;
typedef void (*button_click_cb)(void *ctx);
typedef unsigned (*button_draw_cb)(struct gui_button *btn, LPNMCUSTOMDRAW item);

struct gui_button {
	int id;
	const char *text;
	button_click_cb click_cb;
	void *click_ctx;
	button_draw_cb draw_cb;
	void *draw_ctx;
	HWND hwnd;
	bool enabled;
};

struct gui_button *gui_button_init(int id, const char *text,
		int x, int y, int w, int h,
		HWND p_hwnd, HINSTANCE p_hinst);
struct gui_button *gui_button_get(int id);
unsigned gui_button_draw_brush(struct gui_button *btn, LPNMCUSTOMDRAW item);
unsigned gui_button_draw_img(struct gui_button *btn, LPNMCUSTOMDRAW item);

void show_settings_win(bool show);

#endif /* PW_GUI_H */
