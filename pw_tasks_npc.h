/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#include <inttypes.h>
#include <stdint.h>

struct pw_tasks_npc;

struct pw_tasks_npc *pw_tasks_npc_load(const char *filepath);
int pw_tasks_npc_patch_obj(struct pw_tasks_npc *tasks, struct cjson *obj);
int pw_tasks_npc_save(struct pw_tasks_npc *tasks, const char *filename);
