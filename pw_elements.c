/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>

#include "common.h"
#include "pw_elements.h"

#define TYPE_END 0
#define INT16 1
#define INT32 2
#define FLOAT 3
#define ARRAY_END 4
#define WSTRING(n) (0x1000 + (n))
#define STRING(n) (0x2000 + (n))
#define ARRAY_START(n) (0x3000 + (n))

struct pw_elements_serializer {
	const char *name;
	unsigned type;
};

static long
serialize(FILE *fp, struct pw_elements_serializer **slzr_table_p, void **data_p,
		unsigned data_cnt, bool skip_empty_objs, bool newlines)
{
	unsigned data_idx;
	struct pw_elements_serializer *slzr;
	void *data = *data_p;
	bool nonzero, obj_printed;
	long sz, arr_sz;

	fprintf(fp, "[");
	/* in case the arr is full of empty objects or just 0-fields -> print just [] */
	arr_sz = ftell(fp);
	for (data_idx = 0; data_idx < data_cnt; data_idx++) {
		slzr = *slzr_table_p;
		obj_printed = false;
		if (slzr->name[0] != 0) {
			obj_printed = true;
			fprintf(fp, "{");
		}
		/* when obj contains only 0-fields -> print {} */
		sz = ftell(fp);
		nonzero = false;

		while (true) {
			if (slzr->type == INT16) {
				if (!obj_printed || (*(uint16_t *)data != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%u,", *(uint16_t *)data);
					nonzero = *(uint16_t *)data != 0;
				}
				data += 2;
			} else if (slzr->type == INT32) {
				if (!obj_printed || (*(uint32_t *)data != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%u,", *(uint32_t *)data);
					nonzero = *(uint32_t *)data != 0;
				}
				data += 4;
			} else if (slzr->type == FLOAT) {
				if (!obj_printed || (*(float *)data != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%.8f,", *(float *)data);
					nonzero = *(float *)data != 0;
				}
				data += 4;
			} else if (slzr->type > WSTRING(0) && slzr->type <= WSTRING(0x1000)) {
				unsigned len = slzr->type - WSTRING(0);

				if (slzr->name[0] != '_') {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "\"");
					fwsprint(fp, (const uint16_t *)data, len);
					fprintf(fp, "\",");
					nonzero = *(uint16_t *)data != 0;
				}
				data += len * 2;
			} else if (slzr->type > STRING(0) && slzr->type <= STRING(0x1000)) {
				unsigned len = slzr->type - STRING(0);

				if (slzr->name[0] != '_') {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "\"");
					fprintf(fp, "%s", (char *)data);
					fprintf(fp, "\",");
					nonzero = *(char *)data != 0;
				}
				data += len;
			} else if (slzr->type > ARRAY_START(0) && slzr->type <= ARRAY_START(0x1000)) {
				unsigned cnt = slzr->type - ARRAY_START(0);

				if (slzr->name[0] != '_') {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					slzr++;
					serialize(fp, &slzr, &data, cnt, true, false);
					fprintf(fp, ",");
				}
			} else if (slzr->type == ARRAY_END) {
				break;
			} else if (slzr->type == TYPE_END) {
				break;
			}
			slzr++;
		}

		if (skip_empty_objs && !nonzero) {
			if (obj_printed) {
				/* go back to { */
				fseek(fp, sz, SEEK_SET);
			} else {
				/* overwrite previous comma */
				fseek(fp, -1, SEEK_CUR);
			}
		} else {
			/* overwrite previous comma */
			fseek(fp, -1, SEEK_CUR);
			/* save } of the last non-empty object since we may need to strip
			 * all subsequent objects from the array */
			arr_sz = ftell(fp) + (obj_printed ? 1 : 0);
		}

		if (obj_printed) {
			fprintf(fp, "}");
		}
		fprintf(fp, ",");
		if (newlines) {
			fprintf(fp, "\n");
		}
	}

	/* overwrite previous comma and strip empty objects from the array */
	fseek(fp, arr_sz, SEEK_SET);

	fprintf(fp, "]");

	*slzr_table_p = slzr;
	*data_p = data;
	return arr_sz + 1;
}

long
pw_elements_serialize(FILE *fp, struct pw_elements_serializer *slzr_table, void *data, unsigned data_cnt)
{
	return serialize(fp, &slzr_table, &data, data_cnt, false, true);
}

static struct pw_elements_serializer mine_essence_serializer[] = {
	{ "id", INT32 },
	{ "id_type", INT32 },
	{ "name", WSTRING(32) },
	{ "level", INT32 },
	{ "level_required", INT32 },
	{ "item_required", INT32 },
	{ "use_up_tool", INT32 },
	{ "time_min", INT32 },
	{ "time_max", INT32 },
	{ "exp", INT32 },
	{ "sp", INT32 },
	{ "file_model", WSTRING(64) },
	{ "mat_item", ARRAY_START(16) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "mat_count", ARRAY_START(2) },
		{ "num", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "task_in", INT32 },
	{ "task_out", INT32 },
	{ "uninterruptible", INT32 },
	{ "npcgen", ARRAY_START(4) },
		{ "mob_id", INT32 },
		{ "num", INT32 },
		{ "radius", FLOAT },
	{ "", ARRAY_END },
	{ "aggro_monster_faction", INT32 },
	{ "aggro_radius", FLOAT },
	{ "aggro_num", INT32 },
	{ "permanent", INT32 },
	{ "", TYPE_END },
};

static struct pw_elements_serializer monster_essence_serializer[] = {
	{ "id", INT32 },
	{ "id_type", INT32 },
	{ "name", WSTRING(32) },
	{ "prop", WSTRING(16) },
	{ "desc", WSTRING(16) },
	{ "faction", INT32 },
	{ "monster_faction", INT32 },
	{ "file_model", STRING(128) },
	{ "file_gfx_short", STRING(128) },
	{ "file_gfx_short_hit", STRING(128) },
	{ "size", FLOAT },
	{ "damage_delay", FLOAT },
	{ "id_strategy", INT32 },
	{ "role_in_war", INT32 },
	{ "level", INT32 },
	{ "show_level", INT32 },
	{ "id_pet_egg", INT32 },
	{ "hp", INT32 },
	{ "phys_def", INT32 },
	{ "magic_def", ARRAY_START(5) },
		{ "", FLOAT },
	{ "", ARRAY_END },
	{ "immune_type", INT32 },
	{ "exp", INT32 },
	{ "sp", INT32 },
	{ "money_average", INT32 },
	{ "money_var", INT32 },
	{ "short_range_mode", INT32 },
	{ "sight_range", INT32 },
	{ "attack", INT32 },
	{ "armor", INT32 },
	{ "damage_min", INT32 },
	{ "damage_max", INT32 },
	{ "magic_damage_ext", ARRAY_START(5) },
		{ "min", INT32 },
		{ "max", INT32 },
	{ "", ARRAY_END },
	{ "attack_range", FLOAT },
	{ "attack_speed", FLOAT },
	{ "magic_damage_min", INT32 },
	{ "magic_damage_max", INT32 },
	{ "skill_id", INT32 },
	{ "skill_level", INT32 },
	{ "hp_regenerate", INT32 },
	{ "is_aggressive", INT32 },
	{ "monster_faction_ask_help", INT32 },
	{ "monster_faction_can_help", INT32 },
	{ "aggro_range", FLOAT },
	{ "aggro_time", FLOAT },
	{ "inhabit_type", INT32 },
	{ "patrol_mode", INT32 },
	{ "stand_mode", INT32 },
	{ "walk_speed", FLOAT },
	{ "run_speed", FLOAT },
	{ "fly_speed", FLOAT },
	{ "swim_speed", FLOAT },
	{ "common_strategy", INT32 },
	{ "aggro_strategy", ARRAY_START(4) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "hp_autoskill", ARRAY_START(3) },
		{ "", ARRAY_START(5) },
			{ "id", INT32 },
			{ "level", INT32 },
			{ "prob", FLOAT },
		{ "", ARRAY_END },
	{ "", ARRAY_END },
	{ "skill_on_death", INT32 },
	{ "skill", ARRAY_START(32) },
		{ "id", INT32 },
		{ "level", INT32 },
	{ "", ARRAY_END },
	{ "probability_drop_num", ARRAY_START(4) },
		{ "", FLOAT },
	{ "", ARRAY_END },
	{ "drop_times", INT32 },
	{ "drop_protected", INT32 },
	{ "drop_matter", ARRAY_START(32) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "", TYPE_END },
};

static struct pw_elements_serializer recipe_essence_serializer[] = {
	{ "id", INT32 },
	{ "id_major_type", INT32 },
	{ "id_sub_type", INT32 },
	{ "name", WSTRING(32) },
	{ "recipe_level", INT32 },
	{ "skill_id", INT32 },
	{ "skill_level", INT32 },
	{ "_bind_type", INT32 },
	{ "targets", ARRAY_START(4) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "fail_prob", FLOAT },
	{ "num_to_make", INT32 },
	{ "coins", INT32 },
	{ "duration", INT32 },
	{ "xp", INT32 },
	{ "sp", INT32 },
	{ "mats", ARRAY_START(32) },
		{ "num", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "", TYPE_END },
};

static struct pw_elements_serializer npc_sell_service_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "pages", ARRAY_START(8) },
		{ "page_title", WSTRING(8) },
		{ "item_id", ARRAY_START(32) },
			{ "", INT32 },
		{ "", ARRAY_END },
	{ "", ARRAY_END },
	{ "id_dialog", INT32 },
	{ "", TYPE_END },
};

static struct pw_elements_serializer npc_essence_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "id_type", INT32 },
	{ "refresh_time", FLOAT },
	{ "attack_rule", INT32 },
	{ "file_model", STRING(128) },
	{ "tax_rate", FLOAT },
	{ "base_monster_id", INT32 },
	{ "greeting", WSTRING(256) },
	{ "target_id", INT32 },
	{ "domain_related", INT32 },
	{ "id_talk_service", INT32 },
	{ "id_sell_service", INT32 },
	{ "id_buy_service", INT32 },
	{ "id_repair_service", INT32 },
	{ "id_install_service", INT32 },
	{ "id_uninstall_service", INT32 },
	{ "id_task_out_service", INT32 },
	{ "id_task_in_service", INT32 },
	{ "id_task_matter_service", INT32 },
	{ "id_skill_service", INT32 },
	{ "id_heal_service", INT32 },
	{ "id_transmit_service", INT32 },
	{ "id_transport_service", INT32 },
	{ "id_proxy_service", INT32 },
	{ "id_storage_service", INT32 },
	{ "id_make_service", INT32 },
	{ "id_decompose_service", INT32 },
	{ "id_identify_service", INT32 },
	{ "id_war_towerbuild_service", INT32 },
	{ "id_resetprop_service", INT32 },
	{ "id_petname_service", INT32 },
	{ "id_petlearnskill_service", INT32 },
	{ "id_petforgetskill_service", INT32 },
	{ "id_equipbind_service", INT32 },
	{ "id_equipdestroy_service", INT32 },
	{ "id_equipundestroy_service", INT32 },
	{ "combined_services", INT32 },
	{ "id_mine", INT32 },
	{ "", TYPE_END },
};

