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
	size_t num_bytes = 1;
	int rc;

	rc = download_mem(url, &buf, &num_bytes);
	if (rc) {
		PWLOG(LOG_ERROR, "download_mem(%s) failed: %d\n", url, rc);
		return 1;
	}

	rc = cjson_parse_arr_stream(buf, import_stream_cb, NULL);
	free(buf);
	if (rc) {
		PWLOG(LOG_ERROR, "cjson_parse_arr_stream() failed: %d\n", url, rc);
		return 1;
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	char tmpbuf[1024];
	char *buf;
	size_t num_bytes = 0;
	int i, rc;

	FILE *fp;
	setlocale(LC_ALL, "en_US.UTF-8");

	if (argc < 2) {
		printf("./%s branch_name\n", argv[0]);
		return 0;
	}

	unsigned version = 0;
	char cur_hash[64] = {0};
	cur_hash[0] = '0';

	fp = fopen("patcher/version", "w+b");
	if (fp) {
		fread(&version, 1, sizeof(version), fp);
		fread(cur_hash, 1, sizeof(cur_hash), fp);
		cur_hash[sizeof(cur_hash) - 1] = 0;
		fclose(fp);
	}

	g_elements = calloc(1, sizeof(*g_elements));
	if (!g_elements) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		return 1;
	}

	rc = pw_elements_load(g_elements, "patcher/elements.src");
	if (rc != 0) {
		PWLOG(LOG_ERROR, "pw_elements_load(\"patcher/elements.src\") failed: %d\n", rc);
		return 1;
	}

	for (i = 0; i < PW_MAX_MAPS; i++) {
		const struct map_name *map = &g_map_names[i];
		struct pw_npc_file *npc = &g_npc_files[i];

		snprintf(tmpbuf, sizeof(tmpbuf), "config/%s/npcgen.data", map->dir_name);
		rc = pw_npcs_load(npc, map->name, tmpbuf, false);
		if (rc) {
			PWLOG(LOG_ERROR, "pw_npcs_load(\"%s\") failed: %d\n", map->name, rc);
			return 1;
		}
		
	}

	const char *branch_name = argv[1];
	
	snprintf(tmpbuf, sizeof(tmpbuf), "http://miragetest.ddns.net/editor/project/fetch/%s/since/%s",
			branch_name, cur_hash);

	rc = download_mem(tmpbuf, &buf, &num_bytes);
	if (rc) {
		PWLOG(LOG_ERROR, "download_mem failed for %s\n", tmpbuf);
		return 1;
	}

	struct cjson *ver_cjson = cjson_parse(buf);

	const char *origin = JSs(ver_cjson, "origin");
	struct cjson *updates = JS(ver_cjson, "updates");
	for (i = 0; i < updates->count; i++) {
		struct cjson *update = JS(updates, i);
		bool is_cached = JSi(update, "cached");
		const char *hash_type = is_cached ? "cache" : "uploads";

		PWLOG(LOG_INFO, "Fetching patch \"%s\" ...\n", JSs(update, "topic"));

		snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s/%s/%s.json", origin, hash_type, branch_name, JSs(update, "hash"));
		rc = patch(tmpbuf);
		if (rc) {
			PWLOG(LOG_ERROR, "Failed to patch\n");
		}
	}

	cjson_free(ver_cjson);
	free(buf);

	pw_elements_save(g_elements, "elements_exp.data", true);

	for (i = 0; i < PW_MAX_MAPS; i++) {
		const struct map_name *map = &g_map_names[i];
		struct pw_npc_file *npc = &g_npc_files[i];

		snprintf(tmpbuf, sizeof(tmpbuf), "config/%s/npcgen.data2", map->dir_name);
		rc = pw_npcs_save(npc, tmpbuf);
		if (rc) {
			PWLOG(LOG_ERROR, "pw_npcs_save(\"%s\") failed: %d\n", map->name, rc);
			return 1;
		}
	}

	return rc;
}
