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
#include <unistd.h>
#include <sys/types.h>

#include "cjson.h"
#include "cjson_ext.h"
#include "common.h"
#include "serializer.h"
#include "idmap.h"
#include "chain_arr.h"
#include "pw_npc.h"

extern struct pw_idmap *g_elements_map;
struct pw_idmap *g_triggers_map;

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

	if (f->type == CJSON_TYPE_NONE) {
		return 4;
	}

	if (name && strcmp(name, "npc") == 0) {
		is_npc = 1;
	}
	*(uint32_t *)data = is_npc;
	return 4;
}

static size_t
serialize_id_removed_fn(FILE *fp, struct serializer *f, void *data)
{
	/* TODO? */
	return 0;
}

static size_t
deserialize_id_removed_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	uint32_t is_removed = !!(*(uint32_t *)(data) & (1 << 31));

	if (f->type == CJSON_TYPE_NONE) {
		return 0;
	}

	deserialize_log(f, &is_removed);

	if (JSi(f)) {
		*(uint32_t *)(data) |= (1 << 31);
	} else {
		*(uint32_t *)(data) &= ~(1 << 31);
	}

	return 0;
}

static void
deserialize_elements_id_field_async_fn(struct pw_idmap_el *node, void *target_data)
{
	PWLOG(LOG_INFO, "patching (prev:%u, new: %u)\n", *(uint32_t *)target_data, node->id);
	*(uint32_t *)target_data = node->id;
}

static size_t
deserialize_elements_id_field_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	int64_t val = JSi(f);

	if (f->type == CJSON_TYPE_NONE) {
		return 4;
	}

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

static size_t
serialize_trigger_id_fn(FILE *fp, struct serializer *f, void *data)
{
	uint32_t trigger = *(uint32_t *)data;

	fprintf(fp, "\"trigger\":\"%u\",", trigger);
	return 4;
}

static void
deserialize_trigger_id_async_fn(struct pw_idmap_el *node, void *target_data)
{
	PWLOG(LOG_INFO, "patching (prev:%u, new: %u)\n", *(uint32_t *)target_data, node->id);
	*(uint32_t *)target_data = node->id;
}

static size_t
deserialize_trigger_id_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	int64_t val = JSi(f);

	if (f->type == CJSON_TYPE_NONE) {
		return 4;
	}

	if (val >= 0x80000000) {
		int rc = pw_idmap_get_async(g_triggers_map, val, 0, deserialize_trigger_id_async_fn, data);

		if (rc) {
			assert(false);
		}
	} else {
		deserialize_log(f, data);
		*(uint32_t *)(data) = (uint32_t)val;
	}

	return 4;
}

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

static struct serializer spawner_serializer[] = {
	{ "_fixed_y", _INT32 },
	{ "_groups_cnt", _INT32 },
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
	{ "_removed", _CUSTOM, serialize_id_removed_fn, deserialize_id_removed_fn },
	{ "id", _INT32 },
	{ "trigger", _CUSTOM, serialize_trigger_id_fn, deserialize_trigger_id_fn },
	{ "lifetime", _FLOAT },
	{ "max_num", _INT32 },
	{ "groups", _CHAIN_TABLE, spawner_group_serializer },
	{ "", _TYPE_END },
};

static void
init_spawner_group_fn(void *grp, void *ctx)
{
	*(uint8_t *)serializer_get_field(spawner_group_serializer, "default_group", grp) = 1;
	*(uint8_t *)serializer_get_field(spawner_group_serializer, "default_need", grp) = 1;
	*(uint8_t *)serializer_get_field(spawner_group_serializer, "default_help", grp) = 1;
}