static struct pw_elements_serializer npc_make_service_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "make_skill_id", INT32 },
	{ "produce_type", INT32 },
	{ "pages", ARRAY_START(8) },
		{ "page_title", WSTRING(8) },
		{ "item_id", ARRAY_START(32) },
			{ "", INT32 },
		{ "", ARRAY_END },
	{ "", ARRAY_END },
	{ "", TYPE_END },
};

static int32_t
pw_elements_load_table(void **table, uint32_t el_size, FILE *fp)
{
		int32_t count;

		fread(&count, 1, sizeof(count), fp);
		*table = calloc(count, el_size);
		fread(*table, 1, count * el_size, fp);

		return count;
}

static void
pw_elements_save_table(void *table, uint32_t el_size, uint32_t el_count, FILE *fp)
{
	size_t count_off = ftell(fp);
	uint32_t count = 0;

	fwrite(&count, sizeof(count), 1, fp);

	for (uint32_t i = 0; i < el_count; i++) {
		void *el = table + i * el_size;
		if (*(uint32_t *)el == 0) continue;
		fwrite(el, el_size, 1, fp);
		count++;
	}

	size_t end_off = ftell(fp);
	fseek(fp, count_off, SEEK_SET);
	fwrite(&count, 1, sizeof(count), fp);
	fseek(fp, end_off, SEEK_SET);
}

static void
pw_elements_read_talk_proc(struct talk_proc *talk, FILE *fp)
{
	fread(&talk->id, 1, sizeof(talk->id), fp);
	fread(&talk->name, 1, sizeof(talk->name), fp);

	fread(&talk->questions_cnt, 1, sizeof(talk->questions_cnt), fp);
	talk->questions = calloc(talk->questions_cnt, sizeof(*talk->questions));
	for (int q = 0; q < talk->questions_cnt; ++q) {
			struct question *question = &talk->questions[q];
			fread(&question->id, 1, sizeof(question->id), fp);
			fread(&question->control, 1, sizeof(question->control), fp);

			fread(&question->text_size, 1, sizeof(question->text_size), fp);
			question->text = calloc(question->text_size, sizeof(*question->text));
			fread(question->text, 1, question->text_size * sizeof(*question->text), fp);

			fread(&question->choices_cnt, 1, sizeof(question->choices_cnt), fp);
			question->choices = calloc(question->choices_cnt, sizeof(*question->choices));
			fread(question->choices, 1, question->choices_cnt * sizeof(*question->choices), fp);
	}
}

static int32_t
pw_elements_load_talk_proc(struct talk_proc **table, FILE *fp)
{
		int32_t count;

		fread(&count, 1, sizeof(count), fp);
		*table = calloc(count, sizeof(struct talk_proc));

		for (int i = 0; i < count; ++i) {
				struct talk_proc *talk = &(*table)[i];

		pw_elements_read_talk_proc(talk, fp);
		}

		return count;
}

static void
pw_elements_write_talk_proc(struct talk_proc *talk, FILE *fp)
{
	fwrite(&talk->id, 1, sizeof(talk->id), fp);
	fwrite(&talk->name, 1, sizeof(talk->name), fp);

	fwrite(&talk->questions_cnt, 1, sizeof(talk->questions_cnt), fp);
	for (int q = 0; q < talk->questions_cnt; ++q) {
		struct question *question = &talk->questions[q];
		fwrite(&question->id, 1, sizeof(question->id), fp);
		fwrite(&question->control, 1, sizeof(question->control), fp);

		fwrite(&question->text_size, 1, sizeof(question->text_size), fp);
		fwrite(question->text, 1, question->text_size * sizeof(*question->text), fp);

		fwrite(&question->choices_cnt, 1, sizeof(question->choices_cnt), fp);
		fwrite(question->choices, 1, question->choices_cnt * sizeof(*question->choices), fp);
	}
}

static void
pw_elements_save_talk_proc(struct talk_proc *table, uint32_t count, FILE *fp)
{
		fwrite(&count, 1, sizeof(count), fp);
		for (int i = 0; i < count; ++i) {
				struct talk_proc *talk = &table[i];

		pw_elements_write_talk_proc(talk, fp);
	}
}

static void
load_control_block_0(struct control_block0 *block, FILE *fp)
{
		fread(&block->unk1, 1, sizeof(block->unk1), fp);
		fread(&block->size, 1, sizeof(block->size), fp);
		block->unk2 = calloc(1, block->size);
		fread(block->unk2, 1, block->size, fp);
		fread(&block->unk3, 1,sizeof(block->unk3), fp);
}

static void
save_control_block_0(struct control_block0 *block, FILE *fp)
{
		fwrite(&block->unk1, 1, sizeof(block->unk1), fp);
		fwrite(&block->size, 1, sizeof(block->size), fp);
		fwrite(block->unk2, 1, block->size, fp);
		fwrite(&block->unk3, 1,sizeof(block->unk3), fp);
}

static void
load_control_block_1(struct control_block1 *block, FILE *fp)
{
		fread(&block->unk1, 1, sizeof(block->unk1), fp);
		fread(&block->size, 1, sizeof(block->size), fp);
		block->unk2 = calloc(1, block->size);
		fread(block->unk2, 1, block->size, fp);
}

static void
save_control_block_1(struct control_block1 *block, FILE *fp)
{
		fwrite(&block->unk1, 1, sizeof(block->unk1), fp);
		fwrite(&block->size, 1, sizeof(block->size), fp);
		fwrite(block->unk2, 1, block->size, fp);
}

