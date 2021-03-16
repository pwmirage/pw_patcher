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

static char *g_branch_name = "public";
static bool g_force_update = false;
static char *g_latest_version_str;
static struct cjson *g_latest_version;
struct pw_version g_version;
static uint32_t g_parent_tid;

struct pw_elements *g_elements;
struct pw_task_file *g_tasks;
struct pw_tasks_npc *g_tasks_npc;

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
		return 1;
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
		return 1;
	}

	return 0;
}

static int
patch(void)
{
	char tmpbuf[1024];
	size_t len;
	int i, rc;

	rc = pw_version_load(&g_version);
	if (rc < 0) {
		PWLOG(LOG_ERROR, "pw_version_load() failed with rc=%d\n", rc);
		return rc;
	}

	if (strcmp(g_version.branch, g_branch_name) != 0) {
		PWLOG(LOG_INFO, "new branch detected, forcing a fresh update\n");
		g_force_update = true;
		g_version.generation = 0;
		snprintf(g_version.branch, sizeof(g_version.branch), "%s", g_branch_name);
		snprintf(g_version.cur_hash, sizeof(g_version.cur_hash), "0");
	}


	snprintf(tmpbuf, sizeof(tmpbuf), "https://pwmirage.com/editor/project/fetch/%s/since/%s/%u",
			g_branch_name, g_version.cur_hash, g_version.generation);

	rc = download_mem(tmpbuf, &g_latest_version_str, &len);
	if (rc) {
		PWLOG(LOG_ERROR, "version fetch failed: %s\n", tmpbuf);
		return rc;
	}

	g_latest_version = cjson_parse(g_latest_version_str);
	if (!g_latest_version) {
		PWLOG(LOG_ERROR, "cjson_parse() failed\n");
		return -ENOMEM;
	}

	g_elements = calloc(1, sizeof(*g_elements));
	if (!g_elements) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		return -ENOMEM;
	}

	g_tasks = calloc(1, sizeof(*g_tasks));
	if (!g_tasks) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		return -ENOMEM;
	}

	if (!JSi(g_latest_version, "cumulative")) {
		g_force_update = true;
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
		return rc;
	}

	rc = pw_tasks_load(g_tasks, tasks_path, "patcher/tasks.imap");
	if (rc != 0) {
		return rc;
	}

	g_tasks_npc = pw_tasks_npc_load(tasks_npc_path);
	if (!g_tasks_npc) {
		return rc;
	}

	const char *origin = JSs(g_latest_version, "origin");
	struct cjson *updates = JS(g_latest_version, "updates");
	const char *last_hash = NULL;

	if (updates->count == 0) {
		PWLOG(LOG_INFO, "Already up to date!\n");
		/* nothing to do */
		return 0;
	}

	if (g_force_update) {
		char *buf;
		size_t num_bytes = 0;

		snprintf(tmpbuf, sizeof(tmpbuf), "https://pwmirage.com/editor/project/cache/%s/rates.json",
				g_branch_name);

		rc = download_mem(tmpbuf, &buf, &num_bytes);
		if (rc) {
			PWLOG(LOG_ERROR, "rates download failed!\n");
			return rc;
		}

		struct cjson *rates = cjson_parse(buf);
		pw_elements_adjust_rates(g_elements, rates);
		pw_tasks_adjust_rates(g_tasks, rates);
		cjson_free(rates);
		free(buf);
	}

	for (i = 0; i < updates->count; i++) {
		struct cjson *update = JS(updates, i);
		bool is_cached = JSi(update, "cached");
		last_hash = JSs(update, "hash");

		PWLOG(LOG_INFO, "Fetching patch \"%s\" ...\n", JSs(update, "name"));
		snprintf(tmpbuf, sizeof(tmpbuf), "Downloading patch %d of %d", i + 1, updates->count);

		if (is_cached) {
			snprintf(tmpbuf, sizeof(tmpbuf), "%s/cache/%s/%s.json", origin, g_branch_name, last_hash);
		} else {
			snprintf(tmpbuf, sizeof(tmpbuf), "%s/uploads/%s.json", origin, last_hash);
		}

		rc = apply_patch(tmpbuf);
		if (rc) {
			PWLOG(LOG_ERROR, "Failed to patch\n");
			return rc;
		}
	}

	PWLOG(LOG_INFO, "Saving.\n");

	rc = save_serverlist();
	if (rc) {
		PWLOG(LOG_ERROR, "Failed to save the serverlist\n");
		return rc;
	}

	rc = pw_elements_save(g_elements, "element/data/elements.data", false);
	if (rc) {
		PWLOG(LOG_ERROR, "Failed to save the elements\n");
		return rc;
	}

	rc = pw_tasks_save(g_tasks, "element/data/tasks.data", false);
	if (rc) {
		PWLOG(LOG_ERROR, "Failed to save the tasks\n");
		return rc;
	}

	rc = pw_tasks_npc_save(g_tasks_npc, "element/data/task_npc.data");
	if (rc) {
		PWLOG(LOG_ERROR, "Failed to save task_npc\n");
		return rc;
	}

	PWLOG(LOG_INFO, "All done\n");

	g_version.version = JSi(g_latest_version, "version");
	snprintf(g_version.branch, sizeof(g_version.branch), "%s", g_branch_name);
	if (last_hash) {
		snprintf(g_version.cur_hash, sizeof(g_version.cur_hash), "%s", last_hash);
	}
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

	char *a = &argv[0];
	while (argc > 0) {
		if (argc >= 2 && (strcmp(a, "-b") == 0 || strcmp(a, "-branch") == 0)) {
			g_branch_name = a + 1;
			a++;
		} else if (argc >= 2 && (strcmp(a, "-t") == 0 || strcmp(a, "-tid") == 0)) {
			sscanf(a + 1, "%"SCNu32, &g_parent_tid);
		} else if (strcmp(a, "--docrash") == 0) {
			docrash = true;
		}

		a++;
		argc--;
	}

	if (!docrash) {
		SetErrorMode(SEM_FAILCRITICALERRORS);
	}

	PWLOG(LOG_INFO, "Using branch: %s\n", g_branch_name);
	return patch() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