static struct serializer resource_group_serializer[] = {
	{ "_unused_type", _INT32 },
	{ "type", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "respawn_time", _INT32 },
	{ "count", _INT32 },
	{ "height_offset", _FLOAT },
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
	{ "_groups_cnt", _INT32 },
	{ "auto_spawn", _INT8 },
	{ "auto_respawn", _INT8 },
	{ "_unused1", _INT8 },
	{ "_removed", _CUSTOM, serialize_id_removed_fn, deserialize_id_removed_fn },
	{ "id", _INT32 },
	{ "dir", _ARRAY_START(2) },
		{ "", _INT8 },
	{ "", _ARRAY_END },
	{ "rad", _INT8 },
	{ "trigger", _CUSTOM, serialize_trigger_id_fn, deserialize_trigger_id_fn },
	{ "max_num", _INT32 },
	{ "groups", _CHAIN_TABLE, resource_group_serializer },
	{ "", _TYPE_END },
};
static struct serializer dynamic_serializer[] = {
	{ "_removed", _CUSTOM, serialize_id_removed_fn, deserialize_id_removed_fn },
	{ "id", _INT32 },
	{ "pos", _ARRAY_START(3) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "dir", _ARRAY_START(2) },
		{ "", _INT8 },
	{ "", _ARRAY_END },
	{ "rad", _INT8 },
	{ "trigger", _CUSTOM, serialize_trigger_id_fn, deserialize_trigger_id_fn },
	{ "scale", _INT8 },
	{ "", _TYPE_END },
};

static struct serializer trigger_serializer[] = {
	{ "_removed", _CUSTOM, serialize_id_removed_fn, deserialize_id_removed_fn },
	{ "id", _INT32 },
	{ "_ai_id", _INT32 }, /* ID to be referenced in aipolicy and API */
	{ "name", _STRING(128) },
	{ "auto_start", _INT8 },
	{ "start_delay", _INT32 },
	{ "stop_delay", _INT32 },
	{ "_no_start_time", _INT8 }, /* all hidden -> we'll control it with scripts */
	{ "_no_stop_time", _INT8 },
	{ "_start_time", _OBJECT_START },
		{ "year", _INT32 },
		{ "month", _INT32 },
		{ "week_day", _INT32 },
		{ "day", _INT32 },
		{ "hour", _INT32 },
		{ "minute", _INT32 },
	{ "", _OBJECT_END },
	{ "_end_time", _OBJECT_START },
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
	char buf2[256];

	if (!g_triggers_map) {
		g_triggers_map = pw_idmap_init("npcgen_triggers",
				clean_load ? NULL : "patcher/npcgen_triggers.imap",
				true);
		if (!g_triggers_map) {
			PWLOG(LOG_ERROR, "pw_idmap_init() failed\n");
			return 1;
		}
		pw_idmap_ignore_dups(g_triggers_map);
	}

	memset(npc, 0, sizeof(*npc));
	snprintf(buf, sizeof(buf), "npcgen_%s", name);

	npc->name = name;
	snprintf(buf2, sizeof(buf2), "patcher/%s.imap", buf);
	npc->idmap = pw_idmap_init(buf, clean_load ? NULL : buf2, true);
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

	rc = pw_chain_table_init(&npc->spawners, "spawners", spawner_serializer, serializer_get_size(spawner_serializer), npc->hdr.creature_sets_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->spawners, count: %u\n", npc->hdr.creature_sets_count);
		goto err;
	}

	npc->spawners.idmap_type = pw_idmap_register_type(npc->idmap);
	npc->spawners.chain->count = npc->hdr.creature_sets_count;
	for (int i = 0;	i < npc->hdr.creature_sets_count; i++) {
		void *el = npc->spawners.chain->data + i * npc->spawners.el_size;
		uint32_t id;
		size_t off = ftell(fp);
		struct pw_chain_table *tbl;

		fread(el, 1, 59, fp);
		if (npc->hdr.version >= 7) {
		    fread(el + 59, 1, 12, fp); /* trigger, lifetime, max_num */
		}

		uint32_t groups_count = *(uint32_t *)serializer_get_field(spawner_serializer, "_groups_cnt", el);
		tbl = pw_chain_table_fread(fp, "spawner_groups", groups_count, spawner_group_serializer);
		if (!tbl) {
			PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed for spawner->groups\n");
			goto err;
		}
		tbl->new_el_fn = init_spawner_group_fn;
		*(void **)serializer_get_field(spawner_serializer, "groups", el) = tbl;

		if (clean_load) {
			/* set it on the initial npcgen load */
			SPAWNER_ID(el) = i;
		}
		id = SPAWNER_ID(el) & ~(1UL << 31);
		PWLOG(LOG_DEBUG_5, "spawner parsed, off=%u, id=%u, groups=%u\n", off, id, groups_count);
		pw_idmap_set(npc->idmap, id, npc->spawners.idmap_type, el);
	}

