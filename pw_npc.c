/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>

#include "cjson.h"
#include "cjson_ext.h"
#include "common.h"
#include "pw_npc.h"

extern struct pw_idmap *g_elements_map;

const struct map_name g_map_names[PW_MAX_MAPS] = {
	{ "gs01", "world" },
	{ "is01", "a01" },
	{ "is02", "a02" },
	{ "is05", "a05" },
	{ "is06", "a06" },
	{ "is07", "a07" },
	{ "is08", "a08" },
	{ "is09", "a09" },
	{ "is10", "a10" },
	{ "is11", "a11" },
	{ "is12", "a12" },
	{ "is13", "a13" },
	{ "is14", "a14" },
	{ "is15", "a15" },
	{ "is16", "a16" },
	{ "is17", "a17" },
	{ "is18", "a18" },
	{ "is19", "a19" },
	{ "is20", "a20" },
	{ "is21", "a21" },
	{ "is22", "a22" },
	{ "is23", "a23" },
	{ "is24", "a24" },
	{ "is25", "a25" },
	{ "is26", "a26" },
	{ "is27", "a27" },
	{ "is28", "a28" },
	{ "is29", "a29" },
	{ "is31", "a31" },
	{ "is32", "a32" },
	{ "is33", "a33" },
	{ "a26b", "a26b" },
};

static size_t
serialize_spawner_type_fn(FILE *fp, struct serializer *f, void *data)
{
	uint32_t is_npc = *(uint32_t *)data;

	fprintf(fp, "\"type\":\"%s\",", is_npc ? "npc" : "monster");
	return 4;
}

static size_t
deserialize_spawner_type_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	uint32_t is_npc = 0;
	const char *name = JSs(f);

	if (name && strcmp(name, "npc") == 0) {
		is_npc = 1;
	}
	*(uint32_t *)data = is_npc;
	return 4;
}

static size_t
deserialize_spawner_groups_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	struct pw_spawner_set *set = data;

	if (f->type != CJSON_TYPE_NONE && f->type != CJSON_TYPE_OBJECT) {
		PWLOG(LOG_ERROR, "found json group field that is not an object (type: %d)\n", f->type);
		return 0;
	}

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

		void *grp_el = NULL;
		struct pw_chain_el *chain = set->groups->chain;
		unsigned remaining_idx = idx;

		while (chain) {
			if (remaining_idx < chain->count) {
				/* the group exists */
				grp_el = chain->data + remaining_idx * set->groups->el_size;
				break;
			}

			if (remaining_idx < chain->capacity) {
				remaining_idx -= chain->count;
				/* the group doesn't exist, but memory for it is already allocated */
				break;
			}
			assert(remaining_idx >= chain->count); /* technically possible if chain->cap == 0 */
			remaining_idx -= chain->count;
			chain = chain->next;
		}

		if (!grp_el) {
			if (remaining_idx > 8) {
				/* sane limit - the hole is too big */
				continue;
			}

			while (true) {
				grp_el = pw_chain_table_new_el(set->groups);
				if (!grp_el) {
					PWLOG(LOG_ERROR, "pw_chain_table_new_el() failed\n");
					return 0;
				}

				if (remaining_idx == 0) {
					break;
				}
				remaining_idx--;
			}
		}

		deserialize(json_el, set->groups->serializer, grp_el);
		json_el = json_el->next;
	}
	return 0;
}

static size_t
deserialize_id_removed_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	uint32_t is_removed = !!(*(uint32_t *)(data) & (1 << 31));

	deserialize_log(f, &is_removed);

	if (JSi(f)) {
		*(uint32_t *)(data) |= (1 << 31);
	} else {
		*(uint32_t *)(data) &= ~(1 << 31);
	}

	return 0;
}

static void
deserialize_elements_id_field_async_fn(void *data, void *target_data)
{
	PWLOG(LOG_INFO, "patching (prev:%u, new: %u)\n", *(uint32_t *)target_data, *(uint32_t *)data);
	*(uint32_t *)target_data = *(uint32_t *)data;
}

static size_t
deserialize_elements_id_field_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	int64_t val = JSi(f);

	if (val >= 0x80000000) {
		int rc = pw_idmap_get_async(g_elements_map, val, 0, deserialize_elements_id_field_async_fn, data);

		if (rc) {
			assert(false);
		}
	} else {
		deserialize_log(f, data);
		*(uint32_t *)(data) = (uint32_t)val;
	}

	return 4;
}

