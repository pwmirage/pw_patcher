/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>

#include "common.h"
#include "cjson_ext.h"
#include "pw_elements.h"

static char g_icon_names[4096 / 32 * 2048 / 32 + 1][64] = {};
static char g_item_colors[65536] = {};
static char *g_item_descs[65536] = {};

static int
load_icons(void)
{
	FILE *fp = fopen("iconlist_ivtrm.txt", "r");
	char *line = NULL;
	size_t len = 64;
	ssize_t read;
	char *tmp = calloc(1, len);

	if (fp == NULL) {
		fprintf(stderr, "Can't open iconlist_ivtrm.txt\n");
		return -1;
	}

	/* skip header */
	for (int i = 0; i < 4; i++) {
		getline(&line, &len, fp);
	}

	unsigned i = 0;
	while ((read = getline(&tmp, &len, fp)) != -1) {
		sprint(g_icon_names[i], 64, tmp, len);
		i++;
	}

	fprintf(stderr, "Parsed %u icons\n", i);
	fclose(fp);
	return 0;
}

static int
load_colors(void)
{
	FILE *fp = fopen("item_color.txt", "r");
	size_t len = 64;
	ssize_t read;
	char *line= calloc(1, len);

	if (fp == NULL) {
		fprintf(stderr, "Can't open item_color.txt\n");
		return -1;
	}

	unsigned i = 0;
	while ((read = getline(&line, &len, fp)) != -1) {
		char *id, *color;

		id = strtok(line, "\t");
		if (id == NULL || strlen(id) == 0) {
			break;
		}

		color = strtok(NULL, "\t");
		g_item_colors[atoi(id)] = (char)atoi(color);

		i++;
	}

	fprintf(stderr, "Parsed %u item colors\n", i);
	fclose(fp);
	return 0;
}

static int
load_descriptions(void)
{
	FILE *fp = fopen("item_ext_desc.txt", "r");
	size_t len = 0;
	ssize_t read;
	char *line = NULL;

	if (fp == NULL) {
		fprintf(stderr, "Can't open item_ext_desc.txt\n");
		return -1;
	}

	/* skip header */
	for (int i = 0; i < 5; i++) {
		getline(&line, &len, fp);
	}

	unsigned i = 0;

	while ((read = getline(&line, &len, fp)) != -1) {
		char *id, *txt;

		id = strtok(line, " ");
		if (id == NULL || strlen(id) == 0) {
			break;
		}

		txt = strtok(NULL, "\"");
		txt = strtok(NULL, "\"");
		g_item_descs[atoi(id)] = txt;
		/* FIXME: line is technically leaked */
		line = NULL;
		len = 0;
		i++;
	}

	fprintf(stderr, "Parsed %u item descriptions\n", i);
	fclose(fp);
	return 0;
}


static size_t
icon_serialize_fn(FILE *fp, void *data)
{
	unsigned len = 128;
	char out[1024] = {};
	char *tmp = out, *basename = out;
	int i;

	sprint(out, sizeof(out), data, len);
	while (*tmp) {
		if (*tmp == '\\') {
			basename = tmp + 1;
		}
		tmp++;
	}

	for (i = 0; i < sizeof(g_icon_names) / sizeof(g_icon_names[0]); i++) {
		if (g_icon_names[i] == NULL) {
			i = -1;
			break;
		}

		if (memcmp(basename, g_icon_names[i], strlen(basename) - 3) == 0) {
			break;
		}
	}

	fprintf(fp, "\"icon\":%d,", i);
	return 128;
}

static size_t
float_or_int_fn(FILE *fp, void *data)
{
	uint32_t u = *(uint32_t *)data;

	if (u > 10240) {
		fprintf(fp, "%.8f,", *(float *)&u);
	} else {
		fprintf(fp, "%u,", u);
	}

	return 4;
}