	rc = pw_chain_table_init(&npc->resources, "resources", resource_serializer, serializer_get_size(resource_serializer), npc->hdr.resource_sets_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->recources, count: %u\n", npc->hdr.creature_sets_count);
		goto err;
	}

	npc->resources.idmap_type = pw_idmap_register_type(npc->idmap);
	npc->resources.chain->count = npc->hdr.resource_sets_count;
	for (int i = 0;	i < npc->hdr.resource_sets_count; i++) {
		void *el = npc->resources.chain->data + i * npc->resources.el_size;
		uint32_t id;
		size_t off = ftell(fp);
		struct pw_chain_table *tbl;

		fread(el, 1, 31, fp);
		if (npc->hdr.version >= 6) {
			fread(el + 31, 1, 3, fp);
		}
		if (npc->hdr.version >= 7) {
			fread(el + 34, 1, 8, fp);
		}

		uint32_t groups_count = *(uint32_t *)serializer_get_field(resource_serializer, "_groups_cnt", el);
		tbl = pw_chain_table_fread(fp, "resource_groups", groups_count, resource_group_serializer);
		if (!tbl) {
			PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed for resource->groups\n");
			goto err;
		}
		*(void **)serializer_get_field(resource_serializer, "groups", el) = tbl;

		if (clean_load) {
			/* set it on the initial npcgen load */
			RESOURCE_ID(el) = 100000 + i;
		}
		id = RESOURCE_ID(el) & ~(1UL << 31);
		PWLOG(LOG_DEBUG_5, "resource parsed, off=%u, id=%u, groups=%u\n", off, id, groups_count);
		pw_idmap_set(npc->idmap, id, npc->resources.idmap_type, el);
	}

	rc = pw_chain_table_init(&npc->dynamics, "dynamics", dynamic_serializer, 24, npc->hdr.dynamics_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->dynamics, count: %u\n", npc->hdr.dynamics_count);
		goto err;
	}

	npc->dynamics.idmap_type = pw_idmap_register_type(npc->idmap);
	npc->dynamics.chain->count = npc->hdr.dynamics_count;
	for (int i = 0;	i < npc->hdr.dynamics_count; i++) {
		void *el = (void *)(npc->dynamics.chain->data + i * npc->dynamics.el_size);

		fread(el, 1, 19, fp);

		if (npc->hdr.version >= 9) {
		    fread(el + 19, 1, 4, fp);
		}

		if (npc->hdr.version >= 10) {
		    fread(el + 23, 1, 1, fp);
		} else {
		    *(uint8_t *)(el + 23) = 16;
		}

		PWLOG(LOG_DEBUG_5, "dynamic parsed, off=%u, id=%u\n", ftell(fp), i + 1);
		pw_idmap_set(npc->idmap, i + 1, npc->dynamics.idmap_type, el);
	}

	rc = pw_chain_table_init(&npc->triggers, "triggers", trigger_serializer, 199, npc->hdr.triggers_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->triggers, count: %u\n", npc->hdr.triggers_count);
		goto err;
	}

	npc->triggers.idmap_type = pw_idmap_register_type(npc->idmap);
	npc->triggers.chain->count = npc->hdr.triggers_count;
	for (int i = 0;	i < npc->hdr.triggers_count; i++) {
		void *el = (void *)(npc->triggers.chain->data + i * npc->triggers.el_size);
		uint32_t id;

		fread(el, 1, 195, fp);
		if (npc->hdr.version >= 8) {
			fread(el + 195, 1, 4, fp);
		}

		id = TRIGGER_ID(el) & ~(1UL << 31);
		PWLOG(LOG_DEBUG_5, "trigger parsed, off=%u, id=%u\n", ftell(fp), id);

		pw_idmap_set(npc->idmap, id, npc->triggers.idmap_type, el);
		pw_idmap_set(g_triggers_map, id, 0, el);
	}

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
	struct pw_idmap_el *node;
	void *table_el;
	const char *obj_type;
	int64_t id;