static size_t
serialize_elements_id_field_fn(FILE *fp, struct serializer *f, void *data)
{
	/* TODO */
	return 4;
}

static struct serializer spawner_serializer[] = {
	{ "groups", _CUSTOM, NULL /* FIXME */, deserialize_spawner_groups_fn },
	{ "fixed_y", _INT32 },
	{ "groups_count", _INT32 }, 
	{ "pos", _ARRAY_START(3) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "dir", _ARRAY_START(3) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "spread", _ARRAY_START(3) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "type", _CUSTOM, serialize_spawner_type_fn, deserialize_spawner_type_fn }, /* 4 bytes */
	{ "mob_type", _INT32 },
	{ "auto_spawn", _INT8 },
	{ "auto_respawn", _INT8 },
	{ "_unused1", _INT8 },
	{ "_removed", _CUSTOM, NULL /* FIXME */, deserialize_id_removed_fn },
	{ "id", _INT32 },
	{ "trigger", _INT32 /* TODO parse high IDs */ },
	{ "lifetime", _FLOAT },
	{ "max_num", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer spawner_group_serializer[] = {
	{ "type", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "count", _INT32 },
	{ "_unused1", _INT32 },
	{ "_unused2", _INT32 },
	{ "aggro", _INT32 },
	{ "_unused3", _FLOAT },
	{ "_unused4", _FLOAT },
	{ "group_id", _INT32 },
	{ "accept_help_group_id", _INT32 },
	{ "help_group_id", _INT32 },
	{ "need_help", _INT8 },
	{ "default_group", _INT8 },
	{ "default_need", _INT8 },
	{ "default_help", _INT8 },
	{ "path_id", _INT32, /* TODO parse high IDs */ },
	{ "path_type", _INT32 },
	{ "path_speed", _INT32 },
	{ "disappear_time", _INT32 },
	{ "", _TYPE_END },
};

/* TODO add CONST_STR type field == "resource"? */
static struct serializer resource_serializer[] = {
	{ "pos", _ARRAY_START(3) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "spread", _ARRAY_START(2) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "groups_count", _INT32 },
	{ "auto_spawn", _INT8 },
	{ "auto_respawn", _INT8 },
	{ "_unused1", _INT8 },
	{ "_removed", _CUSTOM, NULL /* FIXME */, deserialize_id_removed_fn },
	{ "id", _INT32 },
	{ "dir", _ARRAY_START(2) },
		{ "", _INT8 },
	{ "", _ARRAY_END },
	{ "rad", _INT8 },
	{ "trigger", _INT32 }, /* TODO */
	{ "max_num", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer resource_group_serializer[] = {
	{ "_unused_type", _INT32 },
	{ "type", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "respawn_time", _INT32 },
	{ "count", _INT32 },
	{ "height_offset", _FLOAT },
	{ "", _TYPE_END },
};

static struct serializer dynamic_serializer[] = {
	{ "_removed", _CUSTOM, NULL /* FIXME */, deserialize_id_removed_fn },
	{ "id", _INT32 },
	{ "pos", _ARRAY_START(3) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "dir", _ARRAY_START(2) },
		{ "", _INT8 },
	{ "", _ARRAY_END },
	{ "rad", _INT8 },
	{ "trigger", _INT32 },
	{ "scale", _INT8 },
	{ "", _TYPE_END },
};

static struct serializer trigger_serializer[] = {
	{ "_removed", _CUSTOM, NULL /* FIXME */, deserialize_id_removed_fn },
	{ "id", _INT32 },
	{ "console_id", _INT32 },
	{ "name", _WSTRING(64) },
	{ "auto_start", _INT8 },
	{ "start_delay", _INT32 },
	{ "stop_delay", _INT32 },
	{ "has_start_time", _INT8 },
	{ "has_stop_time", _INT8 },
	{ "start_time", _OBJECT_START },
		{ "year", _INT32 },
		{ "month", _INT32 },
		{ "week_day", _INT32 },
		{ "day", _INT32 },
		{ "hour", _INT32 },
		{ "minute", _INT32 },
	{ "", _OBJECT_END },
	{ "end_time", _OBJECT_START },
		{ "year", _INT32 },
		{ "month", _INT32 },
		{ "week_day", _INT32 },
		{ "day", _INT32 },
		{ "hour", _INT32 },
		{ "minute", _INT32 },
	{ "", _OBJECT_END },
	{ "interval", _INT32 },
	{ "", _TYPE_END },
};

int
pw_npcs_load(struct pw_npc_file *npc, const char *name, const char *file_path, bool clean_load)
{
	FILE *fp;
	int rc;
	char buf[128];
	uint32_t max_id;

	memset(npc, 0, sizeof(*npc));
	snprintf(buf, sizeof(buf), "npcgen_%s", name);

	npc->name = name;
	npc->idmap = pw_idmap_init(buf, clean_load);
	if (!npc->idmap) {
		PWLOG(LOG_ERROR, "pw_idmap_init() failed\n");
		return 1;
	}

	fp = fopen(file_path, "rb");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "Cant open %s\n", file_path);
		return -errno;
	}

	fread(&npc->hdr.version, 1, sizeof(npc->hdr.version), fp);
	if (npc->hdr.version != 0xA && npc->hdr.version != 0x5) {
		PWLOG(LOG_ERROR, "Invalid version %u (first four bytes)!\n", npc->hdr.version);
		goto err;
	}

	fread(&npc->hdr.creature_sets_count, 1, sizeof(npc->hdr.creature_sets_count), fp);
	fread(&npc->hdr.resource_sets_count, 1, sizeof(npc->hdr.resource_sets_count), fp);
	if (npc->hdr.version >= 10) {
		fread(&npc->hdr.dynamics_count, 1, sizeof(npc->hdr.dynamics_count), fp);
		fread(&npc->hdr.triggers_count, 1, sizeof(npc->hdr.triggers_count), fp);
	}

	PWLOG(LOG_DEBUG_5, "spawners_count: %u\n", npc->hdr.creature_sets_count);
	PWLOG(LOG_DEBUG_5, "resource_count: %u\n", npc->hdr.resource_sets_count);
	PWLOG(LOG_DEBUG_5, "dynamics_count: %u\n", npc->hdr.dynamics_count);
	PWLOG(LOG_DEBUG_5, "triggers_count: %u\n", npc->hdr.triggers_count);

	rc = pw_chain_table_init(&npc->spawners, "spawners", spawner_serializer, sizeof(struct pw_spawner_set), npc->hdr.creature_sets_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->spawners, count: %u\n", npc->hdr.creature_sets_count);
		goto err;
	}

	npc->spawners.idmap_type = pw_idmap_register_type(npc->idmap);
	npc->spawners.chain->count = npc->hdr.creature_sets_count;
	max_id = 0;
	for (int i = 0;	i < npc->hdr.creature_sets_count; i++) {
		struct pw_spawner_set *el = (void *)(npc->spawners.chain->data + i * npc->spawners.el_size);
		uint32_t id;
		size_t off = ftell(fp);

		fread(el->data, 1, 59, fp);
		if (npc->hdr.version >= 7) {
		    fread(el->data + 59, 1, 12, fp); /* trigger, lifetime, max_num */
		}

		uint32_t groups_count = *(uint32_t *)(el->data + 4);

		el->groups = pw_chain_table_alloc("spawner_group", spawner_group_serializer, 60, groups_count);
		if (!el->groups) {
			PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed for spawner->groups\n");
			goto err;
		}
		el->groups->chain->count = groups_count;

		for (int j = 0; j < groups_count; j++) {
			void *grp = el->groups->chain->data + j * el->groups->el_size;

			fread(grp, 1, el->groups->el_size, fp);
		}

		if (clean_load) {
			/* set it on the initial npcgen load */
			SPAWNER_ID(el->data) = i;
		}
		id = SPAWNER_ID(el->data) & ~(1UL << 31);
		if (id > max_id) {
			max_id = id;
		}
		PWLOG(LOG_DEBUG_5, "spawner parsed, off=%u, id=%u, groups=%u\n", off, id, groups_count);
		pw_idmap_set(npc->idmap, id, id, npc->spawners.idmap_type, el);
	}
	pw_idmap_end_type_load(npc->idmap, npc->spawners.idmap_type, max_id);

	rc = pw_chain_table_init(&npc->resources, "resources", resource_serializer, sizeof(struct pw_resource_set), npc->hdr.resource_sets_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->recources, count: %u\n", npc->hdr.creature_sets_count);
		goto err;
	}

	npc->resources.idmap_type = pw_idmap_register_type(npc->idmap);
	npc->resources.chain->count = npc->hdr.resource_sets_count;
	max_id = 0;
	for (int i = 0;	i < npc->hdr.resource_sets_count; i++) {
		struct pw_resource_set *el = (void *)(npc->resources.chain->data + i * npc->resources.el_size);
		uint32_t id;
		size_t off = ftell(fp);

		fread(el->data, 1, 31, fp);
		if (npc->hdr.version >= 6) {
			fread(el->data + 31, 1, 3, fp);
		}
		if (npc->hdr.version >= 7) {
			fread(el->data + 34, 1, 8, fp);
		}

		uint32_t groups_count = *(uint32_t *)(el->data + 20);

		el->groups = pw_chain_table_alloc("resource_group", resource_group_serializer, 20, groups_count);
		if (!el->groups) {
			PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed for resource->groups\n");
			goto err;
		}
		el->groups->chain->count = groups_count;

		for (int j = 0; j < groups_count; j++) {
			void *grp = el->groups->chain->data + j * el->groups->el_size;

			fread(grp, 1, el->groups->el_size, fp);
		}

		if (clean_load) {
			/* set it on the initial npcgen load */
			RESOURCE_ID(el->data) = 100000 + i;
		}
		id = RESOURCE_ID(el->data) & ~(1UL << 31);
		if (id > max_id) {
			max_id = id;
		}
		PWLOG(LOG_DEBUG_5, "resource parsed, off=%u, id=%u, groups=%u\n", off, id, groups_count);
		pw_idmap_set(npc->idmap, id, id, npc->resources.idmap_type, el);
	}
	pw_idmap_end_type_load(npc->idmap, npc->resources.idmap_type, max_id);

	rc = pw_chain_table_init(&npc->dynamics, "dynamics", dynamic_serializer, 24, npc->hdr.dynamics_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->dynamics, count: %u\n", npc->hdr.dynamics_count);
		goto err;
	}

	npc->dynamics.idmap_type = pw_idmap_register_type(npc->idmap);
	npc->dynamics.chain->count = npc->hdr.dynamics_count;
	max_id = 0;
	for (int i = 0;	i < npc->hdr.dynamics_count; i++) {
		void *el = (void *)(npc->dynamics.chain->data + i * npc->dynamics.el_size);
		uint32_t id;

		fread(el, 1, 19, fp);

		if (npc->hdr.version >= 9) {
		    fread(el + 19, 1, 4, fp);
		}

		if (npc->hdr.version >= 10) {
		    fread(el + 23, 1, 1, fp);
		} else {
		    *(uint8_t *)(el + 23) = 16;
		}

		id = DYNAMIC_ID(el) & ~(1UL << 31);
		PWLOG(LOG_DEBUG_5, "dynamic parsed, off=%u, id=%u\n", ftell(fp), id);
		if (id > max_id) {
			max_id = id;
		}
		pw_idmap_set(npc->idmap, id, id, npc->dynamics.idmap_type, el);
	}
	pw_idmap_end_type_load(npc->idmap, npc->dynamics.idmap_type, max_id);

	rc = pw_chain_table_init(&npc->triggers, "triggers", trigger_serializer, 199, npc->hdr.triggers_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->triggers, count: %u\n", npc->hdr.triggers_count);
		goto err;
	}

	npc->triggers.idmap_type = pw_idmap_register_type(npc->idmap);
	npc->triggers.chain->count = npc->hdr.triggers_count;
	max_id = 0;
	for (int i = 0;	i < npc->hdr.triggers_count; i++) {
		void *el = (void *)(npc->triggers.chain->data + i * npc->triggers.el_size);
		uint32_t id;

		fread(el, 1, 195, fp);
		if (npc->hdr.version >= 8) {
			fread(el + 195, 1, 4, fp);
		}

		id = TRIGGER_ID(el) & ~(1UL << 31);
		PWLOG(LOG_DEBUG_5, "trigger parsed, off=%u, id=%u\n", ftell(fp), id);
		if (id > max_id) {
			max_id = id;
		}
		pw_idmap_set(npc->idmap, id, id, npc->triggers.idmap_type, el);
	}
	pw_idmap_end_type_load(npc->idmap, npc->triggers.idmap_type, max_id);

	fclose(fp);
	return 0;

err:
	fclose(fp);
	return -errno;
}

int
pw_npcs_patch_obj(struct pw_npc_file *npc, struct cjson *obj)
{
	struct pw_chain_table *table;
	void *table_el;
	const char *obj_type;
	int64_t id;

	id = JSi(obj, "id");
	if (!id) {
		PWLOG(LOG_ERROR, "missing obj.id\n");
		return -1;
	}

	/* todo verify obj._db.type == spawners_* */
	obj_type = JSs(obj, "type");
	if (obj_type) {
		if (strcmp(obj_type, "npc") == 0) {
			table = &npc->spawners;
		} else if (strcmp(obj_type, "monster") == 0) {
			table = &npc->spawners;
		} else if (strcmp(obj_type, "resource") == 0) {
			table = &npc->resources;
		} else {
			PWLOG(LOG_ERROR, "unknown obj.type: \"%s\"\n", obj_type);
			return -1;
		}

		table_el = pw_idmap_get(npc->idmap, id, table->idmap_type);
	} else {
		table = &npc->spawners;
		table_el = pw_idmap_get(npc->idmap, id, table->idmap_type);
		if (!table_el) {
			table = &npc->resources;
			table_el = pw_idmap_get(npc->idmap, id, table->idmap_type);
			if (!table_el) {
				PWLOG(LOG_ERROR, "new spawner without obj.type set: %"PRIu64"\n", id);
				return -1;
			}
		}
	}

	if (!table_el) {
		uint32_t el_id = 0;
		struct pw_chain_el *chain = table->chain;

		while (chain) {
			el_id += chain->count;
			chain = chain->next;
		}

		PWLOG(LOG_DEBUG_5, "0x%llx not found, creating with id=%u\n", id, el_id);

		table_el = pw_chain_table_new_el(table);
		if (table == &npc->spawners) {
			struct pw_spawner_set *set = table_el;

			SPAWNER_ID(table_el) = el_id;
			*(uint32_t *)(table_el + 44) = 1; /* NPC */
			*(uint32_t *)(table_el + 48) = 3214; /* NPC */
			*(uint8_t *)(table_el + 52) = 1; /* auto spawn */
			set->groups = pw_chain_table_alloc("spawner_group", spawner_group_serializer, 60, 16);
		} else if (table == &npc->resources) {
			struct pw_resource_set *set = table_el;

			RESOURCE_ID(table_el) = el_id;

			set->groups = pw_chain_table_alloc("resource_group", resource_group_serializer, 20, 16);
			if (!set->groups) {
				PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed for resource->groups\n");
				return -1;
			}
		}

		pw_idmap_set(npc->idmap, id, el_id, table->idmap_type, table_el);
	}

	unsigned real_id = 0;
	if (table == &npc->spawners) {
		real_id = SPAWNER_ID(table_el);
	} else {
		real_id = RESOURCE_ID(table_el);
	}
	PWLOG(LOG_DEBUG_5, "0x%llx found with real id=%u\n", id, real_id);
	deserialize(obj, table->serializer, table_el);
	return 0;
}

int
pw_npcs_save(struct pw_npc_file *npc, const char *file_path)
{
	FILE *fp;
	struct pw_chain_el *chain;
	void *el;

	fp = fopen(file_path, "wb");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "Cant open %s\n", file_path);
		return -errno;
	}

