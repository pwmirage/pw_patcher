/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
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
#include <time.h>

#include "cjson.h"
#include "cjson_ext.h"
#include "common.h"
#include "idmap.h"
#include "chain_arr.h"
#include "pw_tasks_npc.h"

extern struct pw_idmap *g_elements_map;
extern int g_elements_npc_idmap_id;

#define PW_TASKS_NPC_VERSION 1

/* file header */
struct tasks_npc_header {
	uint32_t pack_size;
	uint32_t time_mark;
	uint16_t version;
	uint16_t npc_count;
};

/* entries as they appear in the file */
struct tasks_npc_entry {
	uint32_t npc_id;
	uint16_t pos[3];
};

/* runtime entries, not saved to file */
struct tasks_npc_entry_tmp {
	uint64_t npc_lid;
	float pos[3];
	bool _removed;
};

struct pw_tasks_npc {
	struct tasks_npc_header hdr;
	struct pw_idmap *idmap;
	struct pw_chain_table *table;
};

struct pw_tasks_npc *
pw_tasks_npc_load(const char *filepath)
{
	struct pw_tasks_npc *tasks;
	FILE *fp;
	uint16_t i;

	fp = fopen(filepath, "rb");
	if (!fp) {
		PWLOG(LOG_ERROR, "Can't open %s\n", filepath);
		return NULL;
	}

	tasks = calloc(1, sizeof(*tasks));
	if (!tasks) {
		fclose(fp);
		return NULL;
	}

	fread(&tasks->hdr, sizeof(tasks->hdr), 1, fp);
	if (tasks->hdr.version != PW_TASKS_NPC_VERSION) {
		PWLOG(LOG_ERROR, "Invalid version: %u\n", tasks->hdr.version);
		fclose(fp);
		return NULL;
	}

	tasks->idmap = pw_idmap_init("spawners_tmp", NULL, true);
	if (!tasks->idmap) {
		goto err;
	}

	tasks->table = pw_chain_table_alloc("", NULL, sizeof(struct tasks_npc_entry_tmp), tasks->hdr.npc_count);
	if (!tasks->table) {
		goto err;
	}

	struct tasks_npc_entry_tmp *entries = (void *)tasks->table->chain->data;

	for (i = 0; i < tasks->hdr.npc_count; i++) {
		struct tasks_npc_entry fentry;
		struct tasks_npc_entry_tmp *entry;

		fread(&fentry, sizeof(fentry), 1, fp);

		entry = &entries[i];
		entry->npc_lid = fentry.npc_id;
		entry->pos[0] = fentry.pos[0];
		entry->pos[1] = fentry.pos[2];
		entry->pos[2] = fentry.pos[1];

		pw_idmap_set(tasks->idmap, entry->npc_lid, 0, entry);
	}

	tasks->table->chain->count = tasks->hdr.npc_count;
	return tasks;

err:
	free(tasks);
	fclose(fp);
	return NULL;
}

int
pw_tasks_npc_patch_obj(struct pw_tasks_npc *tasks, struct cjson *obj)
{
	struct pw_idmap_el *node;
	struct tasks_npc_entry_tmp *entry;
	struct cjson *pos, *_removed;
	float pos_xyz[3];
	int64_t ltype;
	const char *spawner_type;

	ltype = JSi(obj, "groups", "0", "type");
	spawner_type = JSs(obj, "type");
	if (!ltype || !spawner_type || strcmp(spawner_type, "npc") != 0) {
		/* old, incomplete diff or not an npc, ignore it */
		return -1;
	}

	pos = JS(obj, "pos");
	_removed = JS(obj, "_removed");
	if (pos->type != CJSON_TYPE_OBJECT && _removed->type != CJSON_TYPE_OBJECT) {
		return -1;
	}

	node = pw_idmap_get(tasks->idmap, ltype, 0);
	if (node) {
		entry = node->data;
	} else {
		entry = pw_chain_table_new_el(tasks->table);
		if (!entry) {
			PWLOG(LOG_ERROR, "pw_chain_table_new_el() failed\n");
			return -1;
		}

		entry->npc_lid = ltype;
		node = pw_idmap_set(tasks->idmap, entry->npc_lid, 0, entry);
	}

	pos_xyz[0] = JSf(pos, "0");
	pos_xyz[1] = JSf(pos, "1");
	pos_xyz[2] = JSf(pos, "2");

	if (pos_xyz[0]) {
		entry->pos[0] = pos_xyz[0];
	}
	if (pos_xyz[1]) {
		entry->pos[1] = pos_xyz[1];
	}
	if (pos_xyz[2]) {
		entry->pos[2] = pos_xyz[2];
	}

	if (_removed->type != CJSON_TYPE_NONE) {
		entry->_removed = !!JSi(_removed);
	}

	return 0;
}

int
pw_tasks_npc_save(struct pw_tasks_npc *tasks, const char *filename)
{
	FILE *fp;
	void *_el;
	size_t count;

	fp = fopen(filename, "wb");
	if (!fp) {
		return -errno;
	}

	fseek(fp, sizeof(struct tasks_npc_header), SEEK_SET);

	count = 0;
	PW_CHAIN_TABLE_FOREACH(_el, tasks->table) {
		struct tasks_npc_entry_tmp *entry = _el;

		if (entry->_removed) {
			continue;
		}

		if (entry->npc_lid >= 0x80000000) {
			/* at the time of saving all IDs should be already mapped, so go sync */
			struct pw_idmap_el *node = pw_idmap_get(g_elements_map, entry->npc_lid,
					g_elements_npc_idmap_id);
			struct pw_idmap_el *lownode;
			struct tasks_npc_entry_tmp *lowentry;

			if (!node) {
				PWLOG(LOG_ERROR, "found npc lid with no mapping: %llu\n", entry->npc_lid);
				/* no mapping? */
				continue;
			}

			lownode = pw_idmap_get(tasks->idmap, node->id, 0);
			if (lownode) {
				lowentry = (void *)lownode->data;
				/* override the lowentry */
				memcpy(lowentry, entry, sizeof(*entry));
				lowentry->npc_lid = node->id;
			} else {
				lowentry = pw_chain_table_new_el(tasks->table);
				if (!lowentry) {
					PWLOG(LOG_ERROR, "pw_chain_table_new_el() failed\n");
					return -1;
				}

				memcpy(lowentry, entry, sizeof(*entry));
				lowentry->npc_lid = node->id;
			}

			entry = lowentry;
		}
	}

	PW_CHAIN_TABLE_FOREACH(_el, tasks->table) {
		struct tasks_npc_entry_tmp *entry = _el;
		struct tasks_npc_entry fentry;

		if (entry->npc_lid >= 0x80000000) {
			/* we only process the low ids */
			continue;
		}

		fentry.npc_id = entry->npc_lid;
		fentry.pos[0] = entry->pos[0];
		fentry.pos[1] = entry->pos[2];
		fentry.pos[2] = entry->pos[1];

		fwrite(&fentry, sizeof(fentry), 1, fp);
		count++;
	}

	tasks->hdr.npc_count = count;
	long int _time;
	time(&_time);
	tasks->hdr.time_mark = _time;
	tasks->hdr.npc_count = count;
	tasks->hdr.pack_size = sizeof(tasks->hdr) + count * sizeof(struct tasks_npc_entry);

	fseek(fp, 0, SEEK_SET);
	fwrite(&tasks->hdr, sizeof(tasks->hdr), 1, fp);

	fclose(fp);
	return 0;
}