int
pw_elements_load(struct pw_elements *el, const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		fprintf(stderr, "cant open %s\n", filename);
		return 1;
	}

	memset(el, 0, sizeof(*el));

	fread(&el->hdr, 1, sizeof(el->hdr), fp);
	if (el->hdr.version != 12) {
		fprintf(stderr, "element version mismatch, expected 12, found %d\n", el->hdr.version);
		fclose(fp);
		return 1;
	}

	el->mine_essence_serializer = mine_essence_serializer;
	el->monster_essence_serializer = monster_essence_serializer;
	el->recipe_essence_serializer = recipe_essence_serializer;
	el->npc_essence_serializer = npc_essence_serializer;
	el->npc_sell_service_serializer = npc_sell_service_serializer;
	el->npc_make_service_serializer = npc_make_service_serializer;

	el->equipment_addon_cnt = pw_elements_load_table((void **)&el->equipment_addon, sizeof(struct equipment_addon), fp);
	el->weapon_major_type_cnt = pw_elements_load_table((void **)&el->weapon_major_type, sizeof(struct weapon_major_type), fp);
	el->weapon_sub_type_cnt = pw_elements_load_table((void **)&el->weapon_sub_type, sizeof(struct weapon_sub_type), fp);
	el->weapon_essence_cnt = pw_elements_load_table((void **)&el->weapon_essence, sizeof(struct weapon_essence), fp);
	el->armor_major_type_cnt = pw_elements_load_table((void **)&el->armor_major_type, sizeof(struct armor_major_type), fp);
	el->armor_sub_type_cnt = pw_elements_load_table((void **)&el->armor_sub_type, sizeof(struct armor_sub_type), fp);
	el->armor_essence_cnt = pw_elements_load_table((void **)&el->armor_essence, sizeof(struct armor_essence), fp);
	el->decoration_major_type_cnt = pw_elements_load_table((void **)&el->decoration_major_type, sizeof(struct decoration_major_type), fp);
	el->decoration_sub_type_cnt = pw_elements_load_table((void **)&el->decoration_sub_type, sizeof(struct decoration_sub_type), fp);
	el->decoration_essence_cnt = pw_elements_load_table((void **)&el->decoration_essence, sizeof(struct decoration_essence), fp);
	el->medicine_major_type_cnt = pw_elements_load_table((void **)&el->medicine_major_type, sizeof(struct medicine_major_type), fp);
	el->medicine_sub_type_cnt = pw_elements_load_table((void **)&el->medicine_sub_type, sizeof(struct medicine_sub_type), fp);
	el->medicine_essence_cnt = pw_elements_load_table((void **)&el->medicine_essence, sizeof(struct medicine_essence), fp);
	el->material_major_type_cnt = pw_elements_load_table((void **)&el->material_major_type, sizeof(struct material_major_type), fp);
	el->material_sub_type_cnt = pw_elements_load_table((void **)&el->material_sub_type, sizeof(struct material_sub_type), fp);
	el->material_essence_cnt = pw_elements_load_table((void **)&el->material_essence, sizeof(struct material_essence), fp);
	el->damagerune_sub_type_cnt = pw_elements_load_table((void **)&el->damagerune_sub_type, sizeof(struct damagerune_sub_type), fp);
	el->damagerune_essence_cnt = pw_elements_load_table((void **)&el->damagerune_essence, sizeof(struct damagerune_essence), fp);
	el->armorrune_sub_type_cnt = pw_elements_load_table((void **)&el->armorrune_sub_type, sizeof(struct armorrune_sub_type), fp);
	el->armorrune_essence_cnt = pw_elements_load_table((void **)&el->armorrune_essence, sizeof(struct armorrune_essence), fp);
	load_control_block_0(&el->control_block0, fp);
	el->skilltome_sub_type_cnt = pw_elements_load_table((void **)&el->skilltome_sub_type, sizeof(struct skilltome_sub_type), fp);
	el->skilltome_essence_cnt = pw_elements_load_table((void **)&el->skilltome_essence, sizeof(struct skilltome_essence), fp);
	el->flysword_essence_cnt = pw_elements_load_table((void **)&el->flysword_essence, sizeof(struct flysword_essence), fp);
	el->wingmanwing_essence_cnt = pw_elements_load_table((void **)&el->wingmanwing_essence, sizeof(struct wingmanwing_essence), fp);
	el->townscroll_essence_cnt = pw_elements_load_table((void **)&el->townscroll_essence, sizeof(struct townscroll_essence), fp);
	el->unionscroll_essence_cnt = pw_elements_load_table((void **)&el->unionscroll_essence, sizeof(struct unionscroll_essence), fp);
	el->revivescroll_essence_cnt = pw_elements_load_table((void **)&el->revivescroll_essence, sizeof(struct revivescroll_essence), fp);
	el->element_essence_cnt = pw_elements_load_table((void **)&el->element_essence, sizeof(struct element_essence), fp);
	el->taskmatter_essence_cnt = pw_elements_load_table((void **)&el->taskmatter_essence, sizeof(struct taskmatter_essence), fp);
	el->tossmatter_essence_cnt = pw_elements_load_table((void **)&el->tossmatter_essence, sizeof(struct tossmatter_essence), fp);
	el->projectile_type_cnt = pw_elements_load_table((void **)&el->projectile_type, sizeof(struct projectile_type), fp);
	el->projectile_essence_cnt = pw_elements_load_table((void **)&el->projectile_essence, sizeof(struct projectile_essence), fp);
	el->quiver_sub_type_cnt = pw_elements_load_table((void **)&el->quiver_sub_type, sizeof(struct quiver_sub_type), fp);
	el->quiver_essence_cnt = pw_elements_load_table((void **)&el->quiver_essence, sizeof(struct quiver_essence), fp);
	el->stone_sub_type_cnt = pw_elements_load_table((void **)&el->stone_sub_type, sizeof(struct stone_sub_type), fp);
	el->stone_essence_cnt = pw_elements_load_table((void **)&el->stone_essence, sizeof(struct stone_essence), fp);
	el->monster_addon_cnt = pw_elements_load_table((void **)&el->monster_addon, sizeof(struct monster_addon), fp);
	el->monster_type_cnt = pw_elements_load_table((void **)&el->monster_type, sizeof(struct monster_type), fp);
	el->monster_essence_cnt = pw_elements_load_table((void **)&el->monster_essence, sizeof(struct monster_essence), fp);
	el->npc_talk_service_cnt = pw_elements_load_table((void **)&el->npc_talk_service, sizeof(struct npc_talk_service), fp);
	el->npc_sell_service_cnt = pw_elements_load_table((void **)&el->npc_sell_service, sizeof(struct npc_sell_service), fp);
	el->npc_buy_service_cnt = pw_elements_load_table((void **)&el->npc_buy_service, sizeof(struct npc_buy_service), fp);
	el->npc_repair_service_cnt = pw_elements_load_table((void **)&el->npc_repair_service, sizeof(struct npc_repair_service), fp);
	el->npc_install_service_cnt = pw_elements_load_table((void **)&el->npc_install_service, sizeof(struct npc_install_service), fp);
	el->npc_uninstall_service_cnt = pw_elements_load_table((void **)&el->npc_uninstall_service, sizeof(struct npc_uninstall_service), fp);
	el->npc_task_in_service_cnt = pw_elements_load_table((void **)&el->npc_task_in_service, sizeof(struct npc_task_in_service), fp);
	el->npc_task_out_service_cnt = pw_elements_load_table((void **)&el->npc_task_out_service, sizeof(struct npc_task_out_service), fp);
	el->npc_task_matter_service_cnt = pw_elements_load_table((void **)&el->npc_task_matter_service, sizeof(struct npc_task_matter_service), fp);
	el->npc_skill_service_cnt = pw_elements_load_table((void **)&el->npc_skill_service, sizeof(struct npc_skill_service), fp);
	el->npc_heal_service_cnt = pw_elements_load_table((void **)&el->npc_heal_service, sizeof(struct npc_heal_service), fp);
	el->npc_transmit_service_cnt = pw_elements_load_table((void **)&el->npc_transmit_service, sizeof(struct npc_transmit_service), fp);
	el->npc_transport_service_cnt = pw_elements_load_table((void **)&el->npc_transport_service, sizeof(struct npc_transport_service), fp);
	el->npc_proxy_service_cnt = pw_elements_load_table((void **)&el->npc_proxy_service, sizeof(struct npc_proxy_service), fp);
	el->npc_storage_service_cnt = pw_elements_load_table((void **)&el->npc_storage_service, sizeof(struct npc_storage_service), fp);
	el->npc_make_service_cnt = pw_elements_load_table((void **)&el->npc_make_service, sizeof(struct npc_make_service), fp);
	el->npc_decompose_service_cnt = pw_elements_load_table((void **)&el->npc_decompose_service, sizeof(struct npc_decompose_service), fp);
	el->npc_type_cnt = pw_elements_load_table((void **)&el->npc_type, sizeof(struct npc_type), fp);
	el->npc_essence_cnt = pw_elements_load_table((void **)&el->npc_essence, sizeof(struct npc_essence), fp);
	el->talk_proc_cnt = pw_elements_load_talk_proc(&el->talk_proc, fp);
	el->face_texture_essence_cnt = pw_elements_load_table((void **)&el->face_texture_essence, sizeof(struct face_texture_essence), fp);
	el->face_shape_essence_cnt = pw_elements_load_table((void **)&el->face_shape_essence, sizeof(struct face_shape_essence), fp);
	el->face_emotion_type_cnt = pw_elements_load_table((void **)&el->face_emotion_type, sizeof(struct face_emotion_type), fp);
	el->face_expression_essence_cnt = pw_elements_load_table((void **)&el->face_expression_essence, sizeof(struct face_expression_essence), fp);
	el->face_hair_essence_cnt = pw_elements_load_table((void **)&el->face_hair_essence, sizeof(struct face_hair_essence), fp);
	el->face_moustache_essence_cnt = pw_elements_load_table((void **)&el->face_moustache_essence, sizeof(struct face_moustache_essence), fp);
	el->colorpicker_essence_cnt = pw_elements_load_table((void **)&el->colorpicker_essence, sizeof(struct colorpicker_essence), fp);
	el->customizedata_essence_cnt = pw_elements_load_table((void **)&el->customizedata_essence, sizeof(struct customizedata_essence), fp);
	el->recipe_major_type_cnt = pw_elements_load_table((void **)&el->recipe_major_type, sizeof(struct recipe_major_type), fp);
	el->recipe_sub_type_cnt = pw_elements_load_table((void **)&el->recipe_sub_type, sizeof(struct recipe_sub_type), fp);
	el->recipe_essence_cnt = pw_elements_load_table((void **)&el->recipe_essence, sizeof(struct recipe_essence), fp);
	el->enemy_faction_config_cnt = pw_elements_load_table((void **)&el->enemy_faction_config, sizeof(struct enemy_faction_config), fp);
	el->charracter_class_config_cnt = pw_elements_load_table((void **)&el->charracter_class_config, sizeof(struct charracter_class_config), fp);
	el->param_adjust_config_cnt = pw_elements_load_table((void **)&el->param_adjust_config, sizeof(struct param_adjust_config), fp);
	el->player_action_info_config_cnt = pw_elements_load_table((void **)&el->player_action_info_config, sizeof(struct player_action_info_config), fp);
	el->taskdice_essence_cnt = pw_elements_load_table((void **)&el->taskdice_essence, sizeof(struct taskdice_essence), fp);
	el->tasknormalmatter_essence_cnt = pw_elements_load_table((void **)&el->tasknormalmatter_essence, sizeof(struct tasknormalmatter_essence), fp);
	el->face_faling_essence_cnt = pw_elements_load_table((void **)&el->face_faling_essence, sizeof(struct face_faling_essence), fp);
	el->player_levelexp_config_cnt = pw_elements_load_table((void **)&el->player_levelexp_config, sizeof(struct player_levelexp_config), fp);
	el->mine_type_cnt = pw_elements_load_table((void **)&el->mine_type, sizeof(struct mine_type), fp);
	el->mine_essence_cnt = pw_elements_load_table((void **)&el->mine_essence, sizeof(struct mine_essence), fp);
	el->npc_identify_service_cnt = pw_elements_load_table((void **)&el->npc_identify_service, sizeof(struct npc_identify_service), fp);
	el->fashion_major_type_cnt = pw_elements_load_table((void **)&el->fashion_major_type, sizeof(struct fashion_major_type), fp);
	el->fashion_sub_type_cnt = pw_elements_load_table((void **)&el->fashion_sub_type, sizeof(struct fashion_sub_type), fp);
	el->fashion_essence_cnt = pw_elements_load_table((void **)&el->fashion_essence, sizeof(struct fashion_essence), fp);
	el->faceticket_major_type_cnt = pw_elements_load_table((void **)&el->faceticket_major_type, sizeof(struct faceticket_major_type), fp);
	el->faceticket_sub_type_cnt = pw_elements_load_table((void **)&el->faceticket_sub_type, sizeof(struct faceticket_sub_type), fp);
	el->faceticket_essence_cnt = pw_elements_load_table((void **)&el->faceticket_essence, sizeof(struct faceticket_essence), fp);
	el->facepill_major_type_cnt = pw_elements_load_table((void **)&el->facepill_major_type, sizeof(struct facepill_major_type), fp);
	el->facepill_sub_type_cnt = pw_elements_load_table((void **)&el->facepill_sub_type, sizeof(struct facepill_sub_type), fp);
	el->facepill_essence_cnt = pw_elements_load_table((void **)&el->facepill_essence, sizeof(struct facepill_essence), fp);
	el->suite_essence_cnt = pw_elements_load_table((void **)&el->suite_essence, sizeof(struct suite_essence), fp);
	el->gm_generator_type_cnt = pw_elements_load_table((void **)&el->gm_generator_type, sizeof(struct gm_generator_type), fp);
	el->gm_generator_essence_cnt = pw_elements_load_table((void **)&el->gm_generator_essence, sizeof(struct gm_generator_essence), fp);
	el->pet_type_cnt = pw_elements_load_table((void **)&el->pet_type, sizeof(struct pet_type), fp);
	el->pet_essence_cnt = pw_elements_load_table((void **)&el->pet_essence, sizeof(struct pet_essence), fp);
	el->pet_egg_essence_cnt = pw_elements_load_table((void **)&el->pet_egg_essence, sizeof(struct pet_egg_essence), fp);
	el->pet_food_essence_cnt = pw_elements_load_table((void **)&el->pet_food_essence, sizeof(struct pet_food_essence), fp);
	el->pet_faceticket_essence_cnt = pw_elements_load_table((void **)&el->pet_faceticket_essence, sizeof(struct pet_faceticket_essence), fp);
	el->fireworks_essence_cnt = pw_elements_load_table((void **)&el->fireworks_essence, sizeof(struct fireworks_essence), fp);
	el->war_tankcallin_essence_cnt = pw_elements_load_table((void **)&el->war_tankcallin_essence, sizeof(struct war_tankcallin_essence), fp);
	load_control_block_1(&el->control_block1, fp);
	el->npc_war_towerbuild_service_cnt = pw_elements_load_table((void **)&el->npc_war_towerbuild_service, sizeof(struct npc_war_towerbuild_service), fp);
	el->player_secondlevel_config_cnt = pw_elements_load_table((void **)&el->player_secondlevel_config, sizeof(struct player_secondlevel_config), fp);
	el->npc_resetprop_service_cnt = pw_elements_load_table((void **)&el->npc_resetprop_service, sizeof(struct npc_resetprop_service), fp);
	el->npc_petname_service_cnt = pw_elements_load_table((void **)&el->npc_petname_service, sizeof(struct npc_petname_service), fp);
	el->npc_petlearnskill_service_cnt = pw_elements_load_table((void **)&el->npc_petlearnskill_service, sizeof(struct npc_petlearnskill_service), fp);
	el->npc_petforgetskill_service_cnt = pw_elements_load_table((void **)&el->npc_petforgetskill_service, sizeof(struct npc_petforgetskill_service), fp);
	el->skillmatter_essence_cnt = pw_elements_load_table((void **)&el->skillmatter_essence, sizeof(struct skillmatter_essence), fp);
	el->refine_ticket_essence_cnt = pw_elements_load_table((void **)&el->refine_ticket_essence, sizeof(struct refine_ticket_essence), fp);
	el->destroying_essence_cnt = pw_elements_load_table((void **)&el->destroying_essence, sizeof(struct destroying_essence), fp);
	el->npc_equipbind_service_cnt = pw_elements_load_table((void **)&el->npc_equipbind_service, sizeof(struct npc_equipbind_service), fp);
	el->npc_equipdestroy_service_cnt = pw_elements_load_table((void **)&el->npc_equipdestroy_service, sizeof(struct npc_equipdestroy_service), fp);
	el->npc_equipundestroy_service_cnt = pw_elements_load_table((void **)&el->npc_equipundestroy_service, sizeof(struct npc_equipundestroy_service), fp);
	el->bible_essence_cnt = pw_elements_load_table((void **)&el->bible_essence, sizeof(struct bible_essence), fp);
	el->speaker_essence_cnt = pw_elements_load_table((void **)&el->speaker_essence, sizeof(struct speaker_essence), fp);
	el->autohp_essence_cnt = pw_elements_load_table((void **)&el->autohp_essence, sizeof(struct autohp_essence), fp);
	el->automp_essence_cnt = pw_elements_load_table((void **)&el->automp_essence, sizeof(struct automp_essence), fp);
	el->double_exp_essence_cnt = pw_elements_load_table((void **)&el->double_exp_essence, sizeof(struct double_exp_essence), fp);
	el->transmitscroll_essence_cnt = pw_elements_load_table((void **)&el->transmitscroll_essence, sizeof(struct transmitscroll_essence), fp);
	el->dye_ticket_essence_cnt = pw_elements_load_table((void **)&el->dye_ticket_essence, sizeof(struct dye_ticket_essence), fp);

	fclose(fp);
	return 0;
}

