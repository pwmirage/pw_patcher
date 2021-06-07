/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdbool.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <assert.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>
#include <time.h>

#include "common.h"
#include "cjson.h"
#include "cjson_ext.h"
#include "serializer.h"
#include "chain_arr.h"
#include "idmap.h"
#include "pw_tasks.h"
#include "pw_elements.h"
#include "pw_tasks_npc.h"
#include "pw_pck.h"
#include "client_ipc.h"

static char *g_branch_name = "public";
static bool g_force_update = false;
static char *g_latest_version_str;
static struct cjson *g_latest_version;
struct pw_version g_version;
static uint32_t g_parent_tid;

struct pw_elements *g_elements;
struct pw_task_file *g_tasks;
struct pw_tasks_npc *g_tasks_npc;

static void
set_text(unsigned label, unsigned txt, unsigned char p1, unsigned char p2, unsigned char p3)
{
	if (!g_parent_tid) {
		return;
	}

	unsigned param = mg_patcher_msg_pack_params(txt, p1, p2, p3);
	PostThreadMessage(g_parent_tid, MG_PATCHER_STATUS_MSG, label, param);
}

static void
set_progress(unsigned progress)
{
	if (!g_parent_tid) {
		return;
	}

	PostThreadMessage(g_parent_tid, MG_PATCHER_STATUS_MSG, MGP_MSG_SET_PROGRESS, progress);
}

static void
enable_button(unsigned button, bool enable)
{
	if (!g_parent_tid) {
		return;
	}

	unsigned cmd = enable ? MGP_MSG_ENABLE_BUTTON : MGP_MSG_DISABLE_BUTTON;
	PostThreadMessage(g_parent_tid, MG_PATCHER_STATUS_MSG, cmd, button);
}

static void
load_icons(void) {
	FILE *fp = fopen("patcher/iconlist_ivtrm.txt", "r");
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	if (fp == NULL) {
		PWLOG(LOG_ERROR, "failed to open iconlist_ivtrm.txt\n");
		return;
	}

	/* skip header */
	for (int i = 0; i < 4; i++) {
		getline(&line, &len, fp);
		line = NULL;
		len = 0;
	}

	unsigned i = 0;
	while (i < PW_ELEMENTS_ICON_COUNT &&
			(read = getline(&line, &len, fp)) != -1) {
		int printed_cnt = snprintf(g_icon_names[i], sizeof(g_icon_names[0]), "%s", line);
		if (g_icon_names[i][printed_cnt - 1] == '\n') {
			g_icon_names[i][printed_cnt - 1] = 0;
		}

		i++;
		len = 0;
	}

	free(line);
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
	if (obj->type != CJSON_TYPE_OBJECT) {
		PWLOG(LOG_ERROR, "found non-object in the patch file (type=%d)\n", obj->type);
		assert(false);
		return;
	}

	const char *type = JSs(obj, "_db", "type");
	long long id = JSi(obj, "id");
	PWLOG(LOG_INFO, "type: %s, id: 0x%llx\n", type, id);

	print_obj(obj->a, 1);

	if (strncmp(type, "spawners_", 9) == 0) {
		pw_tasks_npc_patch_obj(g_tasks_npc, obj);
		return;
	}

	if (strcmp(type, "tasks") == 0) {
		pw_tasks_patch_obj(g_tasks, obj);
		return;
	}

	pw_elements_patch_obj(g_elements, obj);
}

static int
apply_patch(const char *url)
{
	char *buf, *b;
	size_t num_bytes = 1;
	int rc;

	rc = download_mem(url, &buf, &num_bytes);
	if (rc) {
		PWLOG(LOG_ERROR, "download_mem(%s) failed: %d\n", url, rc);
		return rc;
	}

	b = buf;
	do {
		rc = cjson_parse_arr_stream(b, import_stream_cb, NULL);
		/* skip comma and newline */
		b += rc;
		while (*b && *b != '[') b++;
	} while (rc > 0);
	free(buf);
	if (rc < 0) {
		PWLOG(LOG_ERROR, "cjson_parse_arr_stream() failed: %d\n", url, rc);
		return rc;
	}

	return 0;
}

