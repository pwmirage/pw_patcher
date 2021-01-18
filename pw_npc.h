/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2021 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_NPC_H
#define PW_NPC_H

#include <inttypes.h>
#include <stdint.h>

#include "common.h"

/* we use one of the unused fields as id */
#define SPAWNER_ID(s) (*(uint32_t *)((s) + 57))
#define RESOURCE_ID(r) (*(uint32_t *)((r) + 27))
#define DYNAMIC_ID(r) (*(uint32_t *)((r) + 0))
#define TRIGGER_ID(r) (*(uint32_t *)((r) + 0))

struct map_name {
	const char *name;
	const char *dir_name;
};

struct pw_spawner_set {
	char data[71];
	struct pw_chain_table *groups;
};

struct pw_resource_set {
	char data[42];
	struct pw_chain_table *groups;
};

#define PW_MAX_MAPS 32
extern const struct map_name g_map_names[];

struct pw_npc_file {
	struct pw_npc_header {
		uint32_t version;
		uint32_t creature_sets_count;
		uint32_t resource_sets_count;
		uint32_t dynamics_count;
		uint32_t triggers_count;
	} hdr;

	const char *name;
	struct pw_chain_table spawners;
	struct pw_chain_table resources;
	struct pw_chain_table dynamics;
	struct pw_chain_table triggers;
	struct pw_idmap *idmap;
};

struct cjson;
int pw_npcs_load(struct pw_npc_file *npc, const char *name, const char *file_path, bool clean_load);
int pw_npcs_patch_obj(struct pw_npc_file *npc, struct cjson *obj);
int pw_npcs_save(struct pw_npc_file *npc, const char *file_path);

#endif /* PW_NPC_H */
