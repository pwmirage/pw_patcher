/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdbool.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>

#include "common.h"
#include "cjson.h"
#include "cjson_ext.h"
#include "gui.h"

#define MG_GUI_ID_QUIT 1
#define MG_GUI_ID_PATCH 2
#define MG_GUI_ID_PLAY 3
#define MG_GUI_ID_REPAIR 4

int calc_sha1_hash(const char *path, char *hash_buf, size_t buflen);

static char *g_branch_name = "public";
static bool g_patcher_outdated = false;
static bool g_force_update = false;
static char *g_latest_version_str;
static struct cjson *g_latest_version;
struct pw_version g_version;
static bool g_quickupdate = false;

struct pw_elements *g_elements;
struct pw_task_file *g_tasks;
struct pw_tasks_npc *g_tasks_npc;

DWORD __stdcall
task_cb(void *arg)
{
	struct task_ctx ctx, *org_ctx = arg;

	ctx = *org_ctx;
	free(org_ctx);
	ctx.cb(ctx.arg1, ctx.arg2);
	return 0;
}

void
task(mg_callback cb, void *arg1, void *arg2)
{
	DWORD tid;
	struct task_ctx *ctx = malloc(sizeof(*ctx));
	assert(ctx != NULL);

	ctx->cb = cb;
	ctx->arg1 = arg1;
	ctx->arg2 = arg2;
	CreateThread(NULL, 0, task_cb, ctx, 0, &tid);
}

static int
check_deps(void)
{
	HKEY hKey;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x86", 0, KEY_READ, &hKey);

	DWORD dwBufferSize = sizeof(DWORD);
	DWORD nResult = 0;

	RegQueryValueEx(hKey, "Installed", 0, NULL, (LPBYTE)&nResult, &dwBufferSize);
	return nResult == 0 ? -1 : 0;
}

void patch_cb(void *arg1, void *arg2);