static size_t
serialize_item_id_fn(FILE *fp, void *data)
{
	uint32_t id = *(uint32_t *)data;
	uint32_t color;
	char *desc;

	fprintf(fp, "\"id\":%u,", id);
	color = g_item_colors[id];
	if (color) {
		fprintf(fp, "\"color\":%u,", color);
	}

	desc = g_item_descs[id];
	if (desc) {
		fprintf(fp, "\"desc\":\"%s\",", desc);
	}

	return 4;
}

static struct serializer equipment_addon_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "num_params", INT32 },
	{ "params", ARRAY_START(3) },
        { "", CUSTOM, float_or_int_fn },
	{ "", ARRAY_END },
	{ "", TYPE_END },
};

static struct serializer mine_essence_serializer[] = {
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
	{ "file_model", STRING(128) },
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

static struct serializer monster_essence_serializer[] = {
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

static struct serializer recipe_essence_serializer[] = {
	{ "id", INT32 },
	{ "major_type", INT32 },
	{ "minor_type", INT32 },
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
	{ "duration", FLOAT },
	{ "xp", INT32 },
	{ "sp", INT32 },
	{ "mats", ARRAY_START(32) },
		{ "id", INT32 },
		{ "num", INT32 },
	{ "", ARRAY_END },
	{ "", TYPE_END },
};

static struct serializer npc_sell_service_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "pages", ARRAY_START(8) },
		{ "title", WSTRING(8) },
		{ "item_id", ARRAY_START(32) },
			{ "", INT32 },
		{ "", ARRAY_END },
	{ "", ARRAY_END },
	{ "_id_dialog", INT32 },
	{ "", TYPE_END },
};

static struct serializer npc_essence_serializer[] = {
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

static struct serializer npc_make_service_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "make_skill_id", INT32 },
	{ "_produce_type", INT32 },
	{ "pages", ARRAY_START(8) },
		{ "title", WSTRING(8) },
		{ "recipe_id", ARRAY_START(32) },
			{ "", INT32 },
		{ "", ARRAY_END },
	{ "", ARRAY_END },
	{ "", TYPE_END },
};

static struct serializer weapon_major_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer weapon_sub_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "file_hitgfx", STRING(128) },
	{ "file_hitsfx", STRING(128) },
	{ "probability_fastest", FLOAT },
	{ "probability_fast", FLOAT },
	{ "probability_normal", FLOAT },
	{ "probability_slow", FLOAT },
	{ "probability_slowest", FLOAT },
	{ "attack_speed", FLOAT },
	{ "attack_short_range", FLOAT },
	{ "action_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer weapon_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(1) },
	{ "major_type", INT32 },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "_require_projectile", INT32 }, /* 1 whenever major_type == 13 (ranged) */
	{ "file_model_right", STRING(128) },
	{ "file_model_left", STRING(128) },
	{ "file_matter", STRING(128) },
	{ "file_icon", CUSTOM, icon_serialize_fn },
	{ "require_strength", INT32 },
	{ "require_dexterity", INT32 },
	{ "require_magic", INT32 },
	{ "require_vitality", INT32 },
	{ "character_combo_id", INT32 },
	{ "require_level", INT32 },
	{ "level", INT32 },
	{ "fixed_props", INT32 },
	{ "damage_low", INT32 },
	{ "damage_high", INT32 },
	{ "_damage_high_max", INT32 }, /* must == damage_high, otherwise won't spawn */
	{ "magic_damage_low", INT32 },
	{ "magic_damage_high", INT32 },
	{ "_magic_damage_high_max", INT32 }, /* must == magic_damage_high, otherwise won't spawn */
	{ "attack_range", FLOAT },
	{ "_is_ranged", INT32 }, /* 1 whenever major_type == 13 (ranged) */
	{ "durability_min", INT32 },
	{ "durability_max", INT32 },
	{ "levelup_addon", INT32 },
	{ "mirages_to_refine", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "repairfee", INT32 },
	{ "drop_socket_prob", ARRAY_START(3) },
		{ "", FLOAT },
	{ "", ARRAY_END },
	{ "make_socket_prob", ARRAY_START(3) },
		{ "", FLOAT },
	{ "", ARRAY_END },
	{ "addon_num_prob", ARRAY_START(4) },
		{ "", FLOAT },
	{ "", ARRAY_END },
	{ "probability_unique", FLOAT },
	{ "addons", ARRAY_START(32) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "rands", ARRAY_START(32) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "uniques", ARRAY_START(16) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "durability_drop_min", INT32 },
	{ "durability_drop_max", INT32 },
	{ "decompose_price", INT32 },
	{ "decompose_time", INT32 },
	{ "element_id", INT32 },
	{ "element_num", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer armor_major_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer armor_sub_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "equip_mask", INT32 },
	{ "", TYPE_END },
};

