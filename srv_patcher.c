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

#include "pw_elements.h"
#include "common.h"
#include "cjson.h"
#include "cjson_ext.h"
#include "pw_npc.h"

static struct pw_elements *g_elements;
static struct pw_npc_file g_npc_files[PW_MAX_MAPS];

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
	PWLOG(LOG_INFO, "type: %s\n", type);

	print_obj(obj->a, 1);

	if (strncmp(type, "spawners_", 9) == 0) {
		struct pw_npc_file *npcfile;
		const char *mapid = type + 9;
		int i;

		for (i = 0; i < PW_MAX_MAPS; i++) {
			npcfile = &g_npc_files[i];

			if (strcmp(npcfile->name, mapid) == 0) {
				break;
			}
		}

		if (i == PW_MAX_MAPS) {
			PWLOG(LOG_ERROR, "found unknown map in the patch file (\"%s\")\n", type);
			assert(false);
			return;
		}

		pw_npcs_patch_obj(npcfile, obj);
	} else {
		pw_elements_patch_obj(g_elements, obj);
	}
}

static int
patch(const char *url)
{
	char *buf;
	char *b;
	ssize_t num_bytes = 1;
	int rc;

	rc = download_mem(url, &buf, (size_t *)&num_bytes);
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

int
main(int argc, char *argv[])
{
	struct pw_version version;
	char tmpbuf[1024];
	char *buf;
	size_t num_bytes = 0;
	int i, rc;

	setlocale(LC_ALL, "en_US.UTF-8");

	if (argc < 2) {
		printf("./%s branch_name\n", argv[0]);
		return 0;
	}

	const char *branch_name = argv[1];
	const char *tmp = getenv("PW_UPDATER_FORCE_FRESH");
	bool force_fresh_update = false;
	if (tmp && strlen(tmp) > 0) {
		PWLOG(LOG_INFO, "PW_UPDATER_FORCE_FRESH set\n");
		force_fresh_update = true;
	}

	rc = pw_version_load(&version);
	if (rc < 0) {
		PWLOG(LOG_ERROR, "pw_version_load() failed with rc=%d\n", rc);
		return 1;
	}

	if (force_fresh_update || strcmp(version.branch, branch_name) != 0) {
		if (!force_fresh_update) {
			PWLOG(LOG_INFO, "different branch detected, forcing a fresh update\n");
		}
		force_fresh_update = true;
		version.version = 0;
		snprintf(version.branch, sizeof(version.branch), "%s", branch_name);
		snprintf(version.cur_hash, sizeof(version.cur_hash), "0");
	}

	g_elements = calloc(1, sizeof(*g_elements));
	if (!g_elements) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		return 1;
	}

	snprintf(tmpbuf, sizeof(tmpbuf), "http://miragetest.ddns.net/editor/project/fetch/%s/since/%s/%u",
			branch_name, version.cur_hash, version.version);

	rc = download_mem(tmpbuf, &buf, &num_bytes);
	if (rc) {
		PWLOG(LOG_ERROR, "download_mem failed for %s\n", tmpbuf);
		return 1;
	}

	struct cjson *ver_cjson = cjson_parse(buf);
	if (!ver_cjson) {
		PWLOG(LOG_ERROR, "cjson_parse() failed\n");
		free(buf);
		return 1;
	}

	struct cjson *updates = JS(ver_cjson, "updates");
	bool is_cumulative = JSi(ver_cjson, "cumulative") && !force_fresh_update;
	const char *elements_path;
	const char *last_hash = NULL;

	if (updates->count) {
		if (is_cumulative) {
			elements_path = "config/elements.data";
		} else {
			elements_path = "patcher/elements.data";
		}

		rc = pw_elements_load(g_elements, elements_path, !is_cumulative);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "pw_elements_load(\"%s\") failed: %d\n", elements_path, rc);
			return 1;
		}

		for (i = 0; i < PW_MAX_MAPS; i++) {
			const struct map_name *map = &g_map_names[i];
			struct pw_npc_file *npc = &g_npc_files[i];
			const char *dir;

			if (is_cumulative) {
				dir = "config";
			} else {
				dir = "patcher";
			}

			snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s/npcgen.data", dir, map->dir_name);
			rc = pw_npcs_load(npc, map->name, tmpbuf, !is_cumulative);
			if (rc) {
				PWLOG(LOG_ERROR, "pw_npcs_load(\"%s\") failed: %d\n", map->name, rc);
				return 1;
			}
		}


		const char *origin = JSs(ver_cjson, "origin");

		for (i = 0; i < updates->count; i++) {
			struct cjson *update = JS(updates, i);
			bool is_cached = JSi(update, "cached");
			last_hash = JSs(update, "hash");

			PWLOG(LOG_INFO, "Fetching patch \"%s\" ...\n", JSs(update, "name"));

			if (is_cached) {
				snprintf(tmpbuf, sizeof(tmpbuf), "%s/cache/%s/%s.json", origin, branch_name, last_hash);
			} else {
				snprintf(tmpbuf, sizeof(tmpbuf), "%s/uploads/%s.json", origin, last_hash);
			}

			rc = patch(tmpbuf);
			if (rc) {
				PWLOG(LOG_ERROR, "Failed to patch\n");
			}
		}

		pw_elements_save(g_elements, "config/elements.data", true);

		for (i = 0; i < PW_MAX_MAPS; i++) {
			const struct map_name *map = &g_map_names[i];
			struct pw_npc_file *npc = &g_npc_files[i];

			snprintf(tmpbuf, sizeof(tmpbuf), "config/%s/npcgen.data", map->dir_name);
			rc = pw_npcs_save(npc, tmpbuf);
			if (rc) {
				PWLOG(LOG_ERROR, "pw_npcs_save(\"%s\") failed: %d\n", map->name, rc);
				return 1;
			}
		}
	}

	PWLOG(LOG_INFO, "last_hash: %s\n", last_hash);
	version.version = JSi(ver_cjson, "version");
	snprintf(version.branch, sizeof(version.branch), "%s", branch_name);
	if (last_hash) {
		snprintf(version.cur_hash, sizeof(version.cur_hash), "%s", last_hash);
	}
	pw_version_save(&version);

	cjson_free(ver_cjson);
	free(buf);

	return rc;
}
