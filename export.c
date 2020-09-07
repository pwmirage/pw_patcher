/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
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
#include "pw_npc.h"

static int
print_elements(const char *path)
{
	struct pw_elements elements;
	int rc;
	rc = pw_elements_load(&elements, path);
	if (rc != 0) {
		return 1;
	}

	pw_elements_serialize(&elements);
	return 0;
}

static int
print_npcgen(const char *npcgen_path, const char *mapname)
{
	struct pw_npc_file npcs;
	FILE *fp;
	uint32_t set_idx;
	char buf[64];
	int rc;

	rc = pw_npcs_load(&npcs, npcgen_path);
	if (rc) {
		return rc;
	}

	snprintf(buf, sizeof(buf), "spawners_%s.js", mapname);
	fp = fopen(buf, "w");
	if (fp == NULL) {
		fprintf(stderr, "cant open %s for writing\n", buf);
		return -errno;
	}

	for (set_idx = 0; set_idx < npcs.hdr.creature_sets_count; set_idx++) {
		struct pw_npc_creature_set_data *set = &npcs.creature_sets[set_idx].data;
		struct pw_npc_creature_group *groups = npcs.creature_sets[set_idx].groups;
		uint32_t grp_idx;

		if (set->is_npc) {
			if (set->groups_count != 1) {
				fprintf(stderr, "WARNING: set %u: expected 1 group in npc struct, found %d\n", set_idx, set->groups_count);
				continue;
			}
		}

		fprintf(fp, "g_db.spawners_%s[%u]={", mapname, set_idx);
		fprintf(fp, "id:%u,", set_idx);
		fprintf(fp, "is_npc:%u,", set->is_npc);
		fprintf(fp, "mob_type:%u,", set->mob_type);
		fprintf(fp, "pos:[%.8f,%.8f,%.8f],", set->pos[0], set->fixed_y ? set->pos[1] : 0, set->pos[2]);
		fprintf(fp, "dir:[%.8f,%.8f,%.8f],", set->dir[0], set->dir[1], set->dir[2]);
		fprintf(fp, "spread:[%.8f,%.8f,%.8f],", set->spread[0], set->spread[1], set->spread[2]);
		fprintf(fp, "auto_spawn:%u,", set->auto_spawn);
		fprintf(fp, "auto_respawn:%u,", set->auto_respawn);
		fprintf(fp, "trigger:%u,", set->trigger);
		fprintf(fp, "hp:%u,", set->hp);
		fprintf(fp, "max_num:%u,", set->max_num);
		fprintf(fp, "groups:[\n");

		for (grp_idx = 0; grp_idx < set->groups_count; grp_idx++) {
			struct pw_npc_creature_group_data *grp = &groups->data;
			if (grp->type == 0) {
				fprintf(stderr, "WARNING: empty group in spawner %u\n", set_idx);
				/* invalid mob, won't spawn anyway */
				continue;
			}

			fprintf(fp, "{");
			fprintf(fp, "type:%u,", grp->type);
			fprintf(fp, "count:%u,", grp->count);
			fprintf(fp, "aggro:%u,", grp->aggro);
			fprintf(fp, "group_id:%u,", !grp->default_group ? grp->group_id : 0);
			fprintf(fp, "accept_help_group_id:%u,", grp->need_help && !grp->default_help ? grp->accept_help_group_id : 0);
			fprintf(fp, "help_group_id:%u,", !grp->default_need ? grp->help_group_id : 0);

			fprintf(fp, "path_id:%u,", grp->path_id);
			fprintf(fp, "path_type:%u,", grp->path_type);
			fprintf(fp, "path_speed:%u,", grp->path_speed);
			fprintf(fp, "disappear_time:%u", grp->disappear_time);
			fprintf(fp, "}");
			if (grp_idx != set->groups_count - 1) {
				fprintf(fp, ",\n");
			}
		}
		fprintf(fp,"]};\n");
	}
	fclose(fp);

	snprintf(buf, sizeof(buf), "resources_%s.js", mapname);
	fp = fopen(buf, "w");
	if (fp == NULL) {
		fprintf(stderr, "cant open %s for writing\n", buf);
		return -errno;
	}

	for (set_idx = 0; set_idx < npcs.hdr.resource_sets_count; set_idx++) {
		struct pw_npc_resource_set_data *set = &npcs.resource_sets[set_idx].data;
		struct pw_npc_resource_group *groups = npcs.resource_sets[set_idx].groups;
		uint32_t grp_idx;

		if (set->groups_count == 0) {
				fprintf(stderr, "WARNING: set %u: expected 1 group in resource struct, found 0\n", set_idx);
				continue;
		}

		fprintf(fp, "g_db.resources_%s[%u]={", mapname, set_idx);
		fprintf(fp, "id:%u,", set_idx);
		fprintf(fp, "pos:[%.8f,%.8f,%.8f],", set->pos[0], set->pos[1], set->pos[2]);
		fprintf(fp, "dir:[%.8f,%.8f,%.8f],", (float)set->dir[0], 0.0f, (float)set->dir[1]);
		fprintf(fp, "spread:[%.8f,0,%.8f],", set->spread[0], set->spread[1]);
		fprintf(fp, "auto_spawn:%u,", set->auto_spawn);
		fprintf(fp, "auto_respawn:%u,", set->auto_respawn);
		fprintf(fp, "max_num:%u,", set->max_num);
		fprintf(fp, "groups:[\n");

		for (grp_idx = 0; grp_idx < set->groups_count; grp_idx++) {
			struct pw_npc_resource_group_data *grp = &groups[grp_idx].data;
			if (grp->type == 0) {
				fprintf(stderr, "WARNING: empty group in spawner %u\n", set_idx);
				/* invalid resource, won't spawn anyway */
				continue;
			}

			fprintf(fp, "{");
			fprintf(fp, "type:%u,", grp->type);
			fprintf(fp, "count:%u,", grp->count);
			fprintf(fp, "respawn_time_sec:%u,", grp->respawn_time);
			fprintf(fp, "height_offset:%.8f,", grp->height_offset);
			fprintf(fp, "}");
			if (grp_idx != set->groups_count - 1) {
				fprintf(fp, ",\n");
			}
		}
		fprintf(fp,"]};\n");
	}
	fclose(fp);

	return 0;
}

int
main(void)
{
	int rc;
	setlocale(LC_ALL, "en_US.UTF-8");

	rc = print_npcgen("world/npcgen.data", "world");
	rc = rc || print_elements("elements.data");

	return rc;
}