static struct serializer armor_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(2) },
	{ "major_type", INT32 },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "realname", STRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "equip_location", INT32 },
	{ "level", INT32 },
	{ "require_strength", INT32 },
	{ "require_agility", INT32 },
	{ "require_energy", INT32 },
	{ "require_tili", INT32 },
	{ "character_combo_id", INT32 },
	{ "require_level", INT32 },
	{ "fixed_props", INT32 },
	{ "defence_low", INT32 },
	{ "defence_high", INT32 },
	{ "magic_def", ARRAY_START(5) },
		{ "low", INT32 },
		{ "high", INT32 },
	{ "", ARRAY_END },
	{ "mp_enhance_low", INT32 },
	{ "mp_enhance_high", INT32 },
	{ "hp_enhance_low", INT32 },
	{ "hp_enhance_high", INT32 },
	{ "armor_enhance_low", INT32 },
	{ "armor_enhance_high", INT32 },
	{ "durability_min", INT32 },
	{ "durability_max", INT32 },
	{ "levelup_addon", INT32 },
	{ "material_need", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "repairfee", INT32 },
	{ "drop_socket_prob", ARRAY_START(5) },
		{ "", FLOAT },
	{ "", ARRAY_END },
	{ "make_socket_prob", ARRAY_START(5) },
		{ "", FLOAT },
	{ "", ARRAY_END },
	{ "addon_num_prob", ARRAY_START(4) },
		{ "", FLOAT },
	{ "", ARRAY_END },
	{ "addons", ARRAY_START(32) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "rands", ARRAY_START(32) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "durability_drop_min", INT32 },
	{ "durability_drop_max", INT32 },
	{ "decompose_price", INT32 },
	{ "decompose_time", INT32 },
	{ "element_id", INT32 },
	{ "element_num", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer decoration_major_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer decoration_sub_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "equip_mask", INT32 },
	{ "", TYPE_END },
};

