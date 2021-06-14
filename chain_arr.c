/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include "chain_arr.h"
#include "common.h"

int
pw_chain_table_init(struct pw_chain_table *table, const char *name, struct serializer *serializer, size_t el_size, size_t count)
{
	table->name = name;
	table->serializer = serializer;
	table->el_size = el_size;

	table->chain = table->chain_last = calloc(1, sizeof(*table->chain) + el_size * count);
	if (!table->chain) {
		return -ENOMEM;
	}

	table->chain->capacity = count;
	return 0;
}

struct pw_chain_table *
pw_chain_table_alloc(const char *name, struct serializer *serializer, size_t el_size, size_t count)
{
	struct pw_chain_table *table;
	int rc;

	table = calloc(1, sizeof(*table));
	if (!table) {
		return NULL;
	}

	rc = pw_chain_table_init(table, name, serializer, el_size, count);
	if (rc) {
		free(table);
		return NULL;
	}

	return table;
}

void *
pw_chain_table_new_el(struct pw_chain_table *table)
{
	struct pw_chain_el *chain = table->chain_last;
	void *el = NULL;

	if (chain->count < chain->capacity) {
		 el = &chain->data[chain->count++ * table->el_size];
	} else {
		size_t table_count = 16;
		chain->next = table->chain_last = calloc(1, sizeof(struct pw_chain_el) + table_count * table->el_size);
		if (!chain->next) {
			return NULL;
		}

		chain = table->chain_last;
		chain->capacity = table_count;
		chain->count = 1;

		el = chain->data;
	}

	if (table->new_el_fn) {
		table->new_el_fn(el, table->new_el_ctx);
	}

	return el;
}

struct pw_chain_table *
pw_chain_table_fread(FILE *fp, const char *name, size_t el_count, struct serializer *el_serializer)
{
	struct pw_chain_table *tbl;
	uint32_t el_size = serializer_get_size(el_serializer);
	size_t i;

	uint32_t el_cap = MAX(8, el_count);
	tbl = pw_chain_table_alloc(name, el_serializer, el_size, el_cap);
	if (!tbl) {
		PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed\n");
		return NULL;
	}

	for (i = 0; i < el_count; i++) {
		void *el = pw_chain_table_new_el(tbl);
		fread(el, el_size, 1, fp);
	}

	return tbl;
}

static void
free_chain(struct pw_chain_el *chain)
{
	struct pw_chain_el *tmp;

	while (chain) {
		tmp = chain->next;
		free(chain);
		chain = tmp;
	}
}

uint32_t
pw_chain_table_count(struct pw_chain_table *table)
{
	struct pw_chain_el *chain = table->chain;
	uint32_t count = 0;

	while (chain) {
		count += chain->count;
		chain = chain->next;
	}

	return count;
}

void
pw_chain_table_truncate(struct pw_chain_table *table, uint32_t size)
{
	struct pw_chain_el *chain;
	uint32_t traversed_size = 0;

	chain = table->chain;
	while (chain) {
		if (chain->count + traversed_size > size) {
			uint32_t oldcount = chain->count;

			chain->count = size - traversed_size;
			/* TODO reset that memory in a better way -> the following
			 * memset might clear nested chain tables and cause a memory
			 * leak */
			memset(chain->data + chain->count * table->el_size, 0,
					(oldcount - chain->count) * table->el_size);
			free_chain(chain->next);
			chain->next = NULL;
			return;
		}

		traversed_size += chain->count;
		chain = chain->next;
	}
}

