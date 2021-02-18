/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020 Darek Stojaczyk for pwmirage.com
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>

#include "idmap.h"
#include "common.h"

struct pw_id_el {
	long long lid;
	long id;
	short type;
	uint8_t is_lid_mapping : 1;
	uint8_t is_async_fn : 1;
	uint8_t _unused1: 6;
	char _unused2;
	void *data;
	void *next;
};

#define PW_IDMAP_ARR_SIZE 3637

struct pw_idmap_file_hdr {
	uint32_t version;
	uint32_t type_count;
};

struct pw_idmap_type {
	uint32_t max_natural_id;
};

struct pw_idmap_file_entry {
	uint64_t lid;
	uint32_t id;
	uint8_t type;
};

struct pw_idmap {
	char *name;
	struct pw_idmap_type types[128];
	long registered_types_cnt;
	struct pw_id_el *lists[PW_IDMAP_ARR_SIZE];
};

struct pw_idmap_async_fn_el {
	pw_idmap_async_fn fn;
	void *ctx;
	struct pw_idmap_async_fn_el *next;
};

static struct pw_id_el *_idmap_set(struct pw_idmap *map, long long lid, long id, long type, void *data);

struct pw_idmap *
pw_idmap_init(const char *name, bool clean_load)
{
	struct pw_idmap *map;
	FILE *fp;
	char buf[256];
	int i;

	map = calloc(1, sizeof(*map));
	if (!map) {
		return NULL;
	}

	map->name = calloc(1, strlen(name) + 1);
	if (!map->name) {
		free(map);
		return NULL;
	}
	memcpy(map->name, name, strlen(name));

	if (clean_load) {
		return map;
	}

	snprintf(buf, sizeof(buf), "patcher/%s.imap", map->name);
	fp = fopen(buf, "rb");
	if (fp == NULL) {
		/* we'll create it on pw_idmap_save(), no problem */
		return map;
	}

	struct pw_idmap_file_hdr hdr;
	fread(&hdr, 1, sizeof(hdr), fp);

	for (i = 0; i < hdr.type_count; i++) {
		fread(&map->types[i], 1, sizeof(map->types[i]), fp);
	}

	size_t fpos = ftell(fp);
	fseek(fp, 0, SEEK_END);
	size_t fsize = ftell(fp);
	fseek(fp, fpos, SEEK_SET);
	int entry_cnt = (fsize - fpos) / sizeof(struct pw_idmap_file_entry);

	for (i = 0; i < entry_cnt; i++) {
		struct pw_idmap_file_entry entry;
		struct pw_id_el *id_el;

		fread(&entry, 1, sizeof(entry), fp);
		if (entry.type >= hdr.type_count) {
			PWLOG(LOG_ERROR, "found invalid saved idmap entry (type=%d > %d)\n",
					entry.type, hdr.type_count);
			continue;
		}

		PWLOG(LOG_INFO, "loaded mapping. lid=0x%llx, id=%u\n", entry.lid, entry.id);

		/* dummy to store the lid. When it's loaded from the regular data file
		 * it will be set to a proper lid automatically */
		id_el = _idmap_set(map, entry.id, entry.id, entry.type, NULL);
		id_el->is_lid_mapping = 1;
		id_el->data = (void *)(uintptr_t)entry.lid;
	}

	fclose(fp);
	return map;
}

long
pw_idmap_register_type(struct pw_idmap *map)
{
	return ++map->registered_types_cnt;
}

void *
pw_idmap_get(struct pw_idmap *map, long long lid, long type)
{
	struct pw_id_el *el;

	el = map->lists[lid % PW_IDMAP_ARR_SIZE];

	while (el) {
		if (el->lid == lid && (type == 0 || el->type == 0 || el->type == type)) {
			assert(!el->is_lid_mapping);
			if (el->is_async_fn) {
				return NULL;
			}
			return el->data;
		}

		el = el->next;
	}

	return NULL;
}

/* retrieve the item even if it's not set yet. The callback will be fired
 * once pw_idmap_set() hits */
int
pw_idmap_get_async(struct pw_idmap *map, long long lid, long type, pw_idmap_async_fn fn, void *fn_ctx)
{
	struct pw_id_el *el;
	struct pw_idmap_async_fn_el *async_el;

	el = map->lists[lid % PW_IDMAP_ARR_SIZE];

	while (el) {
		if (el->lid == lid && (type == 0 || el->type == 0 || el->type == type)) {
			break;
		}

		el = el->next;
	}

	if (el && !el->is_async_fn) {
		fn(el->data, fn_ctx);
		return 0;
	}

	PWLOG(LOG_INFO, "setting async mapping at lid=0x%llx\n", lid);

	async_el = calloc(1, sizeof(*async_el));
	if (!async_el) {
		return -1;
	}
	async_el->fn = fn;
	async_el->ctx = fn_ctx;

	if (!el) {
		el = _idmap_set(map, lid, 0, type, NULL);
		el->is_async_fn = 1;
		el->data = async_el;
	} else {
		assert(!el->is_lid_mapping);
		assert(el->is_async_fn);

		/* put it to the front */
		async_el->next = el->data;
		el->data = async_el;
	}

	return 0;
}