static struct serializer decoration_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(3) },
	{ "major_type", INT32 },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_model", STRING(128) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "level", INT32 },
	{ "require_strength", INT32 },
	{ "require_agility", INT32 },
	{ "require_energy", INT32 },
	{ "require_tili", INT32 },
	{ "character_combo_id", INT32 },
	{ "require_level", INT32 },
	{ "fixed_props", INT32 },
	{ "damage_low", INT32 },
	{ "damage_high", INT32 },
	{ "magic_damage_low", INT32 },
	{ "magic_damage_high", INT32 },
	{ "phys_def_low", INT32 },
	{ "phys_def_high", INT32 },
	{ "magic_def", ARRAY_START(5) },
		{ "low", INT32 },
		{ "high", INT32 },
	{ "", ARRAY_END },
	{ "armor_enhance_low", INT32 },
	{ "armor_enhance_high", INT32 },
	{ "durability_min", INT32 },
	{ "durability_max", INT32 },
	{ "levelup_addon", INT32 },
	{ "material_need", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "repairfee", INT32 },
	{ "addon_num_prob", ARRAY_START(4) },
		{ "", FLOAT },
	{ "", ARRAY_END },
	{ "addons", ARRAY_START(32) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "addons", ARRAY_END },
	{ "rands", ARRAY_START(32) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "durability_drop_min", INT32 },
	{ "durability_drop_max", INT32 },
	{ "decompose_price", INT32 },
	{ "decompose_time", INT32 },
	{ "element_id", INT32 },
	{ "element_num", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer medicine_major_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer medicine_sub_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer medicine_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(4) },
	{ "major_type", INT32 },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "require_level", INT32 },
	{ "cool_time", INT32 },
	{ "hp_add_total", INT32 },
	{ "hp_add_time", INT32 },
	{ "mp_add_total", INT32 },
	{ "mp_add_time", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer material_major_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer material_sub_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer material_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(5) },
	{ "major_type", INT32 },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "decompose_price", INT32 },
	{ "decompose_time", INT32 },
	{ "element_id", INT32 },
	{ "element_num", INT32 },
	{ "stack_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer damagerune_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(6) },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "is_magic", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "require_weapon_level_min", INT32 },
	{ "require_weapon_level_max", INT32 },
	{ "damage_increased", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer armorrune_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(7) },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "file_gfx", STRING(128) },
	{ "file_sfx", STRING(128) },
	{ "is_magic", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "require_player_level_min", INT32 },
	{ "require_player_level_max", INT32 },
	{ "damage_reduce_percent", FLOAT },
	{ "damage_reduce_time", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer skilltome_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(8) },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer flysword_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(9) },
	{ "name", WSTRING(32) },
	{ "file_model", STRING(128) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "level", INT32 },
	{ "require_player_level_min", INT32 },
	{ "speed_increase_min", FLOAT },
	{ "speed_increase_max", FLOAT },
	{ "speed_rush_increase_min", FLOAT },
	{ "speed_rush_increase_max", FLOAT },
	{ "time_max_min", FLOAT },
	{ "time_max_max", FLOAT },
	{ "time_increase_per_element", FLOAT },
	{ "fly_mode", INT32 },
	{ "character_combo_id", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer wingmanwing_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(10) },
	{ "name", WSTRING(32) },
	{ "file_model", STRING(128) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "require_player_level_min", INT32 },
	{ "speed_increase", FLOAT },
	{ "mp_launch", INT32 },
	{ "mp_per_second", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer townscroll_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(11) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "use_time", FLOAT },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer revivescroll_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(12) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "use_time", FLOAT },
	{ "cool_time", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer element_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(13) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "level", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer taskmatter_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(14) },
	{ "name", WSTRING(32) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer tossmatter_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(15) },
	{ "name", WSTRING(32) },
	{ "file_model", STRING(128) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "file_firegfx", STRING(128) },
	{ "file_hitgfx", STRING(128) },
	{ "file_hitsfx", STRING(128) },
	{ "require_strength", INT32 },
	{ "require_agility", INT32 },
	{ "require_level", INT32 },
	{ "damage_low", INT32 },
	{ "damage_high_min", INT32 },
	{ "damage_high_max", INT32 },
	{ "use_time", FLOAT },
	{ "attack_range", FLOAT },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer projectile_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer projectile_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(16) },
	{ "projectile_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_model", STRING(128) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "file_firegfx", STRING(128) },
	{ "file_hitgfx", STRING(128) },
	{ "file_hitsfx", STRING(128) },
	{ "require_weapon_level_min", INT32 },
	{ "require_weapon_level_max", INT32 },
	{ "damage_enhance", INT32 },
	{ "damage_scale_enhance", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "addon_ids", ARRAY_START(4) },
		{ "", INT32 },
	{ "", ARRAY_END },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer quiver_sub_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer quiver_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(17) },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "id_projectile", INT32 },
	{ "num_min", INT32 },
	{ "num_max", INT32 },
	{ "", TYPE_END },
};

static struct serializer stone_sub_type_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer stone_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(18) },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "level", INT32 },
	{ "color", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "install_price", INT32 },
	{ "uninstall_price", INT32 },
	{ "id_addon_damage", INT32 },
	{ "id_addon_defence", INT32 },
	{ "weapon_desc", WSTRING(16) },
	{ "armor_desc", WSTRING(16) },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer taskdice_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(19) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "tasks", ARRAY_START(8) },
		{ "id", INT32 },
		{ "prob", FLOAT },
	{ "", ARRAY_END },
	{ "use_on_pick", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
};

