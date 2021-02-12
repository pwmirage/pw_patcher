/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdbool.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <assert.h>
#include <unistd.h>

#include "common.h"
#include "pw_elements.h"
#include "cjson.h"
#include "cjson_ext.h"
#include "gui.h"

static char *g_branch_name = "public";
static bool g_patcher_outdated = false;
static bool g_force_update = false;
static char *g_latest_version_str;
static struct cjson *g_latest_version;
struct pw_version g_version;

struct task_ctx {
	mg_callback cb;
	void *arg1;
	void *arg2;
};

int calc_sha1_hash(const char *path, char *hash_buf, size_t buflen);

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

static void
set_text_cb(void *_label, void *_txt)
{
	SetWindowText(_label, _txt);
	ShowWindow(_label, SW_HIDE);
	ShowWindow(_label, SW_SHOW);
}

static void
set_text(HWND label, const char *txt)
{
	ui_thread(set_text_cb, label, (void *)txt);
}

static void
set_progress_cb(void *_percent, void *arg2)
{
	SendMessage(g_progress_bar, PBM_SETPOS, (int)(uintptr_t)_percent, 0);
}

static void
set_progress(int percent)
{
	ui_thread(set_progress_cb, (void *)(uintptr_t)percent, NULL);
}

static void
enable_button_cb(void *_button, void *_enable)
{
	EnableWindow(_button, (bool)(uintptr_t)_enable);
}

static void
enable_button(HWND button, bool enable)
{
	ui_thread(enable_button_cb, button, (void *)(uintptr_t)enable);
}

static void
show_ui_cb(void *ui, void *_enable)
{
	ShowWindow(ui, (bool)(uintptr_t)_enable ? SW_SHOW : SW_HIDE);
}

static void
show_ui(HWND ui, bool enable)
{
	ui_thread(show_ui_cb,ui, (void *)(uintptr_t)enable);
}

static void
quit_cb(void *arg1, void *arg2)
{
	PostQuitMessage(0);
}

static void
set_banner_cb(void *path, void *arg2)
{
	reload_banner(path);
}

