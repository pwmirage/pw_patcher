/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2021 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_NPC_H
#define PW_NPC_H

#include <inttypes.h>
#include <stdint.h>

#include "chain_arr.h"
#include "common.h"

/* we use one of the unused fields as id */
#define SPAWNER_ID(s) (*(uint32_t *)((s) + 55))
#define RESOURCE_ID(r) (*(uint32_t *)((r) + 27))
#define TRIGGER_ID(r) (*(uint32_t *)((r) + 0))

struct map_name {
	int id;
	const char *name;
	const char *dir_name;
};

struct pw_chain_table;

#define PW_MAX_MAPS 33
extern const struct map_name g_map_names[];

struct pw_npc_file {
	struct pw_npc_header {
		uint32_t version;
		uint32_t creature_sets_count;
		uint32_t resource_sets_count;
		uint32_t dynamics_count;
		uint32_t triggers_count;
	} hdr;

	int map_id;
	const char *name;
	struct pw_chain_table spawners;
	struct pw_chain_table resources;
	struct pw_chain_table dynamics;
	struct pw_chain_table triggers;
};

struct cjson;

int pw_npcs_load_static(const char *triggers_idmap_path, const char *spawners_idmap_path);
void pw_npcs_save_static(const char *triggers_idmap_path, const char *spawners_idmap_path);
size_t pw_npcs_serialize_trigger_id(FILE *fp, struct serializer *f, void *data);
size_t pw_npcs_deserialize_trigger_id(struct cjson *f, struct serializer *slzr, void *data);
size_t pw_npc_serialize_trigger_ai_id(FILE *fp, struct serializer *f, void *data);
size_t pw_npc_deserialize_trigger_ai_id(struct cjson *f, struct serializer *slzr, void *data);

int pw_npcs_load(struct pw_npc_file *npc, int map_id, const char *name, const char *file_path, bool clean_load);
int pw_npcs_serialize(struct pw_npc_file *npc, const char *type, const char *path);
int pw_npcs_patch_obj(struct pw_npc_file *npc, struct cjson *obj);
int pw_npcs_save(struct pw_npc_file *npc, const char *file_path);

#endif /* PW_NPC_H */