static void
call_async_arr(struct pw_idmap_async_fn_el *async_el, void *data)
{
	struct pw_idmap_async_fn_el *tmp;

	while (async_el) {
		async_el->fn(data, async_el->ctx);
		tmp = async_el;
		async_el = async_el->next;
		free(tmp);
	}
}

static struct pw_id_el *
_idmap_set(struct pw_idmap *map, long long lid, long id, long type, void *data)
{
	struct pw_id_el *el, *last_el = NULL;

	el = map->lists[lid % PW_IDMAP_ARR_SIZE];
	while (el) {
		if (el->lid == lid && (type == 0 || el->type == 0 || el->type == type)) {
			break;
		}

		last_el = el;
		el = el->next;
	}

	if (el) {
		if (el->is_lid_mapping) {
			struct pw_id_el *tmp_el;

			/* remove from the map */
			if (last_el) {
				last_el->next = el->next;
			} else {
				map->lists[lid % PW_IDMAP_ARR_SIZE] = NULL;
			}

			/* update lid and put it in the map again */
			el->lid = lid = (long long)(uintptr_t)el->data;
			el->is_lid_mapping = 0;
			el->data = data;
			el->next = NULL;

			PWLOG(LOG_INFO, "found temporary at lid=0x%x, reloacting to 0x%llx\n", el->id, el->lid);

			tmp_el = map->lists[lid % PW_IDMAP_ARR_SIZE];
			last_el = NULL;
			while (tmp_el) {
				if (tmp_el->lid == lid && (type == 0 || tmp_el->type == 0 || tmp_el->type == type)) {
					/* target lid already set */
					if (tmp_el->is_async_fn) {
						call_async_arr(tmp_el->data, data);
						/* replace tmp_el with el */
						last_el->next = el;
						el->next = tmp_el->next;
						free(tmp_el);
						return el;
					} else {
						assert(false);
						break;
					}
				}

				last_el = tmp_el;
				tmp_el = tmp_el->next;
			}

			if (last_el) {
				last_el->next = el;
			} else {
				map->lists[lid % PW_IDMAP_ARR_SIZE] = el;
			}
			return el;
		} else if (el->is_async_fn) {
			call_async_arr(el->data, data);
			el->is_async_fn = 0;
			el->id = id;
		}

		el->data = data;
		return el;
	}

	el = calloc(1, sizeof(struct pw_id_el));
	if (!el) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		return NULL;
	}

	el->lid = lid;
	el->id = id;
	el->type = type;
	el->data = data;

	if (last_el) {
		last_el->next = el;
	} else {
		map->lists[lid % PW_IDMAP_ARR_SIZE] = el;
	}
	return el;
}

void
pw_idmap_set(struct pw_idmap *map, long long lid, long id, long type, void *data)
{
	_idmap_set(map, lid, id, type, data);
}

void
pw_idmap_end_type_load(struct pw_idmap *map, long type_id, uint32_t max_id)
{
	struct pw_idmap_type *type;
	int max_types = sizeof(map->types) / sizeof(map->types[0]);

	if (type_id >= max_types) {
		PWLOG(LOG_ERROR, "can't find given type: %s->%d\n", map->name, type_id);
		return;
	}

	type = &map->types[type_id];
	if (type->max_natural_id == 0) {
		type->max_natural_id = max_id;
	}
}

int
pw_idmap_save(struct pw_idmap *map, const char *filename)
{
	FILE *fp;
	struct pw_id_el *el;
	struct pw_idmap_file_entry entry;
	int i;

	fp = fopen(filename, "wb");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "Cant open %s\n", filename);
		return -errno;
	}

	struct pw_idmap_file_hdr hdr;
	hdr.version = 1;
	hdr.type_count = map->registered_types_cnt;

	fwrite(&hdr, 1, sizeof(hdr), fp);
	for (i = 0; i < hdr.type_count; i++) {
		fwrite(&map->types[i], 1, sizeof(map->types[i]), fp);
	}

	for (i = 0; i < PW_IDMAP_ARR_SIZE; i++) {
		el = map->lists[i];
		while (el) {
			struct pw_idmap_type *type = &map->types[el->type];

			if (el->lid <= type->max_natural_id) {
				el = el->next;
				continue;
			}

			PWLOG(LOG_INFO, "saving mapping. lid=0x%llx, id=%u\n", el->lid, el->id);
			entry.lid = el->lid;
			entry.id = el->id;
			entry.type = el->type;
			fwrite(&entry, 1, sizeof(entry), fp);

			el = el->next;
		}
	}

	fclose(fp);
	return 0;
}
