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

uint32_t g_elements_last_id;
struct pw_idmap *g_elements_map;

static void *
add_element(struct pw_elements_table *table)
{
	struct pw_elements_chain *chain = table->chain_last;

	if (chain->count < chain->capacity) {
		return &chain->data[chain->count++ * table->el_size];
	}

	size_t table_count = 16;
	chain->next = table->chain_last = calloc(1, sizeof(struct pw_elements_chain) + table_count * table->el_size);
	chain = table->chain_last;
	chain->capacity = table_count;
	chain->count = 1;

	return chain->data;
}

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

static struct serializer mines_serializer[] = {
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

static struct serializer monsters_serializer[] = {
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

static struct serializer recipes_serializer[] = {
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

static struct serializer npc_sells_serializer[] = {
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

static struct serializer npcs_serializer[] = {
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

static struct serializer npc_crafts_serializer[] = {
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

static struct serializer weapon_major_types_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer weapon_minor_types_serializer[] = {
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

static struct serializer armor_major_types_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer armor_minor_types_serializer[] = {
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

static struct serializer decoration_major_types_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer decoration_minor_types_serializer[] = {
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

static struct serializer medicine_major_types_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer medicine_minor_types_serializer[] = {
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

static struct serializer projectile_types_serializer[] = {
	{ "id", INT32 },
	{ "name", WSTRING(32) },
	{ "", TYPE_END },
};

static struct serializer projectile_essence_serializer[] = {
	{ "id", CUSTOM, serialize_item_id_fn },
	{ "type", CONST_INT(16) },
	{ "projectile_types", INT32 },
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

static struct serializer stone_types_serializer[] = {
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

static struct serializer armor_sets_serializer[] = {
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

static struct serializer damagerune_sub_type_serializer[] = { { "", TYPE_END } };
static struct serializer armorrune_sub_type_serializer[] = { { "", TYPE_END } };
static struct serializer skilltome_sub_type_serializer[] = { { "", TYPE_END } };
static struct serializer unionscroll_essence_serializer[] = { { "", TYPE_END } };
static struct serializer monster_addon_serializer[] = { { "", TYPE_END } };
static struct serializer monster_type_serializer[] = { { "", TYPE_END } };
static struct serializer npc_talk_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_buy_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_repair_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_install_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_uninstall_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_task_in_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_task_out_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_task_matter_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_skill_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_heal_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_transmit_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_transport_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_proxy_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_storage_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_decompose_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_type_serializer[] = { { "", TYPE_END } };
static struct serializer face_texture_essence_serializer[] = { { "", TYPE_END } };
static struct serializer face_shape_essence_serializer[] = { { "", TYPE_END } };
static struct serializer face_emotion_type_serializer[] = { { "", TYPE_END } };
static struct serializer face_expression_essence_serializer[] = { { "", TYPE_END } };
static struct serializer face_hair_essence_serializer[] = { { "", TYPE_END } };
static struct serializer face_moustache_essence_serializer[] = { { "", TYPE_END } };
static struct serializer colorpicker_essence_serializer[] = { { "", TYPE_END } };
static struct serializer customizedata_essence_serializer[] = { { "", TYPE_END } };
static struct serializer recipe_major_type_serializer[] = { { "", TYPE_END } };
static struct serializer recipe_sub_type_serializer[] = { { "", TYPE_END } };
static struct serializer enemy_faction_config_serializer[] = { { "", TYPE_END } };
static struct serializer charracter_class_config_serializer[] = { { "", TYPE_END } };
static struct serializer param_adjust_config_serializer[] = { { "", TYPE_END } };
static struct serializer player_action_info_config_serializer[] = { { "", TYPE_END } };
static struct serializer face_faling_essence_serializer[] = { { "", TYPE_END } };
static struct serializer player_levelexp_config_serializer[] = { { "", TYPE_END } };
static struct serializer mine_type_serializer[] = { { "", TYPE_END } };
static struct serializer npc_identify_service_serializer[] = { { "", TYPE_END } };
static struct serializer fashion_major_type_serializer[] = { { "", TYPE_END } };
static struct serializer fashion_sub_type_serializer[] = { { "", TYPE_END } };
static struct serializer faceticket_major_type_serializer[] = { { "", TYPE_END } };
static struct serializer faceticket_sub_type_serializer[] = { { "", TYPE_END } };
static struct serializer facepill_major_type_serializer[] = { { "", TYPE_END } };
static struct serializer facepill_sub_type_serializer[] = { { "", TYPE_END } };
static struct serializer gm_generator_type_serializer[] = { { "", TYPE_END } };
static struct serializer pet_type_serializer[] = { { "", TYPE_END } };
static struct serializer pet_essence_serializer[] = { { "", TYPE_END } };
static struct serializer npc_war_towerbuild_service_serializer[] = { { "", TYPE_END } };
static struct serializer player_secondlevel_config_serializer[] = { { "", TYPE_END } };
static struct serializer npc_resetprop_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_petname_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_petlearnskill_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_petforgetskill_service_serializer[] = { { "", TYPE_END } };
static struct serializer destroying_essence_serializer[] = { { "", TYPE_END } };
static struct serializer npc_equipbind_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_equipdestroy_service_serializer[] = { { "", TYPE_END } };
static struct serializer npc_equipundestroy_service_serializer[] = { { "", TYPE_END } };

static struct pw_elements_table *
get_table(struct pw_elements *elements, const char *name)
{
	size_t i;

	for (i = 0; i < elements->tables_count; i++) {
		struct pw_elements_table *table = elements->tables[i];
		
		if (strcmp(table->name, name) == 0) {
			return table;
		}
	}

	return NULL;
}

#define EXPORT_TABLE(elements, table_name, filename) \
({ \
	FILE *fp = fopen(filename, "wb"); \
	struct pw_elements_table *table = get_table(elements, #table_name); \
	long sz; \
\
	if (fp == NULL) { \
		fprintf(stderr, "cant open %s for writing\n", filename); \
	} else { \
		sz = serialize(fp, table->serializer, \
				(void *)table->chain->data, table->chain->count); \
		fclose(fp); \
		truncate(filename, sz); \
	} \
})

void
pw_elements_serialize(struct pw_elements *elements)
{
	EXPORT_TABLE(elements, mines, "mines.json");
	EXPORT_TABLE(elements, monsters, "monsters.json");
	EXPORT_TABLE(elements, recipes, "recipes.json");
	EXPORT_TABLE(elements, npcs, "npcs.json");
	EXPORT_TABLE(elements, npc_sells, "npc_sells.json");
	EXPORT_TABLE(elements, npc_crafts, "npc_crafts.json");

	EXPORT_TABLE(elements, weapon_major_types, "weapon_major_typess.json");
	EXPORT_TABLE(elements, weapon_minor_types, "weapon_minor_types.json");
	EXPORT_TABLE(elements, armor_major_types, "armor_major_typess.json");
	EXPORT_TABLE(elements, armor_minor_types, "armor_minor_types.json");
	EXPORT_TABLE(elements, decoration_major_types, "decoration_major_typess.json");
	EXPORT_TABLE(elements, decoration_minor_types, "decoration_minor_types.json");

	EXPORT_TABLE(elements, medicine_major_types, "medicine_major_typess.json");
	EXPORT_TABLE(elements, medicine_minor_types, "medicine_minor_types.json");
	EXPORT_TABLE(elements, material_major_type, "material_major_types.json");
	EXPORT_TABLE(elements, material_sub_type, "material_minor_types.json");

	EXPORT_TABLE(elements, projectile_types, "projectile_typess.json");
	EXPORT_TABLE(elements, quiver_sub_type, "quiver_typess.json");
	EXPORT_TABLE(elements, stone_types, "stone_types.json");

	EXPORT_TABLE(elements, armor_sets, "armor_sets.json");
	EXPORT_TABLE(elements, equipment_addon, "equipment_addon.json");

	FILE *fp = fopen("items.json", "wb");
	if (fp == NULL) {
		fprintf(stderr, "cant open items.json for writing\n");
		return;
	}
	size_t prev_sz, sz = 0;

#define STR(x, y) #x #y
#define EXPORT_ITEMS(table_name) \
({ \
	struct pw_elements_table *table = get_table(elements, STR(table_name, _essence)); \
\
	prev_sz = sz; \
	sz = serialize(fp, table->serializer, \
			(void *)table->chain->data, table->chain->count); \
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

int
pw_elements_patch_obj(struct pw_elements *elements, struct cjson *obj)
{
	struct pw_elements_table *table;
	void **table_el;
	const char *obj_type;
	int64_t id;
	int i;

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

	for (i = 0; i < elements->tables_count; i++) {
		table = elements->tables[i];
		if (strcmp(table->name, obj_type) == 0) {
			break;
		}
	}

	if (i == elements->tables_count) {
		pwlog(LOG_ERROR, "pw_elements_get_table() failed\n");
		return -1;
	}

	table_el = pw_idmap_get(g_elements_map, id, table);
	if (!table_el) {
		uint32_t el_id = g_elements_last_id++;

		table_el = add_element(table);
		*(uint32_t *)table_el = el_id;
		pw_idmap_set(g_elements_map, id, table, table_el);
		return -1;
	}
	deserialize(obj, table->serializer, table_el);
	return 0;
}

static void
pw_elements_load_table(struct pw_elements *elements, struct pw_elements_table **table_p, const char *name, uint32_t el_size, struct serializer *serializer, FILE *fp)
{
	struct pw_elements_table *table;
	struct pw_elements_chain *chain;
	int32_t i, count;
	void *el;

	*table_p = table = calloc(1, sizeof(*table));
	if (!table) {
		pwlog(LOG_ERROR, "pw_elements_load_table: calloc() failed\n");
		return;
	}

	table->el_size = el_size;
	table->name = name;
	table->serializer = serializer;

	fread(&count, 1, sizeof(count), fp);
	table->chain = table->chain_last = chain = calloc(1, sizeof(struct pw_elements_chain) + count * el_size);
	if (!chain) {
		pwlog(LOG_ERROR, "pw_elements_load_table: calloc() failed\n");
		return;
	}
	chain->count = chain->capacity = count;
	fread(chain->data, 1, count * el_size, fp);

	el = chain->data;
	for (i = 0; i < count; i++) {
		unsigned id = *(uint32_t *)el;

		if (id > g_elements_last_id) {
			g_elements_last_id = id;
		}

		pw_idmap_set(g_elements_map, id, table, el);
		el += el_size;
	}

	elements->tables[elements->tables_count++] = table;
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
pw_elements_load_talk_proc(struct pw_elements *elements, FILE *fp)
{
	void *table;
	int32_t count;

	fread(&count, 1, sizeof(count), fp);
	elements->talk_proc = table = calloc(count, sizeof(struct talk_proc));
	elements->talk_proc_cnt = count;

	for (int i = 0; i < count; ++i) {
		struct talk_proc *talk = table + i * sizeof(struct talk_proc);

		pw_elements_read_talk_proc(talk, fp);
	}

	return count;
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

	g_elements_map = pw_idmap_init();
	if (!g_elements_map) {
		fprintf(stderr, "pw_idmap_init() failed\n");
		fclose(fp);
		return 1;
	}

	memset(el, 0, sizeof(*el));

	fread(&el->hdr, 1, sizeof(el->hdr), fp);
	if (el->hdr.version != 12) {
		fprintf(stderr, "element version mismatch, expected 12, found %d\n", el->hdr.version);
		fclose(fp);
		return 1;
	}

#define LOAD_ARR(arr_name) \
	pw_elements_load_table(el, &el->arr_name, #arr_name, sizeof(struct arr_name), arr_name ## _serializer, fp)

	LOAD_ARR(equipment_addon);
	LOAD_ARR(weapon_major_types);
	LOAD_ARR(weapon_minor_types);
	LOAD_ARR(weapon_essence);
	LOAD_ARR(armor_major_types);
	LOAD_ARR(armor_minor_types);
	LOAD_ARR(armor_essence);
	LOAD_ARR(decoration_major_types);
	LOAD_ARR(decoration_minor_types);
	LOAD_ARR(decoration_essence);
	LOAD_ARR(medicine_major_types);
	LOAD_ARR(medicine_minor_types);
	LOAD_ARR(medicine_essence);
	LOAD_ARR(material_major_type);
	LOAD_ARR(material_sub_type);
	LOAD_ARR(material_essence);
	LOAD_ARR(damagerune_sub_type);
	LOAD_ARR(damagerune_essence);
	LOAD_ARR(armorrune_sub_type);
	LOAD_ARR(armorrune_essence);
	load_control_block_0(&el->control_block0, fp);
	LOAD_ARR(skilltome_sub_type);
	LOAD_ARR(skilltome_essence);
	LOAD_ARR(flysword_essence);
	LOAD_ARR(wingmanwing_essence);
	LOAD_ARR(townscroll_essence);
	LOAD_ARR(unionscroll_essence);
	LOAD_ARR(revivescroll_essence);
	LOAD_ARR(element_essence);
	LOAD_ARR(taskmatter_essence);
	LOAD_ARR(tossmatter_essence);
	LOAD_ARR(projectile_types);
	LOAD_ARR(projectile_essence);
	LOAD_ARR(quiver_sub_type);
	LOAD_ARR(quiver_essence);
	LOAD_ARR(stone_types);
	LOAD_ARR(stone_essence);
	LOAD_ARR(monster_addon);
	LOAD_ARR(monster_type);
	LOAD_ARR(monsters);
	LOAD_ARR(npc_talk_service);
	LOAD_ARR(npc_sells);
	LOAD_ARR(npc_buy_service);
	LOAD_ARR(npc_repair_service);
	LOAD_ARR(npc_install_service);
	LOAD_ARR(npc_uninstall_service);
	LOAD_ARR(npc_task_in_service);
	LOAD_ARR(npc_task_out_service);
	LOAD_ARR(npc_task_matter_service);
	LOAD_ARR(npc_skill_service);
	LOAD_ARR(npc_heal_service);
	LOAD_ARR(npc_transmit_service);
	LOAD_ARR(npc_transport_service);
	LOAD_ARR(npc_proxy_service);
	LOAD_ARR(npc_storage_service);
	LOAD_ARR(npc_crafts);
	LOAD_ARR(npc_decompose_service);
	LOAD_ARR(npc_type);
	LOAD_ARR(npcs);
	pw_elements_load_talk_proc(el, fp);
	LOAD_ARR(face_texture_essence);
	LOAD_ARR(face_shape_essence);
	LOAD_ARR(face_emotion_type);
	LOAD_ARR(face_expression_essence);
	LOAD_ARR(face_hair_essence);
	LOAD_ARR(face_moustache_essence);
	LOAD_ARR(colorpicker_essence);
	LOAD_ARR(customizedata_essence);
	LOAD_ARR(recipe_major_type);
	LOAD_ARR(recipe_sub_type);
	LOAD_ARR(recipes);
	LOAD_ARR(enemy_faction_config);
	LOAD_ARR(charracter_class_config);
	LOAD_ARR(param_adjust_config);
	LOAD_ARR(player_action_info_config);
	LOAD_ARR(taskdice_essence);
	LOAD_ARR(tasknormalmatter_essence);
	LOAD_ARR(face_faling_essence);
	LOAD_ARR(player_levelexp_config);
	LOAD_ARR(mine_type);
	LOAD_ARR(mines);
	LOAD_ARR(npc_identify_service);
	LOAD_ARR(fashion_major_type);
	LOAD_ARR(fashion_sub_type);
	LOAD_ARR(fashion_essence);
	LOAD_ARR(faceticket_major_type);
	LOAD_ARR(faceticket_sub_type);
	LOAD_ARR(faceticket_essence);
	LOAD_ARR(facepill_major_type);
	LOAD_ARR(facepill_sub_type);
	LOAD_ARR(facepill_essence);
	LOAD_ARR(armor_sets);
	LOAD_ARR(gm_generator_type);
	LOAD_ARR(gm_generator_essence);
	LOAD_ARR(pet_type);
	LOAD_ARR(pet_essence);
	LOAD_ARR(pet_egg_essence);
	LOAD_ARR(pet_food_essence);
	LOAD_ARR(pet_faceticket_essence);
	LOAD_ARR(fireworks_essence);
	LOAD_ARR(war_tankcallin_essence);
	load_control_block_1(&el->control_block1, fp);
	LOAD_ARR(npc_war_towerbuild_service);
	LOAD_ARR(player_secondlevel_config);
	LOAD_ARR(npc_resetprop_service);
	LOAD_ARR(npc_petname_service);
	LOAD_ARR(npc_petlearnskill_service);
	LOAD_ARR(npc_petforgetskill_service);
	LOAD_ARR(skillmatter_essence);
	LOAD_ARR(refine_ticket_essence);
	LOAD_ARR(destroying_essence);
	LOAD_ARR(npc_equipbind_service);
	LOAD_ARR(npc_equipdestroy_service);
	LOAD_ARR(npc_equipundestroy_service);
	LOAD_ARR(bible_essence);
	LOAD_ARR(speaker_essence);
	LOAD_ARR(autohp_essence);
	LOAD_ARR(automp_essence);
	LOAD_ARR(double_exp_essence);
	LOAD_ARR(transmitscroll_essence);
	LOAD_ARR(dye_ticket_essence);

#undef LOAD_ARR

	fclose(fp);

	rc = load_icons();
	rc = rc || load_colors();
	rc = rc || load_descriptions();
	return rc;
}