static struct serializer tasknormalmatter_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(20) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer fashion_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(21) },
	{ "major_type", INT32 },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "realname", STRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "equip_location", INT32 },
	{ "level", INT32 },
	{ "require_level", INT32 },
	{ "require_dye_count", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "gender", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer faceticket_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(22) },
	{ "major_type", INT32 },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "require_level", INT32 },
	{ "bound_file", STRING(128) },
	{ "unsymmetrical", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer facepill_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(23) },
	{ "major_type", INT32 },
	{ "minor_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "duration", INT32 },
	{ "camera_scale", FLOAT },
	{ "character_combo_id", INT32 },
	{ "pllfiles", ARRAY_START(16) },
		{ "", STRING(128) },
	{ "", ARRAY_END },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer gm_generator_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(24) },
	{ "id_type", INT32 },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "id_object", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer pet_egg_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(25) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "id_pet", INT32 },
	{ "money_hatched", INT32 },
	{ "money_restored", INT32 },
	{ "honor_point", INT32 },
	{ "level", INT32 },
	{ "exp", INT32 },
	{ "skill_point", INT32 },
	{ "skills", ARRAY_START(32) },
		{ "id", INT32 },
		{ "level", INT32 },
	{ "", ARRAY_END },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer pet_food_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(26) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "level", INT32 },
	{ "hornor", INT32 },
	{ "exp", INT32 },
	{ "food_type", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer pet_faceticket_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(27) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer fireworks_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(28) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "file_fw", STRING(128) },
	{ "level", INT32 },
	{ "time_to_fire", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer war_tankcallin_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(29) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer skillmatter_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(30) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "level_required", INT32 },
	{ "id_skill", INT32 },
	{ "level_skill", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer refine_ticket_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(31) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "desc", WSTRING(16) },
	{ "ext_reserved_prob", FLOAT },
	{ "ext_succeed_prob", FLOAT },
	{ "fail_reserve_level", INT32 },
	{ "fail_ext_succeed_probs", ARRAY_START(12) },
		{ "", FLOAT },
	{ "", ARRAY_END },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer bible_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(32) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "addon_ids", ARRAY_START(10) },
		{ "", INT32 },
	{ "", ARRAY_END },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer speaker_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(33) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "id_icon_set", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer autohp_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(34) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "total_hp", INT32 },
	{ "trigger_amount", FLOAT },
	{ "cool_time", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer automp_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(35) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "total_mp", INT32 },
	{ "trigger_amount", FLOAT },
	{ "cool_time", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer double_exp_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(36) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "double_exp_time", INT32 },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer transmitscroll_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(37) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer dye_ticket_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(38) },
	{ "name", WSTRING(32) },
	{ "file_matter", STRING(128) },
	{ "icon", CUSTOM, icon_serialize_fn },
	{ "h_min", FLOAT },
	{ "h_max", FLOAT },
	{ "s_min", FLOAT },
	{ "s_max", FLOAT },
	{ "v_min", FLOAT },
	{ "v_max", FLOAT },
	{ "price", INT32 },
	{ "shop_price", INT32 },
	{ "pile_num_max", INT32 },
	{ "has_guid", INT32 },
	{ "proc_type", INT32 },
	{ "", TYPE_END },
};

static struct serializer suite_essence_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "max_equips", INT32 },
	{ "equip_ids", ARRAY_START(12) },
		{ "", INT32 },
	{ "", ARRAY_END },
	{ "addon_ids", ARRAY_START(11) },
		{ "", INT32 },
	{ "", ARRAY_END },
	{ "file_gfx", STRING(128) },
	{ "", TYPE_END },
};