	id = JSi(obj, "id");
	if (!id) {
		PWLOG(LOG_ERROR, "missing obj.id\n");
		return -1;
	}

	obj_type = JSs(obj, "type");
	if (strncmp(JSs(obj, "_db", "type"), "triggers_", strlen("triggers_")) == 0) {
		table = &npc->triggers;
		node = pw_idmap_get(npc->idmap, id, table->idmap_type);
	} else if (obj_type) {
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

		node = pw_idmap_get(npc->idmap, id, table->idmap_type);
	} else {
		table = &npc->spawners;
		node = pw_idmap_get(npc->idmap, id, table->idmap_type);
		if (!node) {
			table = &npc->resources;
			node = pw_idmap_get(npc->idmap, id, table->idmap_type);
			if (!node) {
				PWLOG(LOG_ERROR, "new spawner without obj.type set: %"PRIu64"\n", id);
				return -1;
			}
		}
	}

	if (node) {
		table_el = node->data;
	} else {
		table_el = pw_chain_table_new_el(table);
		node = pw_idmap_set(npc->idmap, id, table->idmap_type, table_el);

		if (table == &npc->spawners) {
			struct pw_chain_table *grp_tbl;

			SPAWNER_ID(table_el) = node->id;
			*(uint32_t *)serializer_get_field(table->serializer, "type", table_el) = 1;
			*(uint8_t *)serializer_get_field(table->serializer, "auto_spawn", table_el) = 1;
			*(uint8_t *)serializer_get_field(table->serializer, "auto_respawn", table_el) = 1;
			grp_tbl = pw_chain_table_alloc("spawner_group", spawner_group_serializer, serializer_get_size(spawner_group_serializer), 8);
			if (!grp_tbl) {
				PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed for spawner->groups\n");
				return -1;
			}
			grp_tbl->new_el_fn = init_spawner_group_fn;
			*(void **)serializer_get_field(table->serializer, "groups", table_el) = grp_tbl;
		} else if (table == &npc->resources) {
			struct pw_chain_table *grp_tbl;

			RESOURCE_ID(table_el) = node->id;

			grp_tbl = pw_chain_table_alloc("spawner_group", resource_group_serializer, serializer_get_size(resource_group_serializer), 8);
			if (!grp_tbl) {
				PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed for resource->groups\n");
				return -1;
			}
			*(void **)serializer_get_field(table->serializer, "groups", table_el) = grp_tbl;
		} else if (table == &npc->triggers) {
			TRIGGER_ID(table_el) = node->id;

			*(uint32_t *)serializer_get_field(trigger_serializer, "_ai_id", table_el) = node->id;

			pw_idmap_set(g_triggers_map, id, table->idmap_type, table_el);
		}
	}

	PWLOG(LOG_DEBUG_5, "0x%llx found with id=%u\n", id, node->id);
	deserialize(obj, table->serializer, table_el);

	if (table == &npc->spawners) {
		float *pos = (float *)serializer_get_field(spawner_serializer, "pos", table_el);
		*(uint32_t *)serializer_get_field(spawner_serializer, "_fixed_y", table_el) = pos[1] != 0;
	}

	return 0;
}

#define SAVE_TBL_CNT(name, slzr, data) \
	({ \
		struct pw_chain_table *tbl = *(void **)serializer_get_field(slzr, name, data); \
		struct pw_chain_el *chain = tbl->chain; \
		uint32_t cnt = 0; \
		while (chain) { \
			cnt += chain->count; \
			chain = chain->next; \
		} \
		*(uint32_t *)serializer_get_field(slzr, "_" name "_cnt", data) = cnt; \
	})