int
pw_elements_save(struct pw_elements *el, const char *filename)
{
	FILE *fp = fopen(filename, "wb");
	if (fp == NULL) {
		fprintf(stderr, "cant open %s\n", filename);
		return 1;
	}

	fwrite(&el->hdr, 1, sizeof(el->hdr), fp);

	pw_elements_save_table((void *)el->equipment_addon, sizeof(struct equipment_addon), el->equipment_addon_cnt, fp);
	pw_elements_save_table((void *)el->weapon_major_type, sizeof(struct weapon_major_type), el->weapon_major_type_cnt, fp);
	pw_elements_save_table((void *)el->weapon_sub_type, sizeof(struct weapon_sub_type), el->weapon_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->weapon_essence, sizeof(struct weapon_essence), el->weapon_essence_cnt, fp);
	pw_elements_save_table((void *)el->armor_major_type, sizeof(struct armor_major_type), el->armor_major_type_cnt, fp);
	pw_elements_save_table((void *)el->armor_sub_type, sizeof(struct armor_sub_type), el->armor_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->armor_essence, sizeof(struct armor_essence), el->armor_essence_cnt, fp);
	pw_elements_save_table((void *)el->decoration_major_type, sizeof(struct decoration_major_type), el->decoration_major_type_cnt, fp);
	pw_elements_save_table((void *)el->decoration_sub_type, sizeof(struct decoration_sub_type), el->decoration_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->decoration_essence, sizeof(struct decoration_essence), el->decoration_essence_cnt, fp);
	pw_elements_save_table((void *)el->medicine_major_type, sizeof(struct medicine_major_type), el->medicine_major_type_cnt, fp);
	pw_elements_save_table((void *)el->medicine_sub_type, sizeof(struct medicine_sub_type), el->medicine_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->medicine_essence, sizeof(struct medicine_essence), el->medicine_essence_cnt, fp);
	pw_elements_save_table((void *)el->material_major_type, sizeof(struct material_major_type), el->material_major_type_cnt, fp);
	pw_elements_save_table((void *)el->material_sub_type, sizeof(struct material_sub_type), el->material_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->material_essence, sizeof(struct material_essence), el->material_essence_cnt, fp);
	pw_elements_save_table((void *)el->damagerune_sub_type, sizeof(struct damagerune_sub_type), el->damagerune_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->damagerune_essence, sizeof(struct damagerune_essence), el->damagerune_essence_cnt, fp);
	pw_elements_save_table((void *)el->armorrune_sub_type, sizeof(struct armorrune_sub_type), el->armorrune_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->armorrune_essence, sizeof(struct armorrune_essence), el->armorrune_essence_cnt, fp);
	save_control_block_0(&el->control_block0, fp);
	pw_elements_save_table((void *)el->skilltome_sub_type, sizeof(struct skilltome_sub_type), el->skilltome_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->skilltome_essence, sizeof(struct skilltome_essence), el->skilltome_essence_cnt, fp);
	pw_elements_save_table((void *)el->flysword_essence, sizeof(struct flysword_essence), el->flysword_essence_cnt, fp);
	pw_elements_save_table((void *)el->wingmanwing_essence, sizeof(struct wingmanwing_essence), el->wingmanwing_essence_cnt, fp);
	pw_elements_save_table((void *)el->townscroll_essence, sizeof(struct townscroll_essence), el->townscroll_essence_cnt, fp);
	pw_elements_save_table((void *)el->unionscroll_essence, sizeof(struct unionscroll_essence), el->unionscroll_essence_cnt, fp);
	pw_elements_save_table((void *)el->revivescroll_essence, sizeof(struct revivescroll_essence), el->revivescroll_essence_cnt, fp);
	pw_elements_save_table((void *)el->element_essence, sizeof(struct element_essence), el->element_essence_cnt, fp);
	pw_elements_save_table((void *)el->taskmatter_essence, sizeof(struct taskmatter_essence), el->taskmatter_essence_cnt, fp);
	pw_elements_save_table((void *)el->tossmatter_essence, sizeof(struct tossmatter_essence), el->tossmatter_essence_cnt, fp);
	pw_elements_save_table((void *)el->projectile_type, sizeof(struct projectile_type), el->projectile_type_cnt, fp);
	pw_elements_save_table((void *)el->projectile_essence, sizeof(struct projectile_essence), el->projectile_essence_cnt, fp);
	pw_elements_save_table((void *)el->quiver_sub_type, sizeof(struct quiver_sub_type), el->quiver_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->quiver_essence, sizeof(struct quiver_essence), el->quiver_essence_cnt, fp);
	pw_elements_save_table((void *)el->stone_sub_type, sizeof(struct stone_sub_type), el->stone_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->stone_essence, sizeof(struct stone_essence), el->stone_essence_cnt, fp);
	pw_elements_save_table((void *)el->monster_addon, sizeof(struct monster_addon), el->monster_addon_cnt, fp);
	pw_elements_save_table((void *)el->monster_type, sizeof(struct monster_type), el->monster_type_cnt, fp);
	pw_elements_save_table((void *)el->monster_essence, sizeof(struct monster_essence), el->monster_essence_cnt, fp);
	pw_elements_save_table((void *)el->npc_talk_service, sizeof(struct npc_talk_service), el->npc_talk_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_sell_service, sizeof(struct npc_sell_service), el->npc_sell_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_buy_service, sizeof(struct npc_buy_service), el->npc_buy_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_repair_service, sizeof(struct npc_repair_service), el->npc_repair_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_install_service, sizeof(struct npc_install_service), el->npc_install_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_uninstall_service, sizeof(struct npc_uninstall_service), el->npc_uninstall_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_task_in_service, sizeof(struct npc_task_in_service), el->npc_task_in_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_task_out_service, sizeof(struct npc_task_out_service), el->npc_task_out_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_task_matter_service, sizeof(struct npc_task_matter_service), el->npc_task_matter_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_skill_service, sizeof(struct npc_skill_service), el->npc_skill_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_heal_service, sizeof(struct npc_heal_service), el->npc_heal_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_transmit_service, sizeof(struct npc_transmit_service), el->npc_transmit_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_transport_service, sizeof(struct npc_transport_service), el->npc_transport_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_proxy_service, sizeof(struct npc_proxy_service), el->npc_proxy_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_storage_service, sizeof(struct npc_storage_service), el->npc_storage_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_make_service, sizeof(struct npc_make_service), el->npc_make_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_decompose_service, sizeof(struct npc_decompose_service), el->npc_decompose_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_type, sizeof(struct npc_type), el->npc_type_cnt, fp);
	pw_elements_save_table((void *)el->npc_essence, sizeof(struct npc_essence), el->npc_essence_cnt, fp);
	pw_elements_save_talk_proc(el->talk_proc, el->talk_proc_cnt, fp);
	pw_elements_save_table((void *)el->face_texture_essence, sizeof(struct face_texture_essence), el->face_texture_essence_cnt, fp);
	pw_elements_save_table((void *)el->face_shape_essence, sizeof(struct face_shape_essence), el->face_shape_essence_cnt, fp);
	pw_elements_save_table((void *)el->face_emotion_type, sizeof(struct face_emotion_type), el->face_emotion_type_cnt, fp);
	pw_elements_save_table((void *)el->face_expression_essence, sizeof(struct face_expression_essence), el->face_expression_essence_cnt, fp);
	pw_elements_save_table((void *)el->face_hair_essence, sizeof(struct face_hair_essence), el->face_hair_essence_cnt, fp);
	pw_elements_save_table((void *)el->face_moustache_essence, sizeof(struct face_moustache_essence), el->face_moustache_essence_cnt, fp);
	pw_elements_save_table((void *)el->colorpicker_essence, sizeof(struct colorpicker_essence), el->colorpicker_essence_cnt, fp);
	pw_elements_save_table((void *)el->customizedata_essence, sizeof(struct customizedata_essence), el->customizedata_essence_cnt, fp);
	pw_elements_save_table((void *)el->recipe_major_type, sizeof(struct recipe_major_type), el->recipe_major_type_cnt, fp);
	pw_elements_save_table((void *)el->recipe_sub_type, sizeof(struct recipe_sub_type), el->recipe_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->recipe_essence, sizeof(struct recipe_essence), el->recipe_essence_cnt, fp);
	pw_elements_save_table((void *)el->enemy_faction_config, sizeof(struct enemy_faction_config), el->enemy_faction_config_cnt, fp);
	pw_elements_save_table((void *)el->charracter_class_config, sizeof(struct charracter_class_config), el->charracter_class_config_cnt, fp);
	pw_elements_save_table((void *)el->param_adjust_config, sizeof(struct param_adjust_config), el->param_adjust_config_cnt, fp);
	pw_elements_save_table((void *)el->player_action_info_config, sizeof(struct player_action_info_config), el->player_action_info_config_cnt, fp);
	pw_elements_save_table((void *)el->taskdice_essence, sizeof(struct taskdice_essence), el->taskdice_essence_cnt, fp);
	pw_elements_save_table((void *)el->tasknormalmatter_essence, sizeof(struct tasknormalmatter_essence), el->tasknormalmatter_essence_cnt, fp);
	pw_elements_save_table((void *)el->face_faling_essence, sizeof(struct face_faling_essence), el->face_faling_essence_cnt, fp);
	pw_elements_save_table((void *)el->player_levelexp_config, sizeof(struct player_levelexp_config), el->player_levelexp_config_cnt, fp);
	pw_elements_save_table((void *)el->mine_type, sizeof(struct mine_type), el->mine_type_cnt, fp);
	pw_elements_save_table((void *)el->mine_essence, sizeof(struct mine_essence), el->mine_essence_cnt, fp);
	pw_elements_save_table((void *)el->npc_identify_service, sizeof(struct npc_identify_service), el->npc_identify_service_cnt, fp);
	pw_elements_save_table((void *)el->fashion_major_type, sizeof(struct fashion_major_type), el->fashion_major_type_cnt, fp);
	pw_elements_save_table((void *)el->fashion_sub_type, sizeof(struct fashion_sub_type), el->fashion_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->fashion_essence, sizeof(struct fashion_essence), el->fashion_essence_cnt, fp);
	pw_elements_save_table((void *)el->faceticket_major_type, sizeof(struct faceticket_major_type), el->faceticket_major_type_cnt, fp);
	pw_elements_save_table((void *)el->faceticket_sub_type, sizeof(struct faceticket_sub_type), el->faceticket_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->faceticket_essence, sizeof(struct faceticket_essence), el->faceticket_essence_cnt, fp);
	pw_elements_save_table((void *)el->facepill_major_type, sizeof(struct facepill_major_type), el->facepill_major_type_cnt, fp);
	pw_elements_save_table((void *)el->facepill_sub_type, sizeof(struct facepill_sub_type), el->facepill_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->facepill_essence, sizeof(struct facepill_essence), el->facepill_essence_cnt, fp);
	pw_elements_save_table((void *)el->suite_essence, sizeof(struct suite_essence), el->suite_essence_cnt, fp);
	pw_elements_save_table((void *)el->gm_generator_type, sizeof(struct gm_generator_type), el->gm_generator_type_cnt, fp);
	pw_elements_save_table((void *)el->gm_generator_essence, sizeof(struct gm_generator_essence), el->gm_generator_essence_cnt, fp);
	pw_elements_save_table((void *)el->pet_type, sizeof(struct pet_type), el->pet_type_cnt, fp);
	pw_elements_save_table((void *)el->pet_essence, sizeof(struct pet_essence), el->pet_essence_cnt, fp);
	pw_elements_save_table((void *)el->pet_egg_essence, sizeof(struct pet_egg_essence), el->pet_egg_essence_cnt, fp);
	pw_elements_save_table((void *)el->pet_food_essence, sizeof(struct pet_food_essence), el->pet_food_essence_cnt, fp);
	pw_elements_save_table((void *)el->pet_faceticket_essence, sizeof(struct pet_faceticket_essence), el->pet_faceticket_essence_cnt, fp);
	pw_elements_save_table((void *)el->fireworks_essence, sizeof(struct fireworks_essence), el->fireworks_essence_cnt, fp);
	pw_elements_save_table((void *)el->war_tankcallin_essence, sizeof(struct war_tankcallin_essence), el->war_tankcallin_essence_cnt, fp);
	save_control_block_1(&el->control_block1, fp);
	pw_elements_save_table((void *)el->npc_war_towerbuild_service, sizeof(struct npc_war_towerbuild_service), el->npc_war_towerbuild_service_cnt, fp);
	pw_elements_save_table((void *)el->player_secondlevel_config, sizeof(struct player_secondlevel_config), el->player_secondlevel_config_cnt, fp);
	pw_elements_save_table((void *)el->npc_resetprop_service, sizeof(struct npc_resetprop_service), el->npc_resetprop_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_petname_service, sizeof(struct npc_petname_service), el->npc_petname_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_petlearnskill_service, sizeof(struct npc_petlearnskill_service), el->npc_petlearnskill_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_petforgetskill_service, sizeof(struct npc_petforgetskill_service), el->npc_petforgetskill_service_cnt, fp);
	pw_elements_save_table((void *)el->skillmatter_essence, sizeof(struct skillmatter_essence), el->skillmatter_essence_cnt, fp);
	pw_elements_save_table((void *)el->refine_ticket_essence, sizeof(struct refine_ticket_essence), el->refine_ticket_essence_cnt, fp);
	pw_elements_save_table((void *)el->destroying_essence, sizeof(struct destroying_essence), el->destroying_essence_cnt, fp);
	pw_elements_save_table((void *)el->npc_equipbind_service, sizeof(struct npc_equipbind_service), el->npc_equipbind_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_equipdestroy_service, sizeof(struct npc_equipdestroy_service), el->npc_equipdestroy_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_equipundestroy_service, sizeof(struct npc_equipundestroy_service), el->npc_equipundestroy_service_cnt, fp);
	pw_elements_save_table((void *)el->bible_essence, sizeof(struct bible_essence), el->bible_essence_cnt, fp);
	pw_elements_save_table((void *)el->speaker_essence, sizeof(struct speaker_essence), el->speaker_essence_cnt, fp);
	pw_elements_save_table((void *)el->autohp_essence, sizeof(struct autohp_essence), el->autohp_essence_cnt, fp);
	pw_elements_save_table((void *)el->automp_essence, sizeof(struct automp_essence), el->automp_essence_cnt, fp);
	pw_elements_save_table((void *)el->double_exp_essence, sizeof(struct double_exp_essence), el->double_exp_essence_cnt, fp);
	pw_elements_save_table((void *)el->transmitscroll_essence, sizeof(struct transmitscroll_essence), el->transmitscroll_essence_cnt, fp);
	pw_elements_save_table((void *)el->dye_ticket_essence, sizeof(struct dye_ticket_essence), el->dye_ticket_essence_cnt, fp);

	fclose(fp);
	return 0;
}