	npc->hdr.version = 10;
	fwrite(&npc->hdr, 1, sizeof(npc->hdr), fp);
	
	uint32_t spawners_count = 0;
	PW_CHAIN_TABLE_FOREACH(el, chain, &npc->spawners) {
		struct pw_spawner_set *set = el;
		size_t off_begin;
		uint32_t groups_count = 0;

		if (SPAWNER_ID(set->data) & (1 << 31)) {
			PWLOG(LOG_DEBUG_5, "spawner ignored, off=%u, id=%u\n", ftell(fp), SPAWNER_ID(el));
			continue;
		}

		off_begin = ftell(fp);
		fwrite(&set->data, 1, sizeof(set->data), fp);

		void *grp_el;
		struct pw_chain_table *grp_table = set->groups;
		struct pw_chain_el *grp_chain;
		PW_CHAIN_TABLE_FOREACH(grp_el, grp_chain, grp_table) {
			if (*(uint32_t *)grp_el & ~(1UL << 31)) {
				fwrite(grp_el, 1, grp_table->el_size, fp);
				groups_count++;
			}
		}

		if (groups_count == 0) {
			/* skip */
			PWLOG(LOG_DEBUG_5, "spawner skipped, off=%u, id=%u\n", off_begin, SPAWNER_ID(el));
			fseek(fp, off_begin, SEEK_SET);
			continue;
		}

		PWLOG(LOG_DEBUG_5, "spawner saved, off=%u, id=%u\n", off_begin, SPAWNER_ID(el));
		spawners_count++;
		size_t end_pos = ftell(fp);
		fseek(fp, off_begin + 4, SEEK_SET);
		fwrite(&groups_count, 1, sizeof(groups_count), fp);
		fseek(fp, end_pos, SEEK_SET);
	}