int
pw_npcs_save(struct pw_npc_file *npc, const char *file_path)
{
	FILE *fp;
	void *el;

	fp = fopen(file_path, "wb");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "Cant open %s\n", file_path);
		return -errno;
	}

	npc->hdr.version = 10;
	fwrite(&npc->hdr, 1, sizeof(npc->hdr), fp);
	
	uint32_t spawners_count = 0;
	PW_CHAIN_TABLE_FOREACH(el, &npc->spawners) {
		size_t off_begin;
		uint32_t groups_count = 0;

		if (SPAWNER_ID(el) & (1 << 31)) {
			PWLOG(LOG_DEBUG_5, "spawner ignored, off=%u, id=%u\n", ftell(fp), SPAWNER_ID(el));
			continue;
		}

		SAVE_TBL_CNT("groups", spawner_serializer, el);

		off_begin = ftell(fp);
		fwrite(el, 1, 71, fp);

		void *grp_el;
		struct pw_chain_table *grp_table = *(void **)serializer_get_field(spawner_serializer, "groups", el);
		PW_CHAIN_TABLE_FOREACH(grp_el, grp_table) {
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
	PW_CHAIN_TABLE_FOREACH(el, &npc->resources) {
		size_t off_begin;
		uint32_t groups_count = 0;

		if (RESOURCE_ID(el) & (1 << 31)) {
			PWLOG(LOG_DEBUG_5, "resource ignored, off=%u, id=%u\n", ftell(fp), RESOURCE_ID(el));
			continue;
		}

		SAVE_TBL_CNT("groups", resource_serializer, el);

		off_begin = ftell(fp);
		fwrite(el, 1, 42, fp);

		void *grp_el;
		struct pw_chain_table *grp_table = *(void **)serializer_get_field(resource_serializer, "groups", el);
		PW_CHAIN_TABLE_FOREACH(grp_el, grp_table) {
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
	PW_CHAIN_TABLE_FOREACH(el, &npc->dynamics) {
		PWLOG(LOG_DEBUG_5, "dynamic saved, off=%u, id=%u\n", ftell(fp), dynamics_count + 1);
		fwrite(el, 1, npc->dynamics.el_size, fp);
		dynamics_count++;
	}

	uint32_t no_start_time_off = serializer_get_offset(npc->triggers.serializer, "_no_start_time");
	uint32_t no_stop_time_off = serializer_get_offset(npc->triggers.serializer, "_no_stop_time");
	uint32_t triggers_count = 0;
	PW_CHAIN_TABLE_FOREACH(el, &npc->triggers) {
		if (TRIGGER_ID(el) & (1 << 31)) {
			PWLOG(LOG_DEBUG_5, "trigger ignored, off=%u, id=%u\n", ftell(fp), TRIGGER_ID(el));
			continue;
		}

		*(uint8_t *)(el + no_start_time_off) = 1;
		*(uint8_t *)(el + no_stop_time_off) = 1;

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

	if (g_triggers_map) {
		pw_idmap_save(g_triggers_map, "patcher/npcgen_triggers.imap");
		g_triggers_map = NULL;
	}

	/* save idmap immediately, it's only used server-side */
	char tmpbuf[1024];
	snprintf(tmpbuf, sizeof(tmpbuf), "patcher/%s.imap", npc->name);
	return pw_idmap_save(npc->idmap, tmpbuf);
}

int
pw_npcs_serialize(struct pw_npc_file *npc, const char *path)
{
	FILE *fp = fopen(path, "wb");
	void *el;
	int count = 0;

	if (fp == NULL) {
		PWLOG(LOG_ERROR, "cant open %s\n", path);
		return 1;
	}

	fprintf(fp, "[");

	PW_CHAIN_TABLE_FOREACH(el, &npc->triggers) {
		struct serializer *tmp_slzr = npc->triggers.serializer;
		size_t pos_begin = ftell(fp);
		void *tmp_el = el;

		_serialize(fp, &tmp_slzr, &tmp_el, 1, true, true, true);

		if (ftell(fp) > pos_begin + 2) {
			fprintf(fp, ",\n");
		}
		count++;
	}

	/* replace ,\n} with }] */
	fseek(fp, -3, SEEK_CUR);
	fprintf(fp, count > 0 ? "}]" : "]");

	size_t sz = ftell(fp);
	fclose(fp);
	truncate(path, sz);
	return 0;
}