struct open_pw_pck {
	struct pw_pck data;
	struct open_pw_pck *next;
};

struct open_pw_pck *g_open_pcks;

static int
apply_pck_patch(const char *pck_name, const char *url)
{
	struct open_pw_pck *pck;
	int rc;
	char cur_path[512];

	rc = download(url, "patcher/tmp.patch");
	if (rc) {
		PWLOG(LOG_ERROR, "download(%s) failed: %d\n", url, rc);
		return rc;
	}

	pck = g_open_pcks;
	while (pck) {
		if (strcmp(pck->data.fullname, pck_name) == 0) {
			/* already open */
			break;
		}
		pck = pck->next;
	}

	if (!pck) {
		char tmpbuf[128];

		pck = calloc(1, sizeof(*pck));
		if (!pck) {
			PWLOG(LOG_ERROR, "calloc() failed\n");
			return -ENOMEM;
		}

		snprintf(tmpbuf, sizeof(tmpbuf), "element/%s", pck_name);
		rc = pw_pck_open(&pck->data, tmpbuf);
		if (rc) {
			PWLOG(LOG_ERROR, "pw_pck_open() failed: %d\n", rc);
			return rc;
		}

		pck->next = g_open_pcks;
		g_open_pcks = pck;
	}

	GetCurrentDirectory(sizeof(cur_path), cur_path);
	SetCurrentDirectory("element");
	rc = pw_pck_apply_patch(&pck->data, "../patcher/tmp.patch");
	SetCurrentDirectory(cur_path);
	unlink("patcher/tmp.patch");

	return rc;
}