#define EXPORT_TABLE(elements, table, filename) \
({ \
	FILE *fp = fopen(filename, "wb"); \
	long sz; \
\
	if (fp == NULL) { \
		fprintf(stderr, "cant open %s for writing\n", filename); \
	} else { \
		sz = serialize(fp, table ## _serializer, \
				(void *)(elements)->table, (elements)->table ## _cnt); \
		fclose(fp); \
		truncate(filename, sz); \
	} \
})

void
pw_elements_serialize(struct pw_elements *elements)
{
	EXPORT_TABLE(elements, mine_essence, "mines.json");
	EXPORT_TABLE(elements, monster_essence, "monsters.json");
	EXPORT_TABLE(elements, recipe_essence, "recipes.json");
	EXPORT_TABLE(elements, npc_essence, "npcs.json");
	EXPORT_TABLE(elements, npc_sell_service, "npc_sells.json");
	EXPORT_TABLE(elements, npc_make_service, "npc_crafts.json");

	EXPORT_TABLE(elements, weapon_major_type, "weapon_major_types.json");
	EXPORT_TABLE(elements, weapon_sub_type, "weapon_minor_types.json");
	EXPORT_TABLE(elements, armor_major_type, "armor_major_types.json");
	EXPORT_TABLE(elements, armor_sub_type, "armor_minor_types.json");
	EXPORT_TABLE(elements, decoration_major_type, "decoration_major_types.json");
	EXPORT_TABLE(elements, decoration_sub_type, "decoration_minor_types.json");

	EXPORT_TABLE(elements, medicine_major_type, "medicine_major_types.json");
	EXPORT_TABLE(elements, medicine_sub_type, "medicine_minor_types.json");
	EXPORT_TABLE(elements, material_major_type, "material_major_types.json");
	EXPORT_TABLE(elements, material_sub_type, "material_minor_types.json");

	EXPORT_TABLE(elements, projectile_type, "projectile_types.json");
	EXPORT_TABLE(elements, quiver_sub_type, "quiver_types.json");
	EXPORT_TABLE(elements, stone_sub_type, "stone_types.json");

	EXPORT_TABLE(elements, suite_essence, "armor_sets.json");
	EXPORT_TABLE(elements, equipment_addon, "equipment_addon.json");

	FILE *fp = fopen("items.json", "wb");
	if (fp == NULL) {
		fprintf(stderr, "cant open items.json for writing\n");
		return;
	}
	size_t prev_sz, sz = 0;

#define EXPORT_ITEMS(table) \
({ \
	prev_sz = sz; \
	sz = serialize(fp, table ## _essence_serializer, \
			(void *)(elements)->table ## _essence, (elements)->table ## _essence_cnt); \
	if (prev_sz > 0) { \
		fseek(fp, prev_sz - 1, SEEK_SET); \
		fprintf(fp, ",\n"); /* overwrite ] and [ */ \
		fseek(fp, sz, SEEK_SET); \
	} \
})

	EXPORT_ITEMS(weapon);
	EXPORT_ITEMS(armor);
	EXPORT_ITEMS(decoration);
	EXPORT_ITEMS(medicine);
	EXPORT_ITEMS(material);
	EXPORT_ITEMS(damagerune);
	EXPORT_ITEMS(armorrune);
	EXPORT_ITEMS(skilltome);
	EXPORT_ITEMS(flysword);
	EXPORT_ITEMS(wingmanwing);
	EXPORT_ITEMS(townscroll);
	EXPORT_ITEMS(revivescroll);
	EXPORT_ITEMS(element);
	EXPORT_ITEMS(taskmatter);
	EXPORT_ITEMS(tossmatter);
	EXPORT_ITEMS(projectile);
	EXPORT_ITEMS(quiver);
	EXPORT_ITEMS(stone);
	EXPORT_ITEMS(taskdice);
	EXPORT_ITEMS(tasknormalmatter);
	EXPORT_ITEMS(fashion);
	EXPORT_ITEMS(faceticket);
	EXPORT_ITEMS(facepill);
	EXPORT_ITEMS(gm_generator);
	EXPORT_ITEMS(pet_egg);
	EXPORT_ITEMS(pet_food);
	EXPORT_ITEMS(pet_faceticket);
	EXPORT_ITEMS(fireworks);
	EXPORT_ITEMS(war_tankcallin);
	EXPORT_ITEMS(skillmatter);
	EXPORT_ITEMS(refine_ticket);
	EXPORT_ITEMS(bible);
	EXPORT_ITEMS(speaker);
	EXPORT_ITEMS(autohp);
	EXPORT_ITEMS(automp);
	EXPORT_ITEMS(double_exp);
	EXPORT_ITEMS(transmitscroll);
	EXPORT_ITEMS(dye_ticket);
	EXPORT_ITEMS(flysword);

#undef EXPORT_ITEMS

	fclose(fp);
	truncate("items.json", sz);
}

static int
pw_elements_get_table(struct pw_elements *elements, const char *name, void **table, size_t *tbl_el_size, int32_t *tbl_size, struct serializer **serializer)
{

#define MAP(tbl_name, table_id) \
	if (strcmp(tbl_name, name) == 0) { \
		*serializer = table_id ## _serializer; \
		*tbl_size = (elements)->table_id ## _cnt; \
		*tbl_el_size = sizeof(*(elements)->table_id); \
		*table = (elements)->table_id; \
		return 0; \
	}

	MAP("mines", mine_essence);
	MAP("monsters", monster_essence);
	MAP("recipes", recipe_essence);
	MAP("npcs", npc_essence);
	MAP("npc_sells", npc_sell_service);
	MAP("npc_crafts", npc_make_service);

	MAP("weapon_major_types", weapon_major_type);
	MAP("weapon_minor_types", weapon_sub_type);
	MAP("armor_major_types", armor_major_type);
	MAP("armor_minor_types", armor_sub_type);
	MAP("decoration_major_types", decoration_major_type);
	MAP("decoration_minor_types", decoration_sub_type);

	MAP("medicine_major_types", medicine_major_type);
	MAP("medicine_minor_types", medicine_sub_type);
	MAP("material_major_types", material_major_type);
	MAP("material_minor_types", material_sub_type);

	MAP("projectile_types", projectile_type);
	MAP("quiver_types", quiver_sub_type);
	MAP("stone_types", stone_sub_type);

	MAP("armor_sets", suite_essence);
	MAP("equipment_addon", equipment_addon);

#undef MAP
	return -1;
}

int
pw_elements_patch_obj(struct pw_elements *elements, struct cjson *obj)
{
	void *table, *table_el;
	size_t table_el_size;
	int32_t table_size;
	struct serializer *serializer;
	const char *obj_type;
	int32_t i;
	int64_t id;
	int rc;

	obj_type = JSs(obj, "_db", "type");
	if (!obj_type) {
		pwlog(LOG_ERROR, "missing obj._db.type\n");
		return -1;
	}

	id = JSi(obj, "id");
	if (!id) {
		pwlog(LOG_ERROR, "missing obj.id\n");
		return -1;
	}

	rc = pw_elements_get_table(elements, obj_type, &table, &table_el_size, &table_size, &serializer);
	if (rc) {
		pwlog(LOG_ERROR, "pw_elements_get_table() failed: %d\n", rc);
		return -1;
	}

	/* TODO: handle new objects and custom IDs */
	table_el = table;
	for (i = 0; i < table_size; i++) {
		/* id is always the first field */
		int32_t obj_id = *(int32_t *)table_el;

		if (obj_id == id) {
			break;
		}

		table_el += table_el_size;
	}

	if (i == table_size) {
		pwlog(LOG_INFO, "patched element not in array (id=%"PRId64")\n", id);
		return -1;
	}

	deserialize(obj, serializer, table_el);
	return 0;
}

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
	int rc;

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

	rc = load_icons();
	rc = rc || load_colors();
	rc = rc || load_descriptions();
	return rc;
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
