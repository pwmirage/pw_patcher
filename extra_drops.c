/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <assert.h>

#include "common.h"
#include "cjson_ext.h"
#include "serializer.h"
#include "chain_arr.h"
#include "idmap.h"
#include "avl.h"

static struct serializer monster_id_serializer[] = {
	{ "", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer extra_drop_serializer[] = {
	{ "monster_ids", _CHAIN_TABLE, monster_id_serializer },
	{ "drop_items", _ARRAY_START(256) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "name", _STRING(128) },
	{ "type", _INT32 },
	{ "drop_num_prob", _ARRAY_START(8) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "", _TYPE_END },
};

#define EXTRA_DROPS_VERSION 1

void
extra_drops_load(const char *filename)
{
	FILE *fp;
	struct pw_chain_table table;
	void *el;
	int rc, i;

	fp = fopen(filename, "rb");
	if (!fp) {
		PWLOG(LOG_ERROR, "fopen(\"%s\") failed\n", filename);
		return;
	}

	uint32_t version = 0;
	fread(&version, sizeof(version), 1, fp);

	if (version != EXTRA_DROPS_VERSION) {
		PWLOG(LOG_ERROR, "version mismatch: %d\n", version);
		fclose(fp);
		return;
	}

	uint32_t count = 0;
	fread(&count, sizeof(count), 1, fp);
	fprintf(stderr, "count=%d\n", count);

	rc = pw_chain_table_init(&table, "extra_drops", extra_drop_serializer, serializer_get_size(extra_drop_serializer), count);
	assert(rc == 0);

	table.chain->count = table.chain->capacity;

	for (i = 0; i < count; i++) {
		el = table.chain->data + i * table.el_size;
		
		uint32_t num = 0;
		fread(&num, sizeof(num), 1, fp);

		struct pw_chain_table **tbl_p = el;
		*tbl_p = pw_chain_table_fread(fp, "monster_ids", num, monster_id_serializer);

		fread(el + PW_POINTER_BUF_SIZE, serializer_get_size(extra_drop_serializer) - PW_POINTER_BUF_SIZE, 1, fp);

		fprintf(stderr, "monsters=%d\n", num);
	}

	fclose(fp);
	fp = fopen("extra_drops.json", "wb");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "cant open extra_drops.json\n");
		return;
	}

	fprintf(fp, "[");
	PW_CHAIN_TABLE_FOREACH(el, &table) {
		struct serializer *tmp_slzr = extra_drop_serializer;
		size_t pos_begin = ftell(fp);
		void *tmp_el = el;

		_serialize(fp, &tmp_slzr, &tmp_el, 1, true, true, true);

		if (ftell(fp) > pos_begin + 2) {
			fprintf(fp, ",\n");
		}
	}

	/* replace ,\n} with }] */
	fseek(fp, -3, SEEK_CUR);
	fprintf(fp, "}]");

	size_t sz = ftell(fp);
	fclose(fp);
	truncate("extra_drops.json", sz);
}