static int
patch(void)
{
	char tmpbuf[1024];
	size_t len;
	int i, rc;

	set_text(MGP_MSG_SET_STATUS_LEFT, MGP_LMSG_FETCHING_CHANGES, 0, 0, 0);
	set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_EMPTY, 0, 0, 0);
	set_progress(0);

	rc = pw_version_load(&g_version);
	if (rc < 0) {
		PWLOG(LOG_ERROR, "pw_version_load() failed with rc=%d\n", rc);
		set_text(MGP_MSG_SET_STATUS_LEFT, MGP_LMSG_LOADING_FILES, 0, 0, 0);
		set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_FAILED_LOADING, 0, 0, 0);
		return rc;
	}
	PWLOG(LOG_INFO, "Version: %u, Generation: %u\n", g_version.version ,g_version.generation);

	load_icons();

	if (strcmp(g_version.branch, g_branch_name) != 0) {
		PWLOG(LOG_INFO, "new branch detected, forcing a fresh update\n");
		g_force_update = true;
	}

	if (g_force_update) {
		g_version.generation = 0;
		snprintf(g_version.branch, sizeof(g_version.branch), "%s", g_branch_name);
	}

	snprintf(tmpbuf, sizeof(tmpbuf), "https://pwmirage.com/editor/project/fetch/%s/since/%u/%u",
			g_branch_name, g_version.version, g_version.generation);
	rc = download_mem(tmpbuf, &g_latest_version_str, &len);
	if (rc) {
		PWLOG(LOG_ERROR, "version fetch failed: %s\n", tmpbuf);
		set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_CANT_FETCH, 0, 0, 0);
		return rc;
	}

	g_latest_version = cjson_parse(g_latest_version_str);
	if (!g_latest_version) {
		PWLOG(LOG_ERROR, "cjson_parse() failed\n");
		set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_CANT_PARSE, 0, 0, 0);
		return -ENOMEM;
	}


	struct cjson *updates = JS(g_latest_version, "updates");
	struct cjson *pck_updates = JS(g_latest_version, "pck_updates");

	if (updates->count == 0 && pck_updates->count == 0) {
		PWLOG(LOG_INFO, "Already up to date!\n");
		set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_ALREADY_UPTODATE, 0, 0, 0);
		set_progress(100);
		/* nothing to do */
		return 0;
	}

	if (!JSi(g_latest_version, "cumulative")) {
		g_force_update = true;
	}

	set_text(MGP_MSG_SET_STATUS_LEFT, MGP_LMSG_LOADING_FILES, 0, 0, 0);
	set_progress(5);

	if (g_force_update || updates->count) {
		g_elements = calloc(1, sizeof(*g_elements));
		if (!g_elements) {
			PWLOG(LOG_ERROR, "calloc() failed\n");
			set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_ALLOC_FAILED, 0, 0, 0);
			return -ENOMEM;
		}

		g_tasks = calloc(1, sizeof(*g_tasks));
		if (!g_tasks) {
			PWLOG(LOG_ERROR, "calloc() failed\n");
			set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_ALLOC_FAILED, 0, 0, 0);
			return -ENOMEM;
		}

		const char *elements_path;
		const char *tasks_path;
		const char *tasks_npc_path;

		if (g_force_update) {
			elements_path = "patcher/elements.data.src";
			tasks_path = "patcher/tasks.data.src";
			tasks_npc_path = "patcher/task_npc.data.src";
		} else {
			elements_path = "element/data/elements.data";
			tasks_path = "element/data/tasks.data";
			tasks_npc_path = "element/data/task_npc.data";
		}

		rc = pw_elements_load(g_elements, elements_path, "patcher/elements.imap");
		if (rc != 0) {
			set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_ELEMENTS_PARSING_FAILED, -rc, 0, 0);
			return rc;
		}
		set_progress(15);

		rc = pw_tasks_load(g_tasks, tasks_path, "patcher/tasks.imap");
		if (rc != 0) {
			set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_TASKS_PARSING_FAILED, -rc, 0, 0);
			return rc;
		}

		g_tasks_npc = pw_tasks_npc_load(tasks_npc_path);
		if (!g_tasks_npc) {
			set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_TASK_NPC_PARSING_FAILED, -rc, 0, 0);
			return rc;
		}
	}

	if (g_force_update) {
		char *buf;
		size_t num_bytes = 0;

		snprintf(tmpbuf, sizeof(tmpbuf), "https://pwmirage.com/editor/project/cache/%s/rates.json",
				g_branch_name);

		rc = download_mem(tmpbuf, &buf, &num_bytes);
		if (rc) {
			PWLOG(LOG_ERROR, "download_mem failed for %s\n", tmpbuf);
			return 1;
		}

		struct cjson *rates = cjson_parse(buf);
		pw_elements_adjust_rates(g_elements, rates);
		pw_tasks_adjust_rates(g_tasks, rates);
		cjson_free(rates);
		free(buf);
	}

	set_progress(25);
	set_text(MGP_MSG_SET_STATUS_LEFT, MGP_LMSG_DOWNLOADING_CHANGES, 0, 0, 0);

	const char *origin = JSs(g_latest_version, "origin");
	for (i = 0; i < updates->count; i++) {
		struct cjson *update = JS(updates, i);
		bool is_cached = JSi(update, "cached");
		const char *hash = JSs(update, "hash");

		PWLOG(LOG_INFO, "Fetching patch \"%s\" ...\n", JSs(update, "name"));
		set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_DOWNLOADING_N_OF_N, i + 1, updates->count + pck_updates->count, 0);
		snprintf(tmpbuf, sizeof(tmpbuf), "Downloading patch %d of %d", i + 1, updates->count);

		if (is_cached) {
			snprintf(tmpbuf, sizeof(tmpbuf), "%s/cache/%s/%s.json", origin, g_branch_name, hash);
		} else {
			snprintf(tmpbuf, sizeof(tmpbuf), "%s/uploads/%s.json", origin, hash);
		}

		rc = apply_patch(tmpbuf);
		if (rc) {
			set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_PATCHING_FAILED, -rc, 0, 0);
			PWLOG(LOG_ERROR, "Failed to patch: %d\n", rc);
			return rc;
		}

		set_progress(25 + 50 * (i + 1) / (updates->count + pck_updates->count));
	}

	for (i = 0; i < pck_updates->count; i++) {
		struct cjson *pck_update = JS(pck_updates, i);
		const char *pck_name = JSs(pck_update, "pck");
		const char *url = JSs(pck_update, "url");

		PWLOG(LOG_INFO, "Fetching patch \"%s\" ...\n", JSs(pck_update, "name"));
		set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_DOWNLOADING_N_OF_N, updates->count + i + 1, updates->count + pck_updates->count, 0);
		snprintf(tmpbuf, sizeof(tmpbuf), "Downloading pck patch %d of %d", i + 1, pck_updates->count);

		rc = apply_pck_patch(pck_name, url);
		if (rc) {
			set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_PATCHING_FAILED, -rc, 0, 0);
			PWLOG(LOG_ERROR, "Failed to patch: %d\n", rc);
			return rc;
		}

		set_progress(25 + 50 * (updates->count + i + 1) / (updates->count + pck_updates->count));
	}


	PWLOG(LOG_INFO, "Saving.\n");
	set_text(MGP_MSG_SET_STATUS_LEFT, MGP_LMSG_SAVING_FILES, 0, 0, 0);
	set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_EMPTY, 0, 0, 0);
	set_progress(75);

	rc = save_serverlist();
	if (rc) {
		PWLOG(LOG_ERROR, "Failed to save the serverlist\n");
		set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_SAVING_FAILED, 1, -rc, 0);
		return rc;
	}

	if (g_force_update || updates->count) {
		rc = pw_elements_save(g_elements, "element/data/elements.data", false);
		if (rc) {
			PWLOG(LOG_ERROR, "Failed to save the elements\n");
			set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_SAVING_FAILED, 2, -rc, 0);
			return rc;
		}
		set_progress(85);

		rc = pw_tasks_save(g_tasks, "element/data/tasks.data", false);
		if (rc) {
			PWLOG(LOG_ERROR, "Failed to save the tasks\n");
			set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_SAVING_FAILED, 3, -rc, 0);
			return rc;
		}

		set_progress(95);

		rc = pw_tasks_npc_save(g_tasks_npc, "element/data/task_npc.data");
		if (rc) {
			PWLOG(LOG_ERROR, "Failed to save task_npc\n");
			set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_SAVING_FAILED, 4, -rc, 0);
			return rc;
		}
	}

	PWLOG(LOG_INFO, "All done\n");
	set_text(MGP_MSG_SET_STATUS_LEFT, MGP_LMSG_READY_TO_PLAY, 0, 0, 0);
	set_text(MGP_MSG_SET_STATUS_RIGHT, MGP_RMSG_PATCHING_DONE, 0, 0, 0);

	set_progress(100);

	g_version.version = JSi(g_latest_version, "version");
	g_version.generation = JSi(g_latest_version, "generation");
	snprintf(g_version.branch, sizeof(g_version.branch), "%s", g_branch_name);
	pw_version_save(&g_version);

	return 0;
}