size_t
serialize_chunked_table_fn(FILE *fp, struct serializer *f, void *data)
{
	struct pw_chain_table *table = *(void **)data;
	struct serializer *slzr;
	struct pw_chain_el *chain;

	if (!table) {
		return PW_POINTER_BUF_SIZE;
	}
	slzr = table->serializer;
	chain = table->chain;

	size_t ftell_begin = ftell(fp);
	fprintf(fp, "\"%s\":[", f->name);
	bool obj_printed = false;
	while (chain) {
		size_t i;

		for (i = 0; i < chain->count; i++) {
			void *el = chain->data + i * table->el_size;
			struct serializer *tmp_slzr = slzr;
			size_t pos_begin = ftell(fp);

			_serialize(fp, &tmp_slzr, &el, 1, true, false, true);
			if (ftell(fp) > pos_begin) {
				fprintf(fp, ",");
				obj_printed = true;
			}
		}

		chain = chain->next;
	}

	if (!obj_printed) {
		fseek(fp, ftell_begin, SEEK_SET);
	} else {
		/* override last comma */
		fseek(fp, -1, SEEK_CUR);
		fprintf(fp, "],");
	}

	return PW_POINTER_BUF_SIZE;
}

size_t
deserialize_chunked_table_fn(struct cjson *f, struct serializer *_slzr, void *data)
{
	struct serializer *slzr = _slzr->ctx;
	struct pw_chain_table *table = *(void **)data;

	if (f->type == CJSON_TYPE_NONE) {
		return PW_POINTER_BUF_SIZE;
	}

	if (f->type != CJSON_TYPE_OBJECT && f->type != CJSON_TYPE_NULL) {
		PWLOG(LOG_ERROR, "found json group field that is not an object (type: %d)\n", f->type);
		return PW_POINTER_BUF_SIZE;
	}

	if (f->type == CJSON_TYPE_NULL) {
		if (table) {
			pw_chain_table_truncate(table, 0);
		} else {
			/* nothing to do */
		}
		return PW_POINTER_BUF_SIZE;
	}

	if (!table) {
		size_t el_size = serializer_get_size(slzr);
		table = *(void **)data = pw_chain_table_alloc("", slzr, el_size, 8);
		fprintf(stderr, "allocated tbl at %p\n", table);
		/* FIXME set new_el_fn in here */
		if (!table) {
			PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed\n");
			return PW_POINTER_BUF_SIZE;
		}
	}

	int32_t null_indices_start = -1;
	struct cjson *json_el = f->a;
	while (json_el) {
		char *end;
		unsigned idx = strtod(json_el->key, &end);
		if (end == json_el->key || errno == ERANGE) {
			/* non-numeric key in array */
			json_el = json_el->next;
			assert(false);
			continue;
		}

		if (json_el->type == CJSON_TYPE_NULL) {
			if (null_indices_start == -1) {
				null_indices_start = idx;
			}
			json_el = json_el->next;
			continue;
		} else {
			/* there must not be holes */
			assert(null_indices_start == -1);
		}

		void *el = NULL;
		struct pw_chain_el *chain = table->chain;
		unsigned remaining_idx = idx;

		while (chain) {
			if (remaining_idx < chain->count) {
				/* the obj exists */
				el = chain->data + remaining_idx * table->el_size;
				break;
			}

			if (remaining_idx < chain->capacity) {
				remaining_idx -= chain->count;
				/* the obj doesn't exist, but memory for it is already allocated */
				break;
			}
			assert(remaining_idx >= chain->count); /* technically possible if chain->cap == 0 */
			remaining_idx -= chain->count;
			chain = chain->next;
		}

		if (!el) {
			if (remaining_idx > 8) {
				/* sane limit - the hole is too big */
				continue;
			}

			while (true) {
				el = pw_chain_table_new_el(table);
				if (!el) {
					PWLOG(LOG_ERROR, "pw_chain_table_new_el() failed\n");
					return PW_POINTER_BUF_SIZE;
				}

				if (remaining_idx == 0) {
					break;
				}
				remaining_idx--;
			}
		}

		deserialize(json_el, slzr, el);
		json_el = json_el->next;
	}

	if (null_indices_start != -1) {
		pw_chain_table_truncate(table, null_indices_start);
	}

	return PW_POINTER_BUF_SIZE;
}