	uint32_t resources_count = 0;
	PW_CHAIN_TABLE_FOREACH(el, chain, &npc->resources) {
		struct pw_resource_set *set = el;
		size_t off_begin;
		uint32_t groups_count = 0;

		if (RESOURCE_ID(set->data) & (1 << 31)) {
			PWLOG(LOG_DEBUG_5, "resource ignored, off=%u, id=%u\n", ftell(fp), RESOURCE_ID(el));
			continue;
		}

		off_begin = ftell(fp);
		fwrite(&set->data, 1, sizeof(set->data), fp);

		void *grp_el;
		struct pw_chain_table *grp_table = set->groups;
		struct pw_chain_el *grp_chain;
		PW_CHAIN_TABLE_FOREACH(grp_el, grp_chain, grp_table) {
			if (*(uint32_t *)(grp_el + 4) & ~(1UL << 31)) {
				fwrite(grp_el, 1, grp_table->el_size, fp);
				groups_count++;
			}
		}

		if (groups_count == 0) {
			/* skip */
			PWLOG(LOG_DEBUG_5, "resource skipped, off=%u, id=%u\n", off_begin, RESOURCE_ID(el));
			fseek(fp, off_begin, SEEK_SET);
			continue;
		}

		PWLOG(LOG_DEBUG_5, "resource saved, off=%u, id=%u\n", off_begin, RESOURCE_ID(el));
		resources_count++;
		size_t end_pos = ftell(fp);
		fseek(fp, off_begin + 20, SEEK_SET);
		fwrite(&groups_count, 1, sizeof(groups_count), fp);
		fseek(fp, end_pos, SEEK_SET);
	}

