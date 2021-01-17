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
serialize_spawner_type_fn(FILE *fp, void *data)
{
	uint32_t is_npc = *(uint32_t *)data;

	fprintf(fp, "\"type\":\"%s\",", is_npc ? "npc" : "monster");
	return 4;
}

static size_t
deserialize_spawner_type_fn(struct cjson *f, void *data)
{
	uint32_t is_npc = 0;

	if (strcmp(JSs(f, "type"), "npc") == 0) {
		is_npc = 1;
	}
	*(uint32_t *)data = is_npc;
	return 4;
}

static size_t
deserialize_spawner_groups_fn(struct cjson *f, void *data)
{
	struct pw_spawner_set *set = data;

	if (f->type != CJSON_TYPE_ARRAY) {
		PWLOG(LOG_ERROR, "found json group field that is not an array (type: %d)\n", f->type);
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
			if (remaining_idx >= chain->count) {
				remaining_idx -= chain->count;
				chain = chain->next;
				continue;
			}

			grp_el = chain->data + remaining_idx * set->groups->el_size;
			break;
		}

		if (!grp_el) {
			if (remaining_idx > 8) {
				/* sane limit - the hole is too big */
				break;
			}

			while (remaining_idx > 0) {
				grp_el = pw_chain_table_new_el(set->groups);
				if (!grp_el) {
					PWLOG(LOG_ERROR, "pw_chain_table_new_el() failed\n");
					return 0;
				}
			}
		}

		deserialize(json_el, set->groups->serializer, grp_el);
	}
	return 0;
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
	{ "type", _CUSTOM, serialize_spawner_type_fn, deserialize_spawner_type_fn },
	{ "mob_type", _INT32 },
	{ "auto_spawn", _INT8 },
	{ "auto_respawn", _INT8 },
	{ "_unused1", _INT8 },
	{ "id", _INT32 },
	{ "trigger", _FLOAT },
	{ "lifetime", _FLOAT },
	{ "max_num", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer spawner_group_serializer[] = {
	{ "type", _INT32 },
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
	{ "path_id", _INT32 },
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
	{ "id", _INT32 },
	{ "dir", _ARRAY_START(2) },
		{ "", _INT8 },
	{ "", _ARRAY_END },
	{ "rad", _INT8 },
	{ "trigger", _INT32 },
	{ "max_num", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer resource_group_serializer[] = {
	{ "_unused_type", _INT32 },
	{ "type", _INT32 },
	{ "respawn_time", _INT32 },
	{ "count", _INT32 },
	{ "height_offset", _FLOAT },
	{ "", _TYPE_END },
};

static struct serializer dynamic_serializer[] = {
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
pw_npcs_load(struct pw_npc_file *npc, const char *name, const char *file_path)
{
	FILE *fp;
	int rc;

	memset(npc, 0, sizeof(*npc));

	npc->name = name;
	npc->idmap = pw_idmap_init();
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

	rc = pw_chain_table_init(&npc->spawners, "spawners", spawner_serializer, sizeof(struct pw_spawner_set), npc->hdr.creature_sets_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->spawners, count: %u\n", npc->hdr.creature_sets_count);
		goto err;
	}

	npc->spawners.chain->count = npc->hdr.creature_sets_count;
	for (int i = 0;	i < npc->hdr.creature_sets_count; i++) {
		struct pw_spawner_set *el = (void *)(npc->spawners.chain->data + i * npc->spawners.el_size);

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

		for (int j = 0; j < groups_count; j++) {
			void *grp = el->groups->chain->data + j * el->groups->el_size;

			fread(grp, 1, el->groups->el_size, fp);
		}

		if (!SPAWNER_ID(el->data)) {
			/* set it on the initial npcgen load */
			SPAWNER_ID(el->data) = i;
		}
		pw_idmap_set(npc->idmap, SPAWNER_ID(el->data) & ~(1UL << 31), &npc->spawners, el);
	}

	rc = pw_chain_table_init(&npc->resources, "resources", resource_serializer, sizeof(struct pw_resource_set), npc->hdr.resource_sets_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->recources, count: %u\n", npc->hdr.creature_sets_count);
		goto err;
	}

	npc->resources.chain->count = npc->hdr.resource_sets_count;
	for (int i = 0;	i < npc->hdr.resource_sets_count; i++) {
		struct pw_resource_set *el = (void *)(npc->resources.chain->data + i * npc->resources.el_size);

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

		for (int j = 0; j < groups_count; j++) {
			void *grp = el->groups->chain->data + j * el->groups->el_size;

			fread(grp, 1, el->groups->el_size, fp);
		}

		if (!RESOURCE_ID(el->data)) {
			/* set it on the initial npcgen load */
			RESOURCE_ID(el->data) = 100000 + i;
		}
		pw_idmap_set(npc->idmap, RESOURCE_ID(el->data) & ~(1UL << 31), &npc->resources, el);
	}

	rc = pw_chain_table_init(&npc->dynamics, "dynamics", dynamic_serializer, 24, npc->hdr.dynamics_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->dynamics, count: %u\n", npc->hdr.dynamics_count);
		goto err;
	}

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

		pw_idmap_set(npc->idmap, DYNAMIC_ID(el) & ~(1UL << 31), &npc->dynamics, el);
	}

	rc = pw_chain_table_init(&npc->triggers, "triggers", trigger_serializer, 199, npc->hdr.triggers_count);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_chain_table_init() failed for npc->triggers, count: %u\n", npc->hdr.triggers_count);
		goto err;
	}

	npc->triggers.chain->count = npc->hdr.triggers_count;
	for (int i = 0;	i < npc->hdr.triggers_count; i++) {
		void *el = (void *)(npc->triggers.chain->data + i * npc->triggers.el_size);

		fread(el, 1, 195, fp);
		if (npc->hdr.version >= 8) {
			fread(el + 195, 1, 4, fp);
		}

		pw_idmap_set(npc->idmap, TRIGGER_ID(el) & ~(1UL << 31), &npc->triggers, el);
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
	void **table_el;
	const char *obj_type;
	int64_t id;

	/* FIXME type is missing in objs */
	obj_type = JSs(obj, "type");
	if (!obj_type) {
		PWLOG(LOG_ERROR, "missing obj.type\n");
		return -1;
	}

	id = JSi(obj, "id");
	if (!id) {
		PWLOG(LOG_ERROR, "missing obj.id\n");
		return -1;
	}

	if (id >= 100000) {
		table = &npc->resources;
	} else {
		table = &npc->spawners;
	}

	table_el = pw_idmap_get(npc->idmap, id, table);
	if (!table_el) {
		uint32_t el_id = 0;
		struct pw_chain_el *chain = table->chain;

		while (chain) {
			el_id += chain->count;
			chain = chain->next;
		}

		table_el = pw_chain_table_new_el(table);
		if (table == &npc->spawners) {
			SPAWNER_ID(table_el) = el_id;
		} else if (strcmp(obj_type, "resource")) {
			RESOURCE_ID(table_el) = el_id;
		}

		pw_idmap_set(npc->idmap, id, table, table_el);
		return -1;
	}

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

	fwrite(&npc->hdr, 1, sizeof(npc->hdr), fp);
	
	uint32_t spawners_count = 0;
	PW_CHAIN_TABLE_FOREACH(el, chain, &npc->spawners) {
		struct pw_spawner_set *set = el;
		size_t off_begin;
		uint32_t groups_count = 0;

		if (SPAWNER_ID(set->data) & (1 << 31)) continue;

		off_begin = ftell(fp);
		fwrite(&set->data, 1, npc->spawners.el_size, fp);

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
			fseek(fp, off_begin, SEEK_SET);
			continue;
		}

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

		if (RESOURCE_ID(set->data) & (1 << 31)) continue;

		off_begin = ftell(fp);
		fwrite(&set->data, 1, npc->resources.el_size, fp);

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
			fseek(fp, off_begin, SEEK_SET);
			continue;
		}

		resources_count++;
		size_t end_pos = ftell(fp);
		fseek(fp, off_begin + 20, SEEK_SET);
		fwrite(&groups_count, 1, sizeof(groups_count), fp);
		fseek(fp, end_pos, SEEK_SET);
	}

	uint32_t dynamics_count = 0;
	PW_CHAIN_TABLE_FOREACH(el, chain, &npc->dynamics) {
		if (DYNAMIC_ID(el) & (1 << 31)) continue;
		fwrite(&el, 1, npc->dynamics.el_size, fp);
		dynamics_count++;
	}

	uint32_t triggers_count = 0;
	PW_CHAIN_TABLE_FOREACH(el, chain, &npc->triggers) {
		if (DYNAMIC_ID(el) & (1 << 31)) continue;
		fwrite(&el, 1, npc->dynamics.el_size, fp);
		triggers_count++;
	}

	/* get back to header and write the real creature count */
	fseek(fp, 4, SEEK_SET);
	fwrite(&spawners_count, sizeof(spawners_count), 1, fp);
	fwrite(&resources_count, sizeof(spawners_count), 1, fp);
	fwrite(&dynamics_count, sizeof(dynamics_count), 1, fp);
	fwrite(&triggers_count, sizeof(triggers_count), 1, fp);

	fclose(fp);
	return 0;
}

