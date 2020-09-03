#include <inttypes.h>
#include <stdint.h>

struct pw_npc_creature_group {
	struct __attribute__((packed)) pw_npc_creature_group_data {
		uint32_t type;
		uint32_t count;
		uint32_t _unused1;
		uint32_t _unused2;
		uint32_t aggro;
		float _unused3;
		float _unused4;
		uint32_t group_id;
		uint32_t accept_help_group_id;
		uint32_t help_group_id;
		bool need_help;
		bool default_group;
		bool default_need;
		bool default_help;
		uint32_t path_id;
		uint32_t path_type;
		uint32_t path_speed;
		uint32_t disappear_time;
	} data;
};

struct pw_npc_creature_set {
	struct __attribute__((packed)) pw_npc_creature_set_data {
		uint32_t fixed_y;
		uint32_t groups_count;
		float pos[3];
		float dir[3];
		float spread[3];
		uint32_t is_npc;
		uint32_t mob_type; /* 0-mob, 1-group, 2-boss */
		bool auto_spawn;
		bool auto_respawn;
		bool _unused1;
		uint32_t gen_id;
		uint32_t trigger;
		uint32_t hp;
		uint32_t max_num;
	} data;
	struct pw_npc_creature_group *groups;
};

struct pw_npc_resource_group {
	struct __attribute__((packed)) pw_npc_resource_group_data {
		int _unused_type;
		int type;
		int respawn;
		int amount;
		int height_offset;
	} data;
};

struct pw_npc_resource_set {
	struct __attribute__((packed)) pw_npc_resource_set_data {
		float spawn_x;
		float spawn_alt;
		float spawn_z;
		float spread_x;
		float spread_z;
		int groups_count;
		bool spawn_initially;
		bool auto_respawn;
		bool _unused1;
		int _unused2;
		unsigned char dir[2];
		unsigned char rad;
		int gen_id;
		int max_num;
	} data;
	struct pw_npc_resource_group *groups;
};

struct pw_npc_dynamic {
	struct __attribute__((packed)) pw_npc_dynamic_data {
		int id;
		float spawn_x;
		float spawn_alt;
		float spawn_z;
		unsigned char unknown_5;
		unsigned char unknown_6;
		unsigned char unknown_7;
		int trigger;
		unsigned char unknown_8;
	} data;
};

struct pw_npc_trigger {
	struct __attribute__((packed)) pw_npc_trigger_data {
		int unknown_1;
		int unknown_2;
		char name[128];
		bool unknown_3;
		int unknown_4;
		int unknown_5;
		bool unknown_6;
		bool unknown_7;
		int year_1;
		int month_1;
		int week_day_1;
		int day_1;
		int hour_1;
		int minute_1;
		int year_2;
		int month_2;
		int week_day_2;
		int day_2;
		int hour_2;
		int minute_2;
		int duration;
	} data;
};

struct pw_npc_file {
	struct pw_npc_header {
		uint32_t version;
		uint32_t creature_sets_count;
		uint32_t resource_sets_count;
		uint32_t dynamics_count;
		uint32_t triggers_count;
	} hdr;

	struct pw_npc_creature_set *creature_sets;
	struct pw_npc_resource_set *resource_sets;
	struct pw_npc_dynamic *dynamics;
	struct pw_npc_trigger *triggers;
};

