/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_GUI_H
#define PW_GUI_H

typedef void (*mg_callback)(void *arg1, void *arg2);

#define BUTTON_ID_QUIT 1
#define BUTTON_ID_PATCH 2
#define BUTTON_ID_PLAY 3
#define BUTTON_ID_REPAIR 4

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

void on_init(int argc, char *argv[]);
void on_fini();
void ui_thread(mg_callback cb, void *arg1, void *arg2);
void task(mg_callback cb, void *arg1, void *arg2);
void reload_banner(const char *path);
void on_button_click(int btn);

#endif /* PW_GUI_H */