	uint32_t dynamics_count = 0;
	PW_CHAIN_TABLE_FOREACH(el, chain, &npc->dynamics) {
		if (DYNAMIC_ID(el) & (1 << 31)) {
			PWLOG(LOG_DEBUG_5, "dynamic ignored, off=%u, id=%u\n", ftell(fp), DYNAMIC_ID(el));
			continue;
		}
		PWLOG(LOG_DEBUG_5, "dynamic saved, off=%u, id=%u\n", ftell(fp), DYNAMIC_ID(el));
		fwrite(el, 1, npc->dynamics.el_size, fp);
		dynamics_count++;
	}

	uint32_t triggers_count = 0;
	PW_CHAIN_TABLE_FOREACH(el, chain, &npc->triggers) {
		if (TRIGGER_ID(el) & (1 << 31)) {
			PWLOG(LOG_DEBUG_5, "trigger ignored, off=%u, id=%u\n", ftell(fp), TRIGGER_ID(el));
			continue;
		}

		PWLOG(LOG_DEBUG_5, "trigger saved, off=%u, id=%u\n", ftell(fp), TRIGGER_ID(el));
		fwrite(el, 1, npc->triggers.el_size, fp);
		triggers_count++;
	}

	/* get back to header and write the real creature count */
	fseek(fp, 4, SEEK_SET);
	fwrite(&spawners_count, 1, sizeof(spawners_count), fp);
	fwrite(&resources_count, 1, sizeof(resources_count), fp);
	fwrite(&dynamics_count, 1, sizeof(dynamics_count), fp);
	fwrite(&triggers_count, 1, sizeof(triggers_count), fp);

	PWLOG(LOG_DEBUG_5, "spawners_count: %u\n", spawners_count);
	PWLOG(LOG_DEBUG_5, "resource_count: %u\n", resources_count);
	PWLOG(LOG_DEBUG_5, "dynamics_count: %u\n", dynamics_count);
	PWLOG(LOG_DEBUG_5, "triggers_count: %u\n", triggers_count);

	fclose(fp);

	return pw_idmap_save(npc->idmap);
}