int
pw_elements_save_srv(struct pw_elements *el, const char *filename)
{
	FILE* fp = fopen(filename, "wb");
	if (fp == NULL) {
		fprintf(stderr, "cant open %s\n", filename);
		return 1;
	}

	struct header {
			int16_t version;
			int16_t signature;
			int32_t unk1;
	} hdr;

	hdr.version = 10;
	hdr.signature = 12288;
	hdr.unk1 = 1196736793;
	fwrite(&hdr, 1, sizeof(hdr), fp);

	struct control_block0_10 {
			int32_t unk1;
			int32_t size;
			char *unk2;
			int32_t unk3;
	} cb0;
	cb0.unk1 = 2876672477;
	cb0.size = 7;
	cb0.unk2 = "\xdd\x89\x76\xab\x07\x00\x00\x00\xe7\x3f\xdc\x84\xe2\x27\xc0";
	cb0.unk3 = 1196736793;

	struct control_block1_10 {
			int32_t unk1;
			int32_t size;
			char *unk2;
	} cb1;
	cb1.unk1 = 3996477343;
	cb1.size = 121;
	cb1.unk2 = "\x21\xe6\x88\xdc\x32\xb3\x2c\x13\x98\x86\x8f\xbe\xcc\x54\x59\x4d\x3c\xed\x38\xad\x51\xc2\x04\x4c\x13\x67\x57\x58\xe2\x4c\x80\x56\x25\xe2\x8c\x9d\x13\xfa\x18\x07\xb4\x82\xda\xd2\x69\xc1\xd4\xe4\xed\x18\x85\x70\xe5\xc6\x1c\x40\x0f\x6b\x47\x30\xc2\x8c\xa0\x46\x21\xc6\x94\x91\x6f\x3a\x00\x17\xa8\x96\xc6\xba\x1c\x6c\x71\xf9\x34\xfd\x34\xad\x29\xda\x1c\x40\xd3\x63\x57\x08\xfa\x50\x88\x4e\x3d\xc2\x98\x59\x16\x9f\x1d\x22\xd9\x87\xaf\x66\x08\x50\x5d\x41\x30\xed\x40\xa1\x5d\xd2\x04\x8c\xd3";

	pw_elements_save_table((void *)el->equipment_addon, sizeof(struct equipment_addon), el->equipment_addon_cnt, fp);
	pw_elements_save_table((void *)el->weapon_major_type, sizeof(struct weapon_major_type), el->weapon_major_type_cnt, fp);
	pw_elements_save_table((void *)el->weapon_sub_type, sizeof(struct weapon_sub_type), el->weapon_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->weapon_essence, sizeof(struct weapon_essence), el->weapon_essence_cnt, fp);
	pw_elements_save_table((void *)el->armor_major_type, sizeof(struct armor_major_type), el->armor_major_type_cnt, fp);
	pw_elements_save_table((void *)el->armor_sub_type, sizeof(struct armor_sub_type), el->armor_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->armor_essence, sizeof(struct armor_essence), el->armor_essence_cnt, fp);
	pw_elements_save_table((void *)el->decoration_major_type, sizeof(struct decoration_major_type), el->decoration_major_type_cnt, fp);
	pw_elements_save_table((void *)el->decoration_sub_type, sizeof(struct decoration_sub_type), el->decoration_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->decoration_essence, sizeof(struct decoration_essence), el->decoration_essence_cnt, fp);
	pw_elements_save_table((void *)el->medicine_major_type, sizeof(struct medicine_major_type), el->medicine_major_type_cnt, fp);
	pw_elements_save_table((void *)el->medicine_sub_type, sizeof(struct medicine_sub_type), el->medicine_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->medicine_essence, sizeof(struct medicine_essence), el->medicine_essence_cnt, fp);
	pw_elements_save_table((void *)el->material_major_type, sizeof(struct material_major_type), el->material_major_type_cnt, fp);
	pw_elements_save_table((void *)el->material_sub_type, sizeof(struct material_sub_type), el->material_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->material_essence, sizeof(struct material_essence), el->material_essence_cnt, fp);
	pw_elements_save_table((void *)el->damagerune_sub_type, sizeof(struct damagerune_sub_type), el->damagerune_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->damagerune_essence, sizeof(struct damagerune_essence), el->damagerune_essence_cnt, fp);
	pw_elements_save_table((void *)el->armorrune_sub_type, sizeof(struct armorrune_sub_type), el->armorrune_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->armorrune_essence, sizeof(struct armorrune_essence), el->armorrune_essence_cnt, fp);
	save_control_block_0((struct control_block0 *)&cb0, fp);
	pw_elements_save_table((void *)el->skilltome_sub_type, sizeof(struct skilltome_sub_type), el->skilltome_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->skilltome_essence, sizeof(struct skilltome_essence), el->skilltome_essence_cnt, fp);
	pw_elements_save_table((void *)el->flysword_essence, sizeof(struct flysword_essence), el->flysword_essence_cnt, fp);
	pw_elements_save_table((void *)el->wingmanwing_essence, sizeof(struct wingmanwing_essence), el->wingmanwing_essence_cnt, fp);
	pw_elements_save_table((void *)el->townscroll_essence, sizeof(struct townscroll_essence), el->townscroll_essence_cnt, fp);
	pw_elements_save_table((void *)el->unionscroll_essence, sizeof(struct unionscroll_essence), el->unionscroll_essence_cnt, fp);
	pw_elements_save_table((void *)el->revivescroll_essence, sizeof(struct revivescroll_essence), el->revivescroll_essence_cnt, fp);
	pw_elements_save_table((void *)el->element_essence, sizeof(struct element_essence), el->element_essence_cnt, fp);
	pw_elements_save_table((void *)el->taskmatter_essence, sizeof(struct taskmatter_essence), el->taskmatter_essence_cnt, fp);
	pw_elements_save_table((void *)el->tossmatter_essence, sizeof(struct tossmatter_essence), el->tossmatter_essence_cnt, fp);
	pw_elements_save_table((void *)el->projectile_type, sizeof(struct projectile_type), el->projectile_type_cnt, fp);
	pw_elements_save_table((void *)el->projectile_essence, sizeof(struct projectile_essence), el->projectile_essence_cnt, fp);
	pw_elements_save_table((void *)el->quiver_sub_type, sizeof(struct quiver_sub_type), el->quiver_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->quiver_essence, sizeof(struct quiver_essence), el->quiver_essence_cnt, fp);
	pw_elements_save_table((void *)el->stone_sub_type, sizeof(struct stone_sub_type), el->stone_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->stone_essence, sizeof(struct stone_essence), el->stone_essence_cnt, fp);
	pw_elements_save_table((void *)el->monster_addon, sizeof(struct monster_addon), el->monster_addon_cnt, fp);
	pw_elements_save_table((void *)el->monster_type, sizeof(struct monster_type), el->monster_type_cnt, fp);
	pw_elements_save_table((void *)el->monster_essence, sizeof(struct monster_essence), el->monster_essence_cnt, fp);
	pw_elements_save_table((void *)el->npc_talk_service, sizeof(struct npc_talk_service), el->npc_talk_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_sell_service, sizeof(struct npc_sell_service), el->npc_sell_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_buy_service, sizeof(struct npc_buy_service), el->npc_buy_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_repair_service, sizeof(struct npc_repair_service), el->npc_repair_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_install_service, sizeof(struct npc_install_service), el->npc_install_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_uninstall_service, sizeof(struct npc_uninstall_service), el->npc_uninstall_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_task_in_service, sizeof(struct npc_task_in_service), el->npc_task_in_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_task_out_service, sizeof(struct npc_task_out_service), el->npc_task_out_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_task_matter_service, sizeof(struct npc_task_matter_service), el->npc_task_matter_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_skill_service, sizeof(struct npc_skill_service), el->npc_skill_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_heal_service, sizeof(struct npc_heal_service), el->npc_heal_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_transmit_service, sizeof(struct npc_transmit_service), el->npc_transmit_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_transport_service, sizeof(struct npc_transport_service), el->npc_transport_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_proxy_service, sizeof(struct npc_proxy_service), el->npc_proxy_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_storage_service, sizeof(struct npc_storage_service), el->npc_storage_service_cnt, fp);

	/* patched npc_make_service */
	fwrite(&el->npc_make_service_cnt, 1, sizeof(el->npc_make_service_cnt), fp);
	for (unsigned i = 0; i < el->npc_make_service_cnt; i++) {
		struct npc_make_service *s = &el->npc_make_service[i];
		const size_t offset = offsetof(struct npc_make_service, produce_type);
		const size_t skip = 4;
		
		fwrite(s, 1, offset, fp);
		fwrite((char *)s + offset + skip, 1, sizeof(*s) - offset - skip, fp);
	}

	pw_elements_save_table((void *)el->npc_decompose_service, sizeof(struct npc_decompose_service), el->npc_decompose_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_type, sizeof(struct npc_type), el->npc_type_cnt, fp);
	pw_elements_save_table((void *)el->npc_essence, sizeof(struct npc_essence), el->npc_essence_cnt, fp);
	pw_elements_save_talk_proc(el->talk_proc, el->talk_proc_cnt, fp);
	pw_elements_save_table((void *)el->face_texture_essence, sizeof(struct face_texture_essence), el->face_texture_essence_cnt, fp);
	pw_elements_save_table((void *)el->face_shape_essence, sizeof(struct face_shape_essence), el->face_shape_essence_cnt, fp);
	pw_elements_save_table((void *)el->face_emotion_type, sizeof(struct face_emotion_type), el->face_emotion_type_cnt, fp);
	pw_elements_save_table((void *)el->face_expression_essence, sizeof(struct face_expression_essence), el->face_expression_essence_cnt, fp);
	pw_elements_save_table((void *)el->face_hair_essence, sizeof(struct face_hair_essence), el->face_hair_essence_cnt, fp);
	pw_elements_save_table((void *)el->face_moustache_essence, sizeof(struct face_moustache_essence), el->face_moustache_essence_cnt, fp);
	pw_elements_save_table((void *)el->colorpicker_essence, sizeof(struct colorpicker_essence), el->colorpicker_essence_cnt, fp);
	pw_elements_save_table((void *)el->customizedata_essence, sizeof(struct customizedata_essence), el->customizedata_essence_cnt, fp);
	pw_elements_save_table((void *)el->recipe_major_type, sizeof(struct recipe_major_type), el->recipe_major_type_cnt, fp);
	pw_elements_save_table((void *)el->recipe_sub_type, sizeof(struct recipe_sub_type), el->recipe_sub_type_cnt, fp);

	/* patched recipe_essence */
	fwrite(&el->recipe_essence_cnt, 1, sizeof(el->recipe_essence_cnt), fp);
	for (unsigned i = 0; i < el->recipe_essence_cnt; i++) {
		struct recipe_essence *s = &el->recipe_essence[i];
		const size_t offset = offsetof(struct recipe_essence, bind_type);
		const size_t skip = 4;
		
		fwrite(s, 1, offset, fp);
		fwrite((char *)s + offset + skip, 1, sizeof(*s) - offset - skip, fp);
	}

	pw_elements_save_table((void *)el->enemy_faction_config, sizeof(struct enemy_faction_config), el->enemy_faction_config_cnt, fp);
	pw_elements_save_table((void *)el->charracter_class_config, sizeof(struct charracter_class_config), el->charracter_class_config_cnt, fp);
	pw_elements_save_table((void *)el->param_adjust_config, sizeof(struct param_adjust_config), el->param_adjust_config_cnt, fp);
	pw_elements_save_table((void *)el->player_action_info_config, sizeof(struct player_action_info_config), el->player_action_info_config_cnt, fp);
	pw_elements_save_table((void *)el->taskdice_essence, sizeof(struct taskdice_essence), el->taskdice_essence_cnt, fp);
	pw_elements_save_table((void *)el->tasknormalmatter_essence, sizeof(struct tasknormalmatter_essence), el->tasknormalmatter_essence_cnt, fp);
	pw_elements_save_table((void *)el->face_faling_essence, sizeof(struct face_faling_essence), el->face_faling_essence_cnt, fp);
	pw_elements_save_table((void *)el->player_levelexp_config, sizeof(struct player_levelexp_config), el->player_levelexp_config_cnt, fp);
	pw_elements_save_table((void *)el->mine_type, sizeof(struct mine_type), el->mine_type_cnt, fp);
	pw_elements_save_table((void *)el->mine_essence, sizeof(struct mine_essence), el->mine_essence_cnt, fp);
	pw_elements_save_table((void *)el->npc_identify_service, sizeof(struct npc_identify_service), el->npc_identify_service_cnt, fp);
	pw_elements_save_table((void *)el->fashion_major_type, sizeof(struct fashion_major_type), el->fashion_major_type_cnt, fp);
	pw_elements_save_table((void *)el->fashion_sub_type, sizeof(struct fashion_sub_type), el->fashion_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->fashion_essence, sizeof(struct fashion_essence), el->fashion_essence_cnt, fp);
	pw_elements_save_table((void *)el->faceticket_major_type, sizeof(struct faceticket_major_type), el->faceticket_major_type_cnt, fp);
	pw_elements_save_table((void *)el->faceticket_sub_type, sizeof(struct faceticket_sub_type), el->faceticket_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->faceticket_essence, sizeof(struct faceticket_essence), el->faceticket_essence_cnt, fp);
	pw_elements_save_table((void *)el->facepill_major_type, sizeof(struct facepill_major_type), el->facepill_major_type_cnt, fp);
	pw_elements_save_table((void *)el->facepill_sub_type, sizeof(struct facepill_sub_type), el->facepill_sub_type_cnt, fp);
	pw_elements_save_table((void *)el->facepill_essence, sizeof(struct facepill_essence), el->facepill_essence_cnt, fp);
	pw_elements_save_table((void *)el->suite_essence, sizeof(struct suite_essence), el->suite_essence_cnt, fp);
	pw_elements_save_table((void *)el->gm_generator_type, sizeof(struct gm_generator_type), el->gm_generator_type_cnt, fp);
	pw_elements_save_table((void *)el->gm_generator_essence, sizeof(struct gm_generator_essence), el->gm_generator_essence_cnt, fp);
	pw_elements_save_table((void *)el->pet_type, sizeof(struct pet_type), el->pet_type_cnt, fp);
	pw_elements_save_table((void *)el->pet_essence, sizeof(struct pet_essence), el->pet_essence_cnt, fp);
	pw_elements_save_table((void *)el->pet_egg_essence, sizeof(struct pet_egg_essence), el->pet_egg_essence_cnt, fp);
	pw_elements_save_table((void *)el->pet_food_essence, sizeof(struct pet_food_essence), el->pet_food_essence_cnt, fp);
	pw_elements_save_table((void *)el->pet_faceticket_essence, sizeof(struct pet_faceticket_essence), el->pet_faceticket_essence_cnt, fp);
	pw_elements_save_table((void *)el->fireworks_essence, sizeof(struct fireworks_essence), el->fireworks_essence_cnt, fp);
	pw_elements_save_table((void *)el->war_tankcallin_essence, sizeof(struct war_tankcallin_essence), el->war_tankcallin_essence_cnt, fp);
	save_control_block_1((struct control_block1 *)&cb1, fp);
	pw_elements_save_table((void *)el->npc_war_towerbuild_service, sizeof(struct npc_war_towerbuild_service), el->npc_war_towerbuild_service_cnt, fp);
	pw_elements_save_table((void *)el->player_secondlevel_config, sizeof(struct player_secondlevel_config), el->player_secondlevel_config_cnt, fp);
	pw_elements_save_table((void *)el->npc_resetprop_service, sizeof(struct npc_resetprop_service), el->npc_resetprop_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_petname_service, sizeof(struct npc_petname_service), el->npc_petname_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_petlearnskill_service, sizeof(struct npc_petlearnskill_service), el->npc_petlearnskill_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_petforgetskill_service, sizeof(struct npc_petforgetskill_service), el->npc_petforgetskill_service_cnt, fp);
	pw_elements_save_table((void *)el->skillmatter_essence, sizeof(struct skillmatter_essence), el->skillmatter_essence_cnt, fp);
	pw_elements_save_table((void *)el->refine_ticket_essence, sizeof(struct refine_ticket_essence), el->refine_ticket_essence_cnt, fp);
	pw_elements_save_table((void *)el->destroying_essence, sizeof(struct destroying_essence), el->destroying_essence_cnt, fp);
	pw_elements_save_table((void *)el->npc_equipbind_service, sizeof(struct npc_equipbind_service), el->npc_equipbind_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_equipdestroy_service, sizeof(struct npc_equipdestroy_service), el->npc_equipdestroy_service_cnt, fp);
	pw_elements_save_table((void *)el->npc_equipundestroy_service, sizeof(struct npc_equipundestroy_service), el->npc_equipundestroy_service_cnt, fp);
	pw_elements_save_table((void *)el->bible_essence, sizeof(struct bible_essence), el->bible_essence_cnt, fp);
	pw_elements_save_table((void *)el->speaker_essence, sizeof(struct speaker_essence), el->speaker_essence_cnt, fp);
	pw_elements_save_table((void *)el->autohp_essence, sizeof(struct autohp_essence), el->autohp_essence_cnt, fp);
	pw_elements_save_table((void *)el->automp_essence, sizeof(struct automp_essence), el->automp_essence_cnt, fp);
	pw_elements_save_table((void *)el->double_exp_essence, sizeof(struct double_exp_essence), el->double_exp_essence_cnt, fp);
	pw_elements_save_table((void *)el->transmitscroll_essence, sizeof(struct transmitscroll_essence), el->transmitscroll_essence_cnt, fp);
	pw_elements_save_table((void *)el->dye_ticket_essence, sizeof(struct dye_ticket_essence), el->dye_ticket_essence_cnt, fp);

	fclose(fp);
	return 0;
}

