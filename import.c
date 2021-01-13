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
        pwlog(LOG_ERROR, "found non-object in the patch file (type=%d)\n", obj->type);
        assert(false);
        return;
    }

    fprintf(stderr, "type: %s\n", JSs(obj, "_db", "type"));

    print_obj(obj->a, 1);
    pw_elements_patch_obj(g_elements, obj);
}

static int
patch(const char *elements_path, const char *url)
{
    char *buf;
    size_t num_bytes = 1;
	int rc;

	rc = pw_elements_load(g_elements, elements_path);
	if (rc != 0) {
        fprintf(stderr, "pw_elements_load(%s) failed: %d\n", elements_path, rc);
		return 1;
	}

    rc = download_mem(url, &buf, &num_bytes);
    if (rc) {
        pwlog(LOG_ERROR, "download_mem(%s) failed: %d\n", url, rc);
        return 1;
    }

    rc = cjson_parse_arr_stream(buf, import_stream_cb, NULL);
    if (rc) {
        pwlog(LOG_ERROR, "cjson_parse_arr_stream() failed: %d\n", url, rc);
        return 1;
    }

    pw_elements_save(g_elements, "elements_exp.data", false);
	return 0;
}

int
main(int argc, char *argv[])
{
	int rc;
	setlocale(LC_ALL, "en_US.UTF-8");

    if (argc < 3) {
        printf("./%s path_to_npcgen.data world_name\n", argv[0]);
        printf("./%s path_to_elements.data\n", argv[0]);
        return 0;
    }

    g_elements = calloc(1, sizeof(*g_elements));
    if (!g_elements) {
	    pwlog(LOG_ERROR, "calloc() failed\n");
	    return -ENOMEM;
    }

    rc = patch(argv[1], argv[2]);

	return rc;
}
