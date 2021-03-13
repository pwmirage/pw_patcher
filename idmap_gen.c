/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

#include "common.h"
#include "cjson.h"
#include "cjson_ext.h"
#include "serializer.h"
#include "chain_arr.h"
#include "idmap.h"
#include "pw_tasks.h"
#include "pw_elements.h"

static struct pw_elements *g_elements;
static struct pw_task_file *g_tasks;

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

	if (strncmp(type, "spawners_", 9) == 0) {
		/* skip those */
	} else {
		pw_elements_patch_obj(g_elements, obj);
	}
}

int
main(int argc, char *argv[])
{
	char tmpbuf[1024];
	char *buf;
	size_t num_bytes = 0;
	int rc;

	setlocale(LC_ALL, "en_US.UTF-8");

	if (argc < 3) {
		printf("./%s branch_name hash\n", argv[0]);
		return 0;
	}

	const char *branch_name = argv[1];
	const char *hash = argv[2];
	snprintf(tmpbuf, sizeof(tmpbuf), "cache/%s/%s.json", branch_name, hash);
	rc = readfile(tmpbuf, &buf, &num_bytes);
	if (rc) {
		PWLOG(LOG_ERROR, "readfile failed for %s\n", branch_name);
		return 1;
	}

	g_elements = calloc(1, sizeof(*g_elements));
	if (!g_elements) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		return 1;
	}

	g_tasks = calloc(1, sizeof(*g_tasks));
	if (!g_tasks) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		return 1;
	}

	const char *elements_path = "patcher/elements.data.src";
	const char *tasks_path = "patcher/tasks.data.src";

	snprintf(tmpbuf, sizeof(tmpbuf), "cache/%s/tasks.imap", branch_name);
	rc = pw_tasks_load(g_tasks, tasks_path, tmpbuf);
	if (rc != 0) {
		PWLOG(LOG_ERROR, "pw_tasks_load(\"%s\") failed: %d\n", elements_path, rc);
		return 1;
	}

	snprintf(tmpbuf, sizeof(tmpbuf), "cache/%s/elements.imap", branch_name);
	rc = pw_elements_load(g_elements, elements_path, tmpbuf);
	if (rc != 0) {
		PWLOG(LOG_ERROR, "pw_elements_load(\"%s\") failed: %d\n", elements_path, rc);
		return 1;
	}

	char *b = buf;
	do {
		rc = cjson_parse_arr_stream(b, import_stream_cb, NULL);
		/* skip comma and newline */
		b += rc;
		while (*b && *b != '[') b++;
	} while (rc > 0);

	free(buf);
	if (rc < 0) {
		PWLOG(LOG_ERROR, "cjson_parse_arr_stream() failed: %d\n", rc);
		return 1;
	}

	snprintf(tmpbuf, sizeof(tmpbuf), "cache/%s/elements.imap", branch_name);
	pw_elements_idmap_save(g_elements, tmpbuf);
	snprintf(tmpbuf, sizeof(tmpbuf), "cache/%s/tasks.imap", branch_name);
	pw_tasks_idmap_save(g_tasks, tmpbuf);

	free(g_elements);
	free(g_tasks);

	return 0;
}