int
pw_elements_get_last_id(struct pw_elements *el)
{
	int id = 0;

	for (int i = 0; i < el->equipment_addon_cnt; i++)
		if (el->equipment_addon[i].id > id) id = el->equipment_addon[i].id;
	for (int i = 0; i < el->weapon_major_type_cnt; i++)
		if (el->weapon_major_type[i].id > id) id = el->weapon_major_type[i].id;
	for (int i = 0; i < el->weapon_sub_type_cnt; i++)
		if (el->weapon_sub_type[i].id > id) id = el->weapon_sub_type[i].id;
	for (int i = 0; i < el->weapon_essence_cnt; i++)
		if (el->weapon_essence[i].id > id) id = el->weapon_essence[i].id;
	for (int i = 0; i < el->armor_major_type_cnt; i++)
		if (el->armor_major_type[i].id > id) id = el->armor_major_type[i].id;
	for (int i = 0; i < el->armor_sub_type_cnt; i++)
		if (el->armor_sub_type[i].id > id) id = el->armor_sub_type[i].id;
	for (int i = 0; i < el->armor_essence_cnt; i++)
		if (el->armor_essence[i].id > id) id = el->armor_essence[i].id;
	for (int i = 0; i < el->decoration_major_type_cnt; i++)
		if (el->decoration_major_type[i].id > id) id = el->decoration_major_type[i].id;
	for (int i = 0; i < el->decoration_sub_type_cnt; i++)
		if (el->decoration_sub_type[i].id > id) id = el->decoration_sub_type[i].id;
	for (int i = 0; i < el->decoration_essence_cnt; i++)
		if (el->decoration_essence[i].id > id) id = el->decoration_essence[i].id;
	for (int i = 0; i < el->medicine_major_type_cnt; i++)
		if (el->medicine_major_type[i].id > id) id = el->medicine_major_type[i].id;
	for (int i = 0; i < el->medicine_sub_type_cnt; i++)
		if (el->medicine_sub_type[i].id > id) id = el->medicine_sub_type[i].id;
	for (int i = 0; i < el->medicine_essence_cnt; i++)
		if (el->medicine_essence[i].id > id) id = el->medicine_essence[i].id;
	for (int i = 0; i < el->material_major_type_cnt; i++)
		if (el->material_major_type[i].id > id) id = el->material_major_type[i].id;
	for (int i = 0; i < el->material_sub_type_cnt; i++)
		if (el->material_sub_type[i].id > id) id = el->material_sub_type[i].id;
	for (int i = 0; i < el->material_essence_cnt; i++)
		if (el->material_essence[i].id > id) id = el->material_essence[i].id;
	for (int i = 0; i < el->damagerune_sub_type_cnt; i++)
		if (el->damagerune_sub_type[i].id > id) id = el->damagerune_sub_type[i].id;
	for (int i = 0; i < el->damagerune_essence_cnt; i++)
		if (el->damagerune_essence[i].id > id) id = el->damagerune_essence[i].id;
	for (int i = 0; i < el->armorrune_sub_type_cnt; i++)
		if (el->armorrune_sub_type[i].id > id) id = el->armorrune_sub_type[i].id;
	for (int i = 0; i < el->armorrune_essence_cnt; i++)
		if (el->armorrune_essence[i].id > id) id = el->armorrune_essence[i].id;
	for (int i = 0; i < el->skilltome_sub_type_cnt; i++)
		if (el->skilltome_sub_type[i].id > id) id = el->skilltome_sub_type[i].id;
	for (int i = 0; i < el->skilltome_essence_cnt; i++)
		if (el->skilltome_essence[i].id > id) id = el->skilltome_essence[i].id;
	for (int i = 0; i < el->flysword_essence_cnt; i++)
		if (el->flysword_essence[i].id > id) id = el->flysword_essence[i].id;
	for (int i = 0; i < el->wingmanwing_essence_cnt; i++)
		if (el->wingmanwing_essence[i].id > id) id = el->wingmanwing_essence[i].id;
	for (int i = 0; i < el->townscroll_essence_cnt; i++)
		if (el->townscroll_essence[i].id > id) id = el->townscroll_essence[i].id;
	for (int i = 0; i < el->unionscroll_essence_cnt; i++)
		if (el->unionscroll_essence[i].id > id) id = el->unionscroll_essence[i].id;
	for (int i = 0; i < el->revivescroll_essence_cnt; i++)
		if (el->revivescroll_essence[i].id > id) id = el->revivescroll_essence[i].id;
	for (int i = 0; i < el->element_essence_cnt; i++)
		if (el->element_essence[i].id > id) id = el->element_essence[i].id;
	for (int i = 0; i < el->taskmatter_essence_cnt; i++)
		if (el->taskmatter_essence[i].id > id) id = el->taskmatter_essence[i].id;
	for (int i = 0; i < el->tossmatter_essence_cnt; i++)
		if (el->tossmatter_essence[i].id > id) id = el->tossmatter_essence[i].id;
	for (int i = 0; i < el->projectile_type_cnt; i++)
		if (el->projectile_type[i].id > id) id = el->projectile_type[i].id;
	for (int i = 0; i < el->projectile_essence_cnt; i++)
		if (el->projectile_essence[i].id > id) id = el->projectile_essence[i].id;
	for (int i = 0; i < el->quiver_sub_type_cnt; i++)
		if (el->quiver_sub_type[i].id > id) id = el->quiver_sub_type[i].id;
	for (int i = 0; i < el->quiver_essence_cnt; i++)
		if (el->quiver_essence[i].id > id) id = el->quiver_essence[i].id;
	for (int i = 0; i < el->stone_sub_type_cnt; i++)
		if (el->stone_sub_type[i].id > id) id = el->stone_sub_type[i].id;
	for (int i = 0; i < el->stone_essence_cnt; i++)
		if (el->stone_essence[i].id > id) id = el->stone_essence[i].id;
	for (int i = 0; i < el->monster_addon_cnt; i++)
		if (el->monster_addon[i].id > id) id = el->monster_addon[i].id;
	for (int i = 0; i < el->monster_type_cnt; i++)
		if (el->monster_type[i].id > id) id = el->monster_type[i].id;
	for (int i = 0; i < el->monster_essence_cnt; i++)
		if (el->monster_essence[i].id > id) id = el->monster_essence[i].id;
	for (int i = 0; i < el->npc_talk_service_cnt; i++)
		if (el->npc_talk_service[i].id > id) id = el->npc_talk_service[i].id;
	for (int i = 0; i < el->npc_sell_service_cnt; i++)
		if (el->npc_sell_service[i].id > id) id = el->npc_sell_service[i].id;
	for (int i = 0; i < el->npc_buy_service_cnt; i++)
		if (el->npc_buy_service[i].id > id) id = el->npc_buy_service[i].id;
	for (int i = 0; i < el->npc_repair_service_cnt; i++)
		if (el->npc_repair_service[i].id > id) id = el->npc_repair_service[i].id;
	for (int i = 0; i < el->npc_install_service_cnt; i++)
		if (el->npc_install_service[i].id > id) id = el->npc_install_service[i].id;
	for (int i = 0; i < el->npc_uninstall_service_cnt; i++)
		if (el->npc_uninstall_service[i].id > id) id = el->npc_uninstall_service[i].id;
	for (int i = 0; i < el->npc_task_in_service_cnt; i++)
		if (el->npc_task_in_service[i].id > id) id = el->npc_task_in_service[i].id;
	for (int i = 0; i < el->npc_task_out_service_cnt; i++)
		if (el->npc_task_out_service[i].id > id) id = el->npc_task_out_service[i].id;
	for (int i = 0; i < el->npc_task_matter_service_cnt; i++)
		if (el->npc_task_matter_service[i].id > id) id = el->npc_task_matter_service[i].id;
	for (int i = 0; i < el->npc_skill_service_cnt; i++)
		if (el->npc_skill_service[i].id > id) id = el->npc_skill_service[i].id;
	for (int i = 0; i < el->npc_heal_service_cnt; i++)
		if (el->npc_heal_service[i].id > id) id = el->npc_heal_service[i].id;
	for (int i = 0; i < el->npc_transmit_service_cnt; i++)
		if (el->npc_transmit_service[i].id > id) id = el->npc_transmit_service[i].id;
	for (int i = 0; i < el->npc_transport_service_cnt; i++)
		if (el->npc_transport_service[i].id > id) id = el->npc_transport_service[i].id;
	for (int i = 0; i < el->npc_proxy_service_cnt; i++)
		if (el->npc_proxy_service[i].id > id) id = el->npc_proxy_service[i].id;
	for (int i = 0; i < el->npc_storage_service_cnt; i++)
		if (el->npc_storage_service[i].id > id) id = el->npc_storage_service[i].id;
	for (int i = 0; i < el->npc_make_service_cnt; i++)
		if (el->npc_make_service[i].id > id) id = el->npc_make_service[i].id;
	for (int i = 0; i < el->npc_decompose_service_cnt; i++)
		if (el->npc_decompose_service[i].id > id) id = el->npc_decompose_service[i].id;
	for (int i = 0; i < el->npc_type_cnt; i++)
		if (el->npc_type[i].id > id) id = el->npc_type[i].id;
	for (int i = 0; i < el->npc_essence_cnt; i++)
		if (el->npc_essence[i].id > id) id = el->npc_essence[i].id;
	for (int i = 0; i < el->talk_proc_cnt; i++)
		if (el->talk_proc[i].id > id) id = el->talk_proc[i].id;
	for (int i = 0; i < el->face_texture_essence_cnt; i++)
		if (el->face_texture_essence[i].id > id) id = el->face_texture_essence[i].id;
	for (int i = 0; i < el->face_shape_essence_cnt; i++)
		if (el->face_shape_essence[i].id > id) id = el->face_shape_essence[i].id;
	for (int i = 0; i < el->face_emotion_type_cnt; i++)
		if (el->face_emotion_type[i].id > id) id = el->face_emotion_type[i].id;
	for (int i = 0; i < el->face_expression_essence_cnt; i++)
		if (el->face_expression_essence[i].id > id) id = el->face_expression_essence[i].id;
	for (int i = 0; i < el->face_hair_essence_cnt; i++)
		if (el->face_hair_essence[i].id > id) id = el->face_hair_essence[i].id;
	for (int i = 0; i < el->face_moustache_essence_cnt; i++)
		if (el->face_moustache_essence[i].id > id) id = el->face_moustache_essence[i].id;
	for (int i = 0; i < el->colorpicker_essence_cnt; i++)
		if (el->colorpicker_essence[i].id > id) id = el->colorpicker_essence[i].id;
	for (int i = 0; i < el->customizedata_essence_cnt; i++)
		if (el->customizedata_essence[i].id > id) id = el->customizedata_essence[i].id;
	for (int i = 0; i < el->recipe_major_type_cnt; i++)
		if (el->recipe_major_type[i].id > id) id = el->recipe_major_type[i].id;
	for (int i = 0; i < el->recipe_sub_type_cnt; i++)
		if (el->recipe_sub_type[i].id > id) id = el->recipe_sub_type[i].id;
	for (int i = 0; i < el->recipe_essence_cnt; i++)
		if (el->recipe_essence[i].id > id) id = el->recipe_essence[i].id;
	for (int i = 0; i < el->enemy_faction_config_cnt; i++)
		if (el->enemy_faction_config[i].id > id) id = el->enemy_faction_config[i].id;
	for (int i = 0; i < el->charracter_class_config_cnt; i++)
		if (el->charracter_class_config[i].id > id) id = el->charracter_class_config[i].id;
	for (int i = 0; i < el->param_adjust_config_cnt; i++)
		if (el->param_adjust_config[i].id > id) id = el->param_adjust_config[i].id;
	for (int i = 0; i < el->player_action_info_config_cnt; i++)
		if (el->player_action_info_config[i].id > id) id = el->player_action_info_config[i].id;
	for (int i = 0; i < el->taskdice_essence_cnt; i++)
		if (el->taskdice_essence[i].id > id) id = el->taskdice_essence[i].id;
	for (int i = 0; i < el->tasknormalmatter_essence_cnt; i++)
		if (el->tasknormalmatter_essence[i].id > id) id = el->tasknormalmatter_essence[i].id;
	for (int i = 0; i < el->face_faling_essence_cnt; i++)
		if (el->face_faling_essence[i].id > id) id = el->face_faling_essence[i].id;
	for (int i = 0; i < el->player_levelexp_config_cnt; i++)
		if (el->player_levelexp_config[i].id > id) id = el->player_levelexp_config[i].id;
	for (int i = 0; i < el->mine_type_cnt; i++)
		if (el->mine_type[i].id > id) id = el->mine_type[i].id;
	for (int i = 0; i < el->mine_essence_cnt; i++)
		if (el->mine_essence[i].id > id) id = el->mine_essence[i].id;
	for (int i = 0; i < el->npc_identify_service_cnt; i++)
		if (el->npc_identify_service[i].id > id) id = el->npc_identify_service[i].id;
	for (int i = 0; i < el->fashion_major_type_cnt; i++)
		if (el->fashion_major_type[i].id > id) id = el->fashion_major_type[i].id;
	for (int i = 0; i < el->fashion_sub_type_cnt; i++)
		if (el->fashion_sub_type[i].id > id) id = el->fashion_sub_type[i].id;
	for (int i = 0; i < el->fashion_essence_cnt; i++)
		if (el->fashion_essence[i].id > id) id = el->fashion_essence[i].id;
	for (int i = 0; i < el->faceticket_major_type_cnt; i++)
		if (el->faceticket_major_type[i].id > id) id = el->faceticket_major_type[i].id;
	for (int i = 0; i < el->faceticket_sub_type_cnt; i++)
		if (el->faceticket_sub_type[i].id > id) id = el->faceticket_sub_type[i].id;
	for (int i = 0; i < el->faceticket_essence_cnt; i++)
		if (el->faceticket_essence[i].id > id) id = el->faceticket_essence[i].id;
	for (int i = 0; i < el->facepill_major_type_cnt; i++)
		if (el->facepill_major_type[i].id > id) id = el->facepill_major_type[i].id;
	for (int i = 0; i < el->facepill_sub_type_cnt; i++)
		if (el->facepill_sub_type[i].id > id) id = el->facepill_sub_type[i].id;
	for (int i = 0; i < el->facepill_essence_cnt; i++)
		if (el->facepill_essence[i].id > id) id = el->facepill_essence[i].id;
	for (int i = 0; i < el->suite_essence_cnt; i++)
		if (el->suite_essence[i].id > id) id = el->suite_essence[i].id;
	for (int i = 0; i < el->gm_generator_type_cnt; i++)
		if (el->gm_generator_type[i].id > id) id = el->gm_generator_type[i].id;
	for (int i = 0; i < el->gm_generator_essence_cnt; i++)
		if (el->gm_generator_essence[i].id > id) id = el->gm_generator_essence[i].id;
	for (int i = 0; i < el->pet_type_cnt; i++)
		if (el->pet_type[i].id > id) id = el->pet_type[i].id;
	for (int i = 0; i < el->pet_essence_cnt; i++)
		if (el->pet_essence[i].id > id) id = el->pet_essence[i].id;
	for (int i = 0; i < el->pet_egg_essence_cnt; i++)
		if (el->pet_egg_essence[i].id > id) id = el->pet_egg_essence[i].id;
	for (int i = 0; i < el->pet_food_essence_cnt; i++)
		if (el->pet_food_essence[i].id > id) id = el->pet_food_essence[i].id;
	for (int i = 0; i < el->pet_faceticket_essence_cnt; i++)
		if (el->pet_faceticket_essence[i].id > id) id = el->pet_faceticket_essence[i].id;
	for (int i = 0; i < el->fireworks_essence_cnt; i++)
		if (el->fireworks_essence[i].id > id) id = el->fireworks_essence[i].id;
	for (int i = 0; i < el->war_tankcallin_essence_cnt; i++)
		if (el->war_tankcallin_essence[i].id > id) id = el->war_tankcallin_essence[i].id;
	for (int i = 0; i < el->npc_war_towerbuild_service_cnt; i++)
		if (el->npc_war_towerbuild_service[i].id > id) id = el->npc_war_towerbuild_service[i].id;
	for (int i = 0; i < el->player_secondlevel_config_cnt; i++)
		if (el->player_secondlevel_config[i].id > id) id = el->player_secondlevel_config[i].id;
	for (int i = 0; i < el->npc_resetprop_service_cnt; i++)
		if (el->npc_resetprop_service[i].id > id) id = el->npc_resetprop_service[i].id;
	for (int i = 0; i < el->npc_petname_service_cnt; i++)
		if (el->npc_petname_service[i].id > id) id = el->npc_petname_service[i].id;
	for (int i = 0; i < el->npc_petlearnskill_service_cnt; i++)
		if (el->npc_petlearnskill_service[i].id > id) id = el->npc_petlearnskill_service[i].id;
	for (int i = 0; i < el->npc_petforgetskill_service_cnt; i++)
		if (el->npc_petforgetskill_service[i].id > id) id = el->npc_petforgetskill_service[i].id;
	for (int i = 0; i < el->skillmatter_essence_cnt; i++)
		if (el->skillmatter_essence[i].id > id) id = el->skillmatter_essence[i].id;
	for (int i = 0; i < el->refine_ticket_essence_cnt; i++)
		if (el->refine_ticket_essence[i].id > id) id = el->refine_ticket_essence[i].id;
	for (int i = 0; i < el->destroying_essence_cnt; i++)
		if (el->destroying_essence[i].id > id) id = el->destroying_essence[i].id;
	for (int i = 0; i < el->npc_equipbind_service_cnt; i++)
		if (el->npc_equipbind_service[i].id > id) id = el->npc_equipbind_service[i].id;
	for (int i = 0; i < el->npc_equipdestroy_service_cnt; i++)
		if (el->npc_equipdestroy_service[i].id > id) id = el->npc_equipdestroy_service[i].id;
	for (int i = 0; i < el->npc_equipundestroy_service_cnt; i++)
		if (el->npc_equipundestroy_service[i].id > id) id = el->npc_equipundestroy_service[i].id;
	for (int i = 0; i < el->bible_essence_cnt; i++)
		if (el->bible_essence[i].id > id) id = el->bible_essence[i].id;
	for (int i = 0; i < el->speaker_essence_cnt; i++)
		if (el->speaker_essence[i].id > id) id = el->speaker_essence[i].id;
	for (int i = 0; i < el->autohp_essence_cnt; i++)
		if (el->autohp_essence[i].id > id) id = el->autohp_essence[i].id;
	for (int i = 0; i < el->automp_essence_cnt; i++)
		if (el->automp_essence[i].id > id) id = el->automp_essence[i].id;
	for (int i = 0; i < el->double_exp_essence_cnt; i++)
		if (el->double_exp_essence[i].id > id) id = el->double_exp_essence[i].id;
	for (int i = 0; i < el->transmitscroll_essence_cnt; i++)
		if (el->transmitscroll_essence[i].id > id) id = el->transmitscroll_essence[i].id;
	for (int i = 0; i < el->dye_ticket_essence_cnt; i++)
		if (el->dye_ticket_essence[i].id > id) id = el->dye_ticket_essence[i].id;

	return id;
}