static void
print_time(void)
{
	time_t timer;
	char buffer[26];
	struct tm* tm_info;

	timer = time(NULL);
	tm_info = localtime(&timer);

	strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
	PWLOG(LOG_INFO, "Patcher starting: %s\n", buffer);
}

int
main(int argc, char *argv[])
{
	bool docrash = false;

	if (access("patcher", F_OK) != 0) {
		return 1;
	}

	setlocale(LC_ALL, "en_US.UTF-8");
	freopen("patcher/patch.log", "w", stderr);

	print_time();

	char **a = argv;
	while (argc > 0) {
		if (argc >= 2 && (strcmp(*a, "-b") == 0 || strcmp(*a, "-branch") == 0)) {
			g_branch_name = *(a + 1);
			a++;
			argc--;
		} else if (argc >= 2 && (strcmp(*a, "-t") == 0 || strcmp(*a, "-tid") == 0)) {
			sscanf(*(a + 1), "%"SCNu32, &g_parent_tid);
			PWLOG(LOG_INFO, "got tid: %u\n", g_parent_tid);
			a++;
			argc--;
		} else if (strcmp(*a, "--fresh") == 0) {
			g_force_update = true;
		} else if (strcmp(*a, "--docrash") == 0) {
			docrash = true;
		}

		a++;
		argc--;
	}

	if (!docrash) {
		SetErrorMode(SEM_FAILCRITICALERRORS);
	}

	if (g_parent_tid) {
		PostThreadMessage(g_parent_tid, MG_PATCHER_STATUS_MSG, 0, 0);
	}

	PWLOG(LOG_INFO, "Using branch: %s\n", g_branch_name);
	return patch() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
