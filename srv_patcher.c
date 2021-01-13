/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020 Darek Stojaczyk for pwmirage.com
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

	PWLOG(LOG_INFO, "type: %s\n", JSs(obj, "_db", "type"));

	print_obj(obj->a, 1);
	pw_elements_patch_obj(g_elements, obj);
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

	fp = fopen("patcher/version", "w+");
	if (!fp) {
		PWLOG(LOG_ERROR, "can't open patcher/version\n");
		return 1;
	}

	unsigned version = 0;
	char cur_hash[64] = {0};
	cur_hash[0] = '0';
	fread(&version, 1, sizeof(version), fp);
	fread(cur_hash, 1, sizeof(cur_hash), fp);
	cur_hash[sizeof(cur_hash) - 1] = 0;
	fclose(fp);

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
	return rc;
}