int
pw_npcs_load(struct pw_npc_file *npc, const char *file_path)
{
	FILE *fp;

	fp = fopen(file_path, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Cant open %s\n", file_path);
		return -errno;
	}

	fread(&npc->hdr, 1, sizeof(npc->hdr), fp);
	if (npc->hdr.version != 0xA) {
		fprintf(stderr, "Invalid version %u (first four bytes)!\n", npc->hdr.version);
		goto err;
	}

	npc->creature_sets = calloc(npc->hdr.creature_sets_count, sizeof(*npc->creature_sets));
	if (!npc->creature_sets) {
		goto err;
	}

	for (int i = 0;	i < npc->hdr.creature_sets_count; i++) {
		struct pw_npc_creature_set *set = &npc->creature_sets[i];

		fread(&set->data, 1, sizeof(set->data), fp);
		if (set->data.groups_count == 0) continue;

		set->groups = calloc(set->data.groups_count, sizeof(*set->groups));
		for (int j = 0; j < set->data.groups_count; j++) {
			struct pw_npc_creature_group *grp = &set->groups[j];

			fread(&grp->data, 1, sizeof(grp->data), fp);
		}
	}

	npc->resource_sets = calloc(npc->hdr.resource_sets_count, sizeof(*npc->resource_sets));
	if (!npc->resource_sets) {
		goto err;
	}

	for (int i = 0;	i < npc->hdr.resource_sets_count; i++) {
		struct pw_npc_resource_set *set = &npc->resource_sets[i];

		fread(&set->data, 1, sizeof(set->data), fp);
		if (set->data.groups_count == 0) continue;

		set->groups = calloc(set->data.groups_count, sizeof(*set->groups));
		for (int j = 0; j < set->data.groups_count; j++) {
			struct pw_npc_resource_group *grp = &set->groups[j];

			fread(&grp->data, 1, sizeof(grp->data), fp);
		}
	}

	npc->dynamics = calloc(npc->hdr.dynamics_count, sizeof(*npc->dynamics));
	if (!npc->dynamics) {
		goto err;
	}

	for (int i = 0;	i < npc->hdr.dynamics_count; i++) {
		struct pw_npc_dynamic *dyn = &npc->dynamics[i];

		fread(&dyn->data, 1, sizeof(dyn->data), fp);
	}

	npc->triggers = calloc(npc->hdr.triggers_count, sizeof(*npc->triggers));
	if (!npc->triggers) {
		goto err;
	}

	for (int i = 0;	i < npc->hdr.triggers_count; i++) {
		struct pw_npc_trigger *trig = &npc->triggers[i];

		fread(&trig->data, 1, sizeof(trig->data), fp);
	}

	fclose(fp);
	return 0;

err:
	fclose(fp);
	free(npc->creature_sets);
	free(npc->resource_sets);
	free(npc->dynamics);
	free(npc->triggers);
	return -errno;
}

int
pw_npcs_save(struct pw_npc_file *npc, const char *file_path)
{
	FILE *fp;
	uint32_t creature_count = 0;

	fp = fopen(file_path, "wb");
	if (fp == NULL) {
		fprintf(stderr, "Cant open %s\n", file_path);
		return -errno;
	}

	fwrite(&npc->hdr, 1, sizeof(npc->hdr), fp);
	for (int i = 0;	i < npc->hdr.creature_sets_count; i++) {
		struct pw_npc_creature_set *set = &npc->creature_sets[i];
		size_t off_begin = ftell(fp);
		int groups = 0;

		fwrite(&set->data, 1, sizeof(set->data), fp);

		for (int j = 0; j < set->data.groups_count; j++) {
			struct pw_npc_creature_group *grp = &set->groups[j];

			if (grp->data.type) {
				fwrite(&grp->data, 1, sizeof(grp->data), fp);
				groups++;
			}
		}
		if (groups == 0) {
			/* skip */
			fseek(fp, off_begin, SEEK_SET);
		}
	}

	for (int i = 0;	i < npc->hdr.resource_sets_count; i++) {
		struct pw_npc_resource_set *set = &npc->resource_sets[i];

		fwrite(&set->data, 1, sizeof(set->data), fp);

		for (int j = 0; j < set->data.groups_count; j++) {
			struct pw_npc_resource_group *grp = &set->groups[j];

			fwrite(&grp->data, 1, sizeof(grp->data), fp);
		}
	}

	for (int i = 0;	i < npc->hdr.dynamics_count; i++) {
		struct pw_npc_dynamic *dyn = &npc->dynamics[i];

		fwrite(&dyn->data, 1, sizeof(dyn->data), fp);
	}

	for (int i = 0;	i < npc->hdr.triggers_count; i++) {
		struct pw_npc_trigger *trig = &npc->triggers[i];

		fwrite(&trig->data, 1, sizeof(trig->data), fp);
	}

	/* get back to header and write the real creature count */
	fseek(fp, 4, SEEK_SET);
	fwrite(&creature_count, sizeof(creature_count), 1, fp);

	fclose(fp);
	return 0;
}
