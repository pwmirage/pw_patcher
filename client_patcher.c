/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdbool.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "common.h"
#include "client.h"
#include "load.c"

static char *g_version_str;
static const nx_json *g_version;
static int g_latest_version;

struct task_ctx {
	mg_callback cb;
	void *arg1;
	void *arg2;
};

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
on_init(void)
{
	char tmpbuf[1024];
	char *buf;
	size_t len;
	int rc;
	FILE *fp;

	setlocale(LC_ALL, "en_US.UTF-8");
	freopen("patcher/patch.log", "w", stderr);

	set_text(g_status_left_lbl, "Reading local version ...");

	unsigned version = 0;
	char cur_hash[64] = {0};
	cur_hash[0] = '0';

	fp = fopen("patcher/version", "rb");
	if (fp) {
		fread(&version, 1, sizeof(version), fp);
		fread(cur_hash, 1, sizeof(cur_hash), fp);
		cur_hash[sizeof(cur_hash) - 1] = 0;
		fclose(fp);
	}

	set_text(g_status_left_lbl, "Fetching latest version ...");
	const char *branch_name = "test1";
	
	snprintf(tmpbuf, sizeof(tmpbuf), "http://miragetest.ddns.net/editor/project/fetch/%s/since/%s",
			branch_name, cur_hash);

	rc = download_mem(tmpbuf, &buf, &len);
	if (rc) {
		PWLOG(LOG_ERROR, "download_mem failed for %s\n", tmpbuf);
		return 1;
	}

	struct cjson *ver_cjson = cjson_parse(buf);
	if (!ver_cjson) {
		set_text(g_status_right_lbl, "Can't fetch patch list");
		return;
	}

	g_version = nx_json_parse_utf8(g_version_str);
	if (!g_version) {
		set_text(g_status_right_lbl, "Can't parse patch list");
		return;
	}

	set_text(g_changelog_lbl, nx_json_get(g_version, "description")->text_value);

	if (nx_json_get(g_version, "version")->int_value > 10) {
		set_text(g_status_right_lbl, "Patcher outdated. Please download new version from pwmirage.com/patcher");
		rc = MessageBox(0, "New version of the patcher is available! "
						"Would you like to download it now?", "Patcher Update", MB_YESNO);
		if (rc == IDYES) {
				ShellExecute(NULL, NULL, "patcher\\updater.exe", NULL, NULL, SW_SHOW);
				ui_thread(quit_cb, NULL, NULL);
				return;
		}
		return;
	}

	const nx_json *patchlist = nx_json_get(g_version, "patchlist");
	const nx_json *last_patch = nx_json_item(patchlist, patchlist->length - 1);
	g_latest_version = nx_json_get(last_patch, "id")->int_value;

	if (local_version != g_latest_version) {
		rc = download("https://raw.githubusercontent.com/pwmirage/version/master/banner.bmp", "patcher/banner");
		if (rc == 0) {
			set_banner("patcher/banner");
		}
	}

	set_text(g_status_left_lbl, "Checking prerequisites ...");
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
	snprintf(msg, sizeof(msg), "Current version: %d. Latest: %d", local_version, g_latest_version);
	fprintf(stderr, "%s\n", msg);
	set_text(g_status_left_lbl, msg);

	if (local_version == g_latest_version) {
		set_text(g_status_right_lbl, "Ready to launch");
		enable_button(g_play_button, true);
	} else {
		set_text(g_status_right_lbl, "Update is required");
		enable_button(g_patch_button, true);
	}
	enable_button(g_repair_button, true);
}

void
on_fini(void)
{
	if (g_version) {
		nx_json_free(g_version);
	}
	free(g_version_str);
	g_version_str = NULL;
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
	struct pw_elements elements;
	struct pw_task_file tasks;
	int rc;

	set_progress(0);

	enable_button(g_patch_button, false);
	set_text(g_status_right_lbl, "Loading local files");

	rc = pw_elements_load(&elements, "patcher/elements.data.src");
	if (rc != 0) {
		set_text(g_status_right_lbl, "elements.data.src not found. Please click the repair button");
		goto err_retry;
	}

	rc = pw_tasks_load(&tasks, "patcher/tasks.data.src");
	if (rc) {
		set_text(g_status_right_lbl, "tasks.data.src not found. Please click the repair button");
		goto err_retry;
	}

	unsigned num_types = PW_ELEMENTS_COUNT(&elements);
	struct list_pool *pool = list_pool_init(num_types, 65536 * 128);
	if (!pool) {
		set_text(g_status_right_lbl, "Failed to allocate memory. Please report this on pwmirage.com");
		goto err_retry;
	}

	set_progress(10);
	set_text(g_status_right_lbl, "Fetching rates...");

	char *rates_str;
	size_t len;
	const nx_json *rates;
	rc = download_mem("https://raw.githubusercontent.com/pwmirage/version/master/rates.json", &rates_str, &len);
	if (rc) {
		set_text(g_status_right_lbl, "Fetching failed! Please try again");
		goto err_retry;
	}

	rates = nx_json_parse_utf8(rates_str);
	if (!rates) {
		set_text(g_status_right_lbl, "Parsing failed! Please try again");
		goto err_retry;
	}

	adjust_rates(&elements, rates);
	pw_tasks_process_rates(&tasks, rates);

	set_text(g_status_right_lbl, "Updating...");

	const nx_json *patchlist = nx_json_get(g_version, "patchlist");
		const nx_json *patch = JS(patchlist, patchlist->length - 1);
		char url[256];
		char *patch_str;


		snprintf(url, sizeof(url), "https://raw.githubusercontent.com/pwmirage/version/master/cache/%s", JSs(patch, "file"));
		fprintf(stderr, "Downloading %s ...", url);
		
		rc = download_mem(url, &patch_str, &len);
		if (rc) {
				set_text(g_status_right_lbl, "Fetching failed! Please try again");
				goto err_retry;
		}

		set_progress(50);

		fprintf(stderr, "done! parsing...");
		const nx_json* json = nx_json_parse_utf8(patch_str);
		if (!json) {
			set_text(g_status_right_lbl, "Parsing failed! Please try again");
			goto err_retry;
		}

		fprintf(stderr, "done! Processing now.\n");
		process_changeset(&elements, pool, json);
		pw_tasks_process_changeset(&elements, &tasks, pool, json);
		set_text(g_status_right_lbl, "Done!");

		int written = snprintf(url, sizeof(url), "Current version: %d. ", JSi(patch, "id"));
		snprintf(url + written, sizeof(url) - written, "Latest: %d", g_latest_version);
		set_text(g_status_left_lbl, url);

		nx_json_free(json);
		free(patch_str);

	set_progress(10 + 80);
	fprintf(stderr, "Saving.\n");
	set_text(g_status_right_lbl, "Saving files...");
	rc = pw_elements_save(&elements, "element/data/elements.data");
	rc = rc ? rc : pw_tasks_save(&tasks, "element/data/tasks.data", true);
	if (rc) {
		set_text(g_status_right_lbl, "Saving failed. No permission to access game directories?");
		return;
	}
	set_text(g_status_right_lbl, "Ready to launch");
	fprintf(stderr, "All done\n");

	set_progress(100);
	enable_button(g_patch_button, false);
	enable_button(g_play_button, true);
	{
		FILE *fp = fopen("patcher/version", "wb");
		fwrite(&g_latest_version, sizeof(g_latest_version), 1, fp);
		fclose(fp);
	}

	return;

err_retry:
	set_progress(0);
	enable_button(g_patch_button, true);
}

void
repair_cb(void *arg1, void *arg2)
{
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
