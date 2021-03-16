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

extern HWND g_win;
extern HWND g_changelog_lbl;
extern HWND g_status_left_lbl;
extern HWND g_status_right_lbl;
extern HWND g_version_lbl;
extern HWND g_progress_bar;
extern HWND g_image;

extern HWND g_quit_button;
extern HWND g_patch_button;
extern HWND g_play_button;
extern HWND g_repair_button;

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

void set_text(HWND label, const char *txt);
void set_progress(int percent);
void set_progress_state(unsigned state);
void enable_button(HWND button, bool enable);
void show_ui(HWND ui, bool enable);
void set_banner(const char *path);
void quit_cb(void *arg1, void *arg2);

#endif /* PW_GUI_H */