void
on_init(int argc, char *argv[])
{
	char tmpbuf[1024];
	size_t len;
	int i, rc;

	char **a = argv;
	while (argc > 0) {
		if (argc >= 2 && (strcmp(*a, "-b") == 0 || strcmp(*a, "-branch") == 0)) {
			g_branch_name = *(a + 1);
			a++;
			argc--;
		} else if (strcmp(*a, "--quickupdate") == 0) {
			g_quickupdate = true;
		}

		a++;
		argc--;
	}

	set_text(g_status_left_lbl, "Reading local version ...");

	rc = pw_version_load(&g_version);
	if (rc < 0) {
		PWLOG(LOG_ERROR, "pw_version_load() failed with rc=%d\n", rc);
		set_text(g_status_right_lbl, "Failed. Invalid file permissions?");
		set_progress_state(PBST_ERROR);
		set_progress(100);
		return;
	}

	if (g_quickupdate) {
		g_branch_name = g_version.branch;
	}

	if (strcmp(g_version.branch, g_branch_name) != 0) {
		PWLOG(LOG_INFO, "new branch detected, forcing a fresh update\n");
		g_force_update = true;
		g_version.generation = 0;
		snprintf(g_version.branch, sizeof(g_version.branch), "%s", g_branch_name);
		snprintf(g_version.cur_hash, sizeof(g_version.cur_hash), "0");
	}

	if (access("patcher", F_OK) != 0) {
		set_progress_state(PBST_ERROR);
		set_progress(100);
		MessageBox(0, "Can't find the \"patcher\" directory. Please redownload the full client.", "Error", MB_OK);
		ui_thread(quit_cb, NULL, NULL);
		return;
	}

	set_text(g_status_left_lbl, "Fetching latest version ...");

	snprintf(tmpbuf, sizeof(tmpbuf), "https://pwmirage.com/editor/project/fetch/%s/since/%s/%u",
			g_branch_name, g_version.cur_hash, g_version.generation);

	rc = download_mem(tmpbuf, &g_latest_version_str, &len);
	if (rc) {
		set_text(g_status_right_lbl, "Can't fetch patch list");
		set_progress_state(PBST_ERROR);
		set_progress(100);
		return;
	}

	g_latest_version = cjson_parse(g_latest_version_str);
	if (!g_latest_version) {
		set_text(g_status_right_lbl, "Can't parse patch list");
		set_progress_state(PBST_ERROR);
		set_progress(100);
		return;
	}

	char *motd = JSs(g_latest_version, "message");
	normalize_json_string(motd, false);
	set_text(g_changelog_lbl, motd);

	set_text(g_status_left_lbl, "Checking prerequisites ...");

	struct cjson *files = JS(g_latest_version, "files");
	for (i = 0; i < files->count; i++) {
		struct cjson *file = JS(files, i);
		const char *name = JSs(file, "name");
		const char *sha = JSs(file, "sha256");
		const char *url = JSs(file, "url");

		set_progress_state(PBST_NORMAL);
		set_progress(100 * i / files->count);

		/* no hidden files and no directories (also no ../) */
		if (name[0] == '.' || strstr(name, "/") || strstr(name, "\\")) {
			PWLOG(LOG_ERROR, "Invalid characters in the filename of a patcher file: %s\n", name);
			continue;
		}

		char namebuf[280];
		if (strcmp(name, "game.exe") == 0) {
			snprintf(namebuf, sizeof(namebuf), "element/%s", name);
		} else if (strcmp(name, "calibrib.ttf") == 0) {
			snprintf(namebuf, sizeof(namebuf), "element/fonts/%s", name);
		} else {
			snprintf(namebuf, sizeof(namebuf), "patcher/%s", name);
		}

		tmpbuf[0] = 0;
		rc = calc_sha1_hash(namebuf, tmpbuf, sizeof(tmpbuf));
		if (rc == 0 && strcmp(tmpbuf, sha) == 0) {
			/* nothing to update */
			continue;
		}

		PWLOG(LOG_INFO, "sha mismatch on %s. expected=%s, got=%s\n", name, sha, tmpbuf);

		snprintf(tmpbuf, sizeof(tmpbuf), "Downloading %s", name);
		set_text(g_status_right_lbl, tmpbuf);

		rc = download(url, namebuf);
		if (rc != 0) {
			char errmsg[512];
			snprintf(errmsg, sizeof(errmsg), "Failed to download \"%s\" from the server. Do you want to retry?", namebuf);

			rc = MessageBox(0, errmsg, "Error", MB_YESNO);
			if (rc == IDYES) {
				set_progress_state(PBST_PAUSED);
				i--;
				continue;
			} else {
				set_progress_state(PBST_ERROR);
				set_progress(100);
				snprintf(errmsg, sizeof(errmsg), "Failed to download \"%s\".", namebuf);
				set_text(g_status_right_lbl, errmsg);
				return;
			}
		}

		if (strcmp(namebuf, "patcher/banner") == 0) {
			set_banner(namebuf);
		}
	}

	if (JSi(g_latest_version, "launcher_version") >= 17) {
		set_progress_state(PBST_PAUSED);

		g_patcher_outdated = true;
		set_text(g_patch_button, "Update");
		enable_button(g_patch_button, true);
		set_text(g_status_right_lbl, "Launcher outdated. Click the update button or download at pwmirage.com/launcher");
		rc = MessageBox(0, "New version of the launcher is available! "
				"Would you like to download it now?", "Launcher Update", MB_YESNO);
		if (rc == IDYES) {
			ShellExecute(NULL, NULL, "patcher/updater.exe", NULL, NULL, SW_SHOW);
			ui_thread(quit_cb, NULL, NULL);
			return;
		}
		return;
	}

	set_text(g_status_right_lbl, "");

	if (access("element/d3d8.dll", F_OK) != 0) {
		rc = download("https://github.com/crosire/d3d8to9/releases/latest/download/d3d8.dll", "element/d3d8.dll");
		if (rc != 0) {
			MessageBox(0, "Failed to download d3d8.dll. Perfect World might not run to its full potential", "Status", MB_OK);
		}
	}

	rc = check_deps();
	set_progress(100);

	if (rc != 0) {
		set_progress_state(PBST_PAUSED);
		set_text(g_status_right_lbl, "Missing Visual Studio C++ Redistributable 2019 x86");
		int install = MessageBox(NULL, "Missing Visual Studio C++ Redistributable 2019 x86.\nWould you like to download now?", "Status", MB_YESNO);
		if (install == IDYES) {
				rc = download("https://aka.ms/vs/16/release/vc_redist.x86.exe", "patcher/vc_redist.x86.exe");
				if (rc != 0) {
					set_text(g_status_right_lbl, "vc_redist.x86 download failed!");
					return;
				}

				STARTUPINFO si = {};
				PROCESS_INFORMATION pi = {};

				si.cb = sizeof(si);
				if(!CreateProcess("patcher/vc_redist.x86.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
					set_text(g_status_right_lbl, "vc_redist.x86 execution failed!");
					return;
				}

				show_ui(g_win, false);
				WaitForSingleObject(pi.hProcess, INFINITE);
				CloseHandle(pi.hProcess);
				show_ui(g_win, true);

				if (check_deps() != 0) {
					set_progress_state(PBST_ERROR);
					set_text(g_status_right_lbl, "vc_redist.x86 installation failed!");
					return;
				}
		} else {
				return;
		}
	}

	char msg[256];
	snprintf(msg, sizeof(msg), "Current version: %d. Latest: %d", g_version.version, (int)JSi(g_latest_version, "version"));
	PWLOG(LOG_INFO, "%s\n", msg);
	set_text(g_status_left_lbl, msg);

	if (g_version.version == JSi(g_latest_version, "version")) {
		set_progress_state(PBST_NORMAL);
		set_text(g_status_right_lbl, "Ready to launch");
		enable_button(g_play_button, true);
	} else {
		set_progress_state(PBST_PAUSED);
		set_text(g_status_right_lbl, "Update is required");
		enable_button(g_patch_button, true);
	}
	enable_button(g_repair_button, true);

	if (g_quickupdate) {
		patch_cb(NULL, NULL);
	}
}

void
on_fini(void)
{
	if (g_latest_version) {
		cjson_free(g_latest_version);
	}
	free(g_latest_version_str);
	g_latest_version_str = NULL;
}

static HMODULE
inject_dll(DWORD pid, char *path_to_dll)
{
	HANDLE proc;
	HANDLE thr;
	char buf[64];
	LPVOID ext_path_to_dll;
	LPVOID load_lib_winapi_addr;
	HMODULE injected_dll = NULL;
	DWORD thr_state;
	int rc;

	proc = OpenProcess(PROCESS_ALL_ACCESS, 0,pid);
	if(proc == NULL) {
		goto err;
	}

	load_lib_winapi_addr = (LPVOID)GetProcAddress(
		GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
	if (load_lib_winapi_addr == NULL) {
		goto err;
	}

	ext_path_to_dll = (LPVOID)VirtualAllocEx(proc, NULL,
		strlen(path_to_dll), MEM_RESERVE|MEM_COMMIT,
		PAGE_READWRITE);
	if (ext_path_to_dll == NULL) {
		goto err;
	}

	rc = WriteProcessMemory(proc, (LPVOID)ext_path_to_dll,
		path_to_dll, strlen(path_to_dll), NULL);
	if (rc == 0) {
		goto err_free;
	}

	thr = CreateRemoteThread(proc, NULL, 0,
		(LPTHREAD_START_ROUTINE)load_lib_winapi_addr,
		(LPVOID)ext_path_to_dll, 0, NULL);
	if (thr == NULL) {
		goto err_free;
	}

	while(GetExitCodeThread(thr, &thr_state)) {
		if(thr_state != STILL_ACTIVE) {
			injected_dll = (HMODULE)thr_state;
			break;
		}
	}

	CloseHandle(thr);
	CloseHandle(proc);

	return injected_dll;

err_free:
	VirtualFreeEx(proc, ext_path_to_dll, 0, MEM_RELEASE);
err:
	snprintf(buf, sizeof(buf), "Failed to open PW process. "
		"Was it closed prematurely?");
	MessageBoxA(NULL, buf, "Loader", MB_OK);
	return NULL;
}

void
patch_cb(void *arg1, void *arg2)
{
	char tmpbuf[1024];
	int rc;

	set_progress_state(PBST_NORMAL);
	set_progress(0);

	enable_button(g_play_button, false);
	if (g_patcher_outdated) {
		set_progress_state(PBST_PAUSED);
		set_progress(100);
		rc = MessageBox(0, "New version of the patcher is available! "
				"Would you like to download it now?", "Patcher Update", MB_YESNO);
		if (rc == IDYES) {
			ShellExecute(NULL, NULL, "patcher/updater.exe", NULL, NULL, SW_SHOW);
			ui_thread(quit_cb, NULL, NULL);
			return;
		}
		return;
	}

	enable_button(g_patch_button, false);
	set_text(g_status_right_lbl, "Loading local files");

	STARTUPINFO si = {};
	PROCESS_INFORMATION pi = {};

	si.cb = sizeof(si);
	PWLOG(LOG_INFO, "tid: %u\n", g_win_tid);
	snprintf(tmpbuf, sizeof(tmpbuf), "patcher/patcher.exe -branch %s -tid %u %s", g_branch_name, (unsigned)g_win_tid, g_force_update ? "--fresh" : "");
	if(!CreateProcess(NULL, tmpbuf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		set_text(g_status_right_lbl, "Failed to start the patcher!");
		set_progress_state(PBST_ERROR);
		set_progress(100);
		goto err_retry;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD exit_code;
	GetExitCodeProcess(pi.hProcess, &exit_code);
	CloseHandle(pi.hProcess);

	if (exit_code != EXIT_SUCCESS) {
		PWLOG(LOG_ERROR, "Patcher failed with rc=0x%x\n", exit_code);
		set_progress_state(PBST_ERROR);
		set_progress(100);

		if (exit_code != EXIT_FAILURE) {
			set_text(g_status_right_lbl, "Patcher failed with an unexpected error. Check the log file for details");
		}
		goto err_retry;
	}

	int written = snprintf(tmpbuf, sizeof(tmpbuf), "Current version: %d. ", (int)JSi(g_latest_version, "version"));
	snprintf(tmpbuf + written, sizeof(tmpbuf) - written, "Latest: %d", (int)JSi(g_latest_version, "version"));
	set_text(g_status_left_lbl, tmpbuf);

	enable_button(g_patch_button, false);
	enable_button(g_play_button, true);

	return;

err_retry:
	set_progress(0);
	enable_button(g_patch_button, true);
}

void
repair_cb(void *arg1, void *arg2)
{
	g_force_update = true;
	patch_cb(arg1, arg2);
}

void
on_button_click(int btn)
{
	if (btn == MG_GUI_ID_PLAY) {
		STARTUPINFO pw_proc_startup_info = {0};
		PROCESS_INFORMATION pw_proc_info = {0};
		BOOL result = FALSE;

		SetCurrentDirectory("element");
		pw_proc_startup_info.cb = sizeof(STARTUPINFO);
		result = CreateProcess(NULL, "game.exe game:mpw",
				NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL,
				&pw_proc_startup_info, &pw_proc_info);
		if(!result) {
			MessageBox(0, "Could not start the PW process", "Error", MB_ICONERROR);
			return;
		}

		inject_dll(pw_proc_info.dwProcessId, "../patcher/gamehook.dll");
		ResumeThread(pw_proc_info.hThread);
		PostQuitMessage(0);
	}

	if (btn == MG_GUI_ID_QUIT) {
		PostQuitMessage(0);
		return;
	}

	if (btn == MG_GUI_ID_PATCH) {
		task(patch_cb, NULL, NULL);
	}

	if (btn == MG_GUI_ID_REPAIR) {
		task(repair_cb, NULL, NULL);
	}
}