static void
set_banner(const char *path)
{
	ui_thread(set_banner_cb, (void *)path, NULL);
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


void
on_init(int argc, char *argv[])
{
	char tmpbuf[1024];
	size_t len;
	int i, rc;

	if (argc >= 3) {
		if (strcmp(argv[1], "-b") == 0) {
			g_branch_name = argv[2];
		}
	}

	set_text(g_status_left_lbl, "Reading local version ...");

	rc = pw_version_load(&g_version);
	if (rc < 0) {
		PWLOG(LOG_ERROR, "pw_version_load() failed with rc=%d\n", rc);
		return;
	}

	if (strcmp(g_version.branch, g_branch_name) != 0) {
		PWLOG(LOG_INFO, "new branch detected, forcing a fresh update\n");
		g_force_update = true;
		g_version.version = 0;
		snprintf(g_version.branch, sizeof(g_version.branch), "%s", g_branch_name);
		snprintf(g_version.cur_hash, sizeof(g_version.cur_hash), "0");
	}

	if (access("patcher", F_OK) != 0) {
		MessageBox(0, "Can't find the \"patcher\" directory. Please redownload a full client.", "Error", MB_OK);
		ui_thread(quit_cb, NULL, NULL);
		return;
	}

	set_text(g_status_left_lbl, "Fetching latest version ...");

	snprintf(tmpbuf, sizeof(tmpbuf), "https://pwmirage.com/editor/project/fetch/%s/since/%s/%u",
			g_branch_name, g_version.cur_hash, g_version.version);

	rc = download_mem(tmpbuf, &g_latest_version_str, &len);
	if (rc) {
		set_text(g_status_right_lbl, "Can't fetch patch list");
		return;
	}

	g_latest_version = cjson_parse(g_latest_version_str);
	if (!g_latest_version) {
		set_text(g_status_right_lbl, "Can't parse patch list");
		return;
	}

	char *motd = JSs(g_latest_version, "message");
	normalize_json_string(motd);
	set_text(g_changelog_lbl, motd);

	set_text(g_status_left_lbl, "Checking prerequisites ...");

	struct cjson *files = JS(g_latest_version, "files");
	for (i = 0; i < files->count; i++) {
		struct cjson *file = JS(files, i);
		const char *name = JSs(file, "name");
		const char *sha = JSs(file, "sha256");
		const char *url = JSs(file, "url");

		set_progress(100 * i / files->count);

		/* no hidden files and no directories (also no ../) */
		if (name[0] == '.' || strstr(name, "/")) {
			PWLOG(LOG_ERROR, "Invalid characters in the filename of a patcher file: %s\n", name);
			continue;
		}

		char namebuf[280];
		snprintf(namebuf, sizeof(namebuf), "patcher/%s", name);
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
				i--;
				continue;
			} else {
				snprintf(errmsg, sizeof(errmsg), "Failed to download \"%s\".", namebuf);
				set_text(g_status_right_lbl, errmsg);
				return;
			}
		}

		if (strcmp(namebuf, "patcher/banner") == 0) {
			set_banner(namebuf);
		}
	}

	set_text(g_status_right_lbl, "");

	if (JSi(g_latest_version, "patcher_version") >= 13) {
		g_patcher_outdated = true;
		set_text(g_patch_button, "Update");
		enable_button(g_patch_button, true);
		set_text(g_status_right_lbl, "Patcher outdated. Click the update button or download at pwmirage.com/patcher");
		rc = MessageBox(0, "New version of the patcher is available! "
				"Would you like to download it now?", "Patcher Update", MB_YESNO);
		if (rc == IDYES) {
			ShellExecute(NULL, NULL, "patcher\\updater.exe", NULL, NULL, SW_SHOW);
			ui_thread(quit_cb, NULL, NULL);
			return;
		}
		return;
	}

	if (access("element/d3d8.dll", F_OK) != 0) {
		rc = download("https://github.com/crosire/d3d8to9/releases/latest/download/d3d8.dll", "element/d3d8.dll");
		if (rc != 0) {
			MessageBox(0, "Failed to download d3d8.dll. Perfect World might not run to its full potential", "Status", MB_OK);
		}
	}

	rc = check_deps();
	set_progress(100);

	if (rc != 0) {
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
				if(!CreateProcess("patcher\\vc_redist.x86.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
					set_text(g_status_right_lbl, "vc_redist.x86 execution failed!");
					return;
				}

				show_ui(g_win, false);
				WaitForSingleObject(pi.hProcess, INFINITE);
				CloseHandle(pi.hProcess);
				show_ui(g_win, true);

				if (check_deps() != 0) {
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
		set_text(g_status_right_lbl, "Ready to launch");
		enable_button(g_play_button, true);
	} else {
		set_text(g_status_right_lbl, "Update is required");
		enable_button(g_patch_button, true);
	}
	enable_button(g_repair_button, true);
}

static int
save_serverlist(void)
{
	FILE *fp = fopen("element/userdata/server/serverlist.txt", "wb");
	char buf[128];
	uint16_t wbuf[64] = {0};
	const char *srv_name;
	const char *srv_ip;

	if (!fp) {
		return -errno;
	}


	uint16_t bom = 0xfeff;
	fwrite(&bom, sizeof(bom), 1, fp);

	if (strcmp(g_branch_name, "public") == 0) {
		srv_name = "Mirage France";
		srv_ip = "29000:91.121.91.123";
	} else {
		srv_name = "Mirage Test1";
		srv_ip = "29000:miragetest.ddns.net";
	}
	snprintf(buf, sizeof(buf), "\r\n%s\t%s\t1", srv_name, srv_ip);

	change_charset("UTF-8", "UTF-16LE", buf, sizeof(buf), (char *)wbuf, sizeof(wbuf));
	fwrite(wbuf, strlen(buf) * 2, 1, fp);
	fclose(fp);

	fp = fopen("element/userdata/currentserver.ini", "w");
	if (!fp) {
		/* ignore it */
		return 0;
	}

	fprintf(fp, "[Server]\nCurrentServer=%s\nCurrentServerAddress=%s\nCurrentLine=\n", srv_name, srv_ip);
	fclose(fp);
	return 0;
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

static void
print_obj(struct cjson *obj, int depth)
{
	int i;

	while (obj) {
		for (i = 0; i < depth; i++) {
			fprintf(stderr, "  ");
		}
		fprintf(stderr, "%s\n", obj->key);

		if (obj->type == CJSON_TYPE_OBJECT) {
			print_obj(obj->a, depth + 1);
		}

		obj = obj->next;
	}
}

static void
import_stream_cb(void *ctx, struct cjson *obj)
{
	struct pw_elements *elements = ctx;

	if (obj->type != CJSON_TYPE_OBJECT) {
		PWLOG(LOG_ERROR, "found non-object in the patch file (type=%d)\n", obj->type);
		assert(false);
		return;
	}

	const char *type = JSs(obj, "_db", "type");
	PWLOG(LOG_INFO, "type: %s\n", type);

	print_obj(obj->a, 1);

	if (strncmp(type, "spawners_", 9) == 0) {
		/* server-side only */
		return;
	}

	pw_elements_patch_obj(elements, obj);
}

static int
patch(struct pw_elements *elements, const char *url)
{
	char *buf, *b;
	size_t num_bytes = 1;
	int rc;

	rc = download_mem(url, &buf, &num_bytes);
	if (rc) {
		PWLOG(LOG_ERROR, "download_mem(%s) failed: %d\n", url, rc);
		return 1;
	}

	b = buf;
	do {
		rc = cjson_parse_arr_stream(b, import_stream_cb, elements);
		/* skip comma and newline */
		b += rc;
		while (*b && *b != '[') b++;
	} while (rc > 0);
	free(buf);
	if (rc < 0) {
		PWLOG(LOG_ERROR, "cjson_parse_arr_stream() failed: %d\n", url, rc);
		return 1;
	}

	return 0;
}

void
patch_cb(void *arg1, void *arg2)
{
	struct pw_elements elements = {0};
	struct pw_task_file tasks = {0};
	char tmpbuf[1024];
	int i, rc;

	enable_button(g_play_button, false);
	if (g_patcher_outdated) {
		rc = MessageBox(0, "New version of the patcher is available! "
				"Would you like to download it now?", "Patcher Update", MB_YESNO);
		if (rc == IDYES) {
			ShellExecute(NULL, NULL, "patcher\\updater.exe", NULL, NULL, SW_SHOW);
			ui_thread(quit_cb, NULL, NULL);
			return;
		}
		return;
	}

	set_progress(0);

	enable_button(g_patch_button, false);
	set_text(g_status_right_lbl, "Loading local files");

	if (!JSi(g_latest_version, "cumulative")) {
		g_force_update = true;
	}

	const char *elements_path;
	const char *tasks_path;

	if (g_force_update) {
		elements_path = "patcher/elements.data.src";
		tasks_path = "patcher/tasks.data.src";
	} else {
		elements_path = "element/data/elements.data";
		tasks_path = "element/data/tasks.data";
	}

	set_progress(5);

	rc = pw_elements_load(&elements, elements_path, !g_force_update);
	if (rc != 0) {
		set_text(g_status_right_lbl, "elements file not found. Please redownload the client");
		goto err_retry;
	}

	set_progress(10);

	rc = pw_tasks_load(&tasks, tasks_path);
	if (rc != 0) {
		set_text(g_status_right_lbl, "tasks file not found. Please redownload the client");
		goto err_retry;
	}

	set_progress(15);
	set_text(g_status_right_lbl, "Updating...");

	const char *origin = JSs(g_latest_version, "origin");
	struct cjson *updates = JS(g_latest_version, "updates");
	const char *last_hash = NULL;

	if (updates->count && g_force_update) {
		char *buf;
		size_t num_bytes = 0;

		snprintf(tmpbuf, sizeof(tmpbuf), "https://pwmirage.com/editor/project/cache/%s/rates.json",
				g_branch_name);

		rc = download_mem(tmpbuf, &buf, &num_bytes);
		if (rc) {
			set_text(g_status_right_lbl, "failed to fetch server information");
			goto err_retry;
		}

		struct cjson *rates = cjson_parse(buf);
		pw_elements_adjust_rates(&elements, rates);
		pw_tasks_adjust_rates(&tasks, rates);
		cjson_free(rates);
		free(buf);
	}

	for (i = 0; i < updates->count; i++) {
		struct cjson *update = JS(updates, i);
		bool is_cached = JSi(update, "cached");
		last_hash = JSs(update, "hash");

		PWLOG(LOG_INFO, "Fetching patch \"%s\" ...\n", JSs(update, "name"));
		snprintf(tmpbuf, sizeof(tmpbuf), "Downloading patch %d of %d", i + 1, updates->count);
		set_text(g_status_right_lbl, tmpbuf);

		if (is_cached) {
			snprintf(tmpbuf, sizeof(tmpbuf), "%s/cache/%s/%s.json", origin, g_branch_name, last_hash);
		} else {
			snprintf(tmpbuf, sizeof(tmpbuf), "%s/uploads/%s.json", origin, last_hash);
		}

		rc = patch(&elements, tmpbuf);
		if (rc) {
			PWLOG(LOG_ERROR, "Failed to patch\n");
			set_text(g_status_right_lbl, "Patching failed!");
			return;
		}
	}

	set_text(g_status_right_lbl, "Done!");

	int written = snprintf(tmpbuf, sizeof(tmpbuf), "Current version: %d. ", (int)JSi(g_latest_version, "version"));
	snprintf(tmpbuf + written, sizeof(tmpbuf) - written, "Latest: %d", (int)JSi(g_latest_version, "version"));
	set_text(g_status_left_lbl,tmpbuf);

	set_progress(10 + 80);
	PWLOG(LOG_INFO, "Saving.\n");
	set_text(g_status_right_lbl, "Saving files...");

	rc = save_serverlist();
	if (rc) {
		set_text(g_status_right_lbl, "Saving failed (1). No permission to access game directories?");
		return;
	}

	rc = pw_elements_save(&elements, "element/data/elements.data", false);
	if (rc) {
		set_text(g_status_right_lbl, "Saving failed (2). No permission to access game directories?");
		return;
	}

	rc = pw_tasks_save(&tasks, "element/data/tasks.data", false);
	if (rc) {
		PWLOG(LOG_ERROR, "err: %d\n", rc);
		set_text(g_status_right_lbl, "Saving failed (3). No permission to access game directories?");
		return;
	}

	set_text(g_status_right_lbl, "Ready to launch");
	PWLOG(LOG_INFO, "All done\n");

	set_progress(100);
	enable_button(g_patch_button, false);
	enable_button(g_play_button, true);

	g_version.version = JSi(g_latest_version, "version");
	snprintf(g_version.branch, sizeof(g_version.branch), "%s", g_branch_name);
	if (last_hash) {
		snprintf(g_version.cur_hash, sizeof(g_version.cur_hash), "%s", last_hash);
	}
	pw_version_save(&g_version);

	return;

err_retry:
	set_progress(0);
	enable_button(g_patch_button, true);
}

void
repair_cb(void *arg1, void *arg2)
{
	char tmpbuf[512];
	size_t len;
	int rc;

	set_text(g_status_left_lbl, "Fetching latest version ...");

	snprintf(tmpbuf, sizeof(tmpbuf), "https://pwmirage.com/editor/project/fetch/%s/since/0/%u",
			g_branch_name, g_version.version);

	cjson_free(g_latest_version);
	g_latest_version = NULL;
	free(g_latest_version_str);
	g_latest_version_str = NULL;

	rc = download_mem(tmpbuf, &g_latest_version_str, &len);
	if (rc) {
		set_text(g_status_right_lbl, "Can't fetch patch list");
		return;
	}

	g_latest_version = cjson_parse(g_latest_version_str);
	if (!g_latest_version) {
		set_text(g_status_right_lbl, "Can't parse patch list");
		return;
	}

	g_force_update = true;
	patch_cb(arg1, arg2);
}

void
on_button_click(int btn)
{

	if (btn == BUTTON_ID_PLAY) {
		STARTUPINFO pw_proc_startup_info = {0};
		PROCESS_INFORMATION pw_proc_info = {0};
		BOOL result = FALSE;

		SetCurrentDirectory("element");
		pw_proc_startup_info.cb = sizeof(STARTUPINFO);
		result = CreateProcess(NULL,	"game.exe"
				" game:mpw",
				NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL,
				&pw_proc_startup_info, &pw_proc_info);
		if(!result) {
			MessageBox(0, "Could not start the PW process", "Error", MB_ICONERROR);
			return;
		}

		inject_dll(pw_proc_info.dwProcessId, "..\\patcher\\gamehook.dll");
		ResumeThread(pw_proc_info.hThread);
		PostQuitMessage(0);
	}

	if (btn == BUTTON_ID_QUIT) {
		PostQuitMessage(0);
		return;
	}

	if (btn == BUTTON_ID_PATCH) {
		task(patch_cb, NULL, NULL);
	}

	if (btn == BUTTON_ID_REPAIR) {
		task(repair_cb, NULL, NULL);
	}
}
