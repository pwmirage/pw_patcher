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
#include <assert.h>

#include "common.h"
#include "cjson_ext.h"
#include "serializer.h"
#include "chain_arr.h"
#include "idmap.h"
#include "pw_elements.h"
#include "avl.h"
#include "pw_item_desc.h"

char g_icon_names[PW_ELEMENTS_ICON_COUNT][128];
char g_item_colors[65536] = {};
char *g_item_descs[65536] = {};

struct pw_idmap *g_elements_map;
int g_elements_taskmatter_idmap_id;
int g_elements_npc_idmap_id;
int g_elements_recipes_idmap_id;
extern struct pw_idmap *g_tasks_map;

static int
memcmp_ic(const char *s1, const char *s2, size_t nbytes)
{
	char c1, c2;

	while (nbytes > 0) {
		c1 = *s1;
		c2 = *s2;

		if (c1 >= 'A' && c1 <= 'Z') {
			c1 += 'a' - 'A';
		}
		if (c2 >= 'A' && c2 <= 'Z') {
			c2 += 'a' - 'A';
		}

		int ret = c1 - c2;
		if (ret != 0) {
			return ret;
		}

		s1++;
		s2++;
		nbytes--;
	}
	return 0;
}

static size_t
icon_serialize_fn(FILE *fp, struct serializer *f, void *data)
{
	unsigned len = 128;
	char out[1024] = {};
	char *tmp = out, *basename = out;
	int i;

	if (fp == g_nullfile) {
		return 128;
	}

	sprint(out, sizeof(out), data, len);
	while (*tmp) {
		if (*tmp == '\\') {
			basename = tmp + 1;
		}
		tmp++;
	}

	for (i = 0; i < PW_ELEMENTS_ICON_COUNT; i++) {
		if (memcmp_ic(basename, g_icon_names[i], strlen(basename) - 3) == 0) {
			break;
		}
	}

	if (i == PW_ELEMENTS_ICON_COUNT) {
		return 128;
	}

	fprintf(fp, "\"icon\":%d,", i);
	return 128;
}

static size_t
icon_deserialize_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	int64_t val = JSi(f);

	if (f->type == CJSON_TYPE_NONE || !g_icon_names[0]) {
		return 128;
	}

	if (val < PW_ELEMENTS_ICON_COUNT) {
		memcpy(data, g_icon_names[val], 128);
	}

	return 128;
}

static size_t
float_or_int_fn(FILE *fp, struct serializer *f, void *data)
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
serialize_item_id_fn(FILE *fp, struct serializer *f, void *data)
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

static size_t
deserialize_item_id_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	/* do nothing */
	return 4;
}

static void
deserialize_id_field_async_fn(struct pw_idmap_el *node, void *target_data)
{
	PWLOG(LOG_INFO, "patching (prev:%u, new: %u)\n", *(uint32_t *)target_data, node->id);
	*(uint32_t *)target_data = node->id;
}

static size_t
deserialize_elements_id_field_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	int64_t val = JSi(f);

	if (f->type == CJSON_TYPE_NONE) {
		return 4;
	}

	if (val >= 0x80000000) {
		int rc = pw_idmap_get_async(g_elements_map, val, 0, deserialize_id_field_async_fn, data);

		if (rc) {
			assert(false);
		}
	} else {
		deserialize_log(f, data);
		*(uint32_t *)(data) = (uint32_t)val;
	}

	return 4;
}

static size_t
serialize_elements_id_field_fn(FILE *fp, struct serializer *f, void *data)
{
	/* TODO */
	return 4;
}

static struct serializer equipment_addon_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "num_params", _INT32 },
	{ "params", _ARRAY_START(3) },
		{ "", _CUSTOM, float_or_int_fn },
	{ "", _ARRAY_END },
	{ "", _TYPE_END },
};

static struct serializer mines_serializer[] = {
	{ "id", _INT32 },
	{ "id_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "level", _INT32 },
	{ "level_required", _INT32 },
	{ "item_required", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "use_up_tool", _INT32 },
	{ "time_min", _INT32 },
	{ "time_max", _INT32 },
	{ "exp", _INT32 },
	{ "sp", _INT32 },
	{ "file_model", _STRING(128) },
	{ "mat_item", _ARRAY_START(16) },
		{ "id", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "mat_count", _ARRAY_START(2) },
		{ "num", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "task_in", _INT32 }, /* TODO */
	{ "task_out", _INT32 },
	{ "uninterruptible", _INT32 },
	{ "npcgen", _ARRAY_START(4) },
		{ "mob_id", _INT32 }, /* TODO */
		{ "num", _INT32 },
		{ "radius", _FLOAT },
	{ "", _ARRAY_END },
	{ "aggro_monster_faction", _INT32 },
	{ "aggro_radius", _FLOAT },
	{ "aggro_num", _INT32 },
	{ "permanent", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer monsters_serializer[] = {
	{ "id", _INT32 },
	{ "id_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "prop", _WSTRING(16) },
	{ "desc", _WSTRING(16) },
	{ "faction", _INT32 },
	{ "monster_faction", _INT32 },
	{ "file_model", _STRING(128) },
	{ "file_gfx_short", _STRING(128) },
	{ "file_gfx_short_hit", _STRING(128) },
	{ "size", _FLOAT },
	{ "damage_delay", _FLOAT },
	{ "id_strategy", _INT32 },
	{ "role_in_war", _INT32 },
	{ "level", _INT32 },
	{ "show_level", _INT32 },
	{ "id_pet_egg", _INT32 }, /* FIXME */
	{ "hp", _INT32 },
	{ "phys_def", _INT32 },
	{ "magic_def", _ARRAY_START(5) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "immune_type", _INT32 },
	{ "exp", _INT32 },
	{ "sp", _INT32 },
	{ "money_average", _INT32 },
	{ "money_var", _INT32 },
	{ "short_range_mode", _INT32 },
	{ "sight_range", _INT32 },
	{ "attack", _INT32 },
	{ "armor", _INT32 },
	{ "damage_min", _INT32 },
	{ "damage_max", _INT32 },
	{ "magic_damage_ext", _ARRAY_START(5) },
		{ "min", _INT32 },
		{ "max", _INT32 },
	{ "", _ARRAY_END },
	{ "attack_range", _FLOAT },
	{ "attack_speed", _FLOAT },
	{ "magic_damage_min", _INT32 },
	{ "magic_damage_max", _INT32 },
	{ "skill_id", _INT32 },
	{ "skill_level", _INT32 },
	{ "hp_regenerate", _INT32 },
	{ "is_aggressive", _INT32 },
	{ "monster_faction_ask_help", _INT32 },
	{ "monster_faction_can_help", _INT32 },
	{ "aggro_range", _FLOAT },
	{ "aggro_time", _FLOAT },
	{ "inhabit_type", _INT32 },
	{ "patrol_mode", _INT32 },
	{ "stand_mode", _INT32 },
	{ "walk_speed", _FLOAT },
	{ "run_speed", _FLOAT },
	{ "fly_speed", _FLOAT },
	{ "swim_speed", _FLOAT },
	{ "common_strategy", _INT32 },
	{ "aggro_strategy", _ARRAY_START(4) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "hp_autoskill", _ARRAY_START(3) },
		{ "", _ARRAY_START(5) },
			{ "id", _INT32 },
			{ "level", _INT32 },
			{ "prob", _FLOAT },
		{ "", _ARRAY_END },
	{ "", _ARRAY_END },
	{ "skill_on_death", _INT32 },
	{ "skill", _ARRAY_START(32) },
		{ "id", _INT32 },
		{ "level", _INT32 },
	{ "", _ARRAY_END },
	{ "probability_drop_num", _ARRAY_START(4) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "drop_times", _INT32 },
	{ "drop_protected", _INT32 },
	{ "drop_matter", _ARRAY_START(32) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "", _TYPE_END },
};

__attribute__((packed)) struct recipes {
	uint32_t id;
	uint32_t major_type;
	uint32_t minor_type;
	char16_t name[32];
	uint32_t recipe_level;
	uint32_t skill_id;
	uint32_t skill_level;
	uint32_t _bind_type;
	struct recipes_targets {
		uint32_t id;
		float prob;
	} targets[4];
	float fail_prob;
	uint32_t num_to_make;
	uint32_t coins;
	float duration;
	uint32_t xp;
	uint32_t sp;
	struct recipes_mats {
		uint32_t id;
		uint32_t num;
	} mats[32];
};
	
static struct serializer recipes_serializer[] = {
	{ "id", _INT32 },
	{ "major_type", _INT32 },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "recipe_level", _INT32 },
	{ "skill_id", _INT32 },
	{ "skill_level", _INT32 },
	{ "_bind_type", _INT32 },
	{ "targets", _ARRAY_START(4) },
		{ "id", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "fail_prob", _FLOAT },
	{ "num_to_make", _INT32 },
	{ "coins", _INT32 },
	{ "duration", _FLOAT },
	{ "xp", _INT32 },
	{ "sp", _INT32 },
	{ "mats", _ARRAY_START(32) },
		{ "id", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
		{ "num", _INT32 },
	{ "", _ARRAY_END },
	{ "", _TYPE_END },
};

static struct serializer npc_sells_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "pages", _ARRAY_START(8) },
		{ "title", _WSTRING(8) },
		{ "item_id", _ARRAY_START(32) },
			{ "", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
		{ "", _ARRAY_END },
	{ "", _ARRAY_END },
	{ "_id_dialog", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer npcs_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "id_type", _INT32 },
	{ "refresh_time", _FLOAT },
	{ "attack_rule", _INT32 },
	{ "file_model", _STRING(128) },
	{ "tax_rate", _FLOAT },
	{ "base_monster_id", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "greeting", _WSTRING(256) },
	{ "target_id", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "domain_related", _INT32 },
	{ "id_talk_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_sell_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_buy_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_repair_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_install_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_uninstall_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_task_out_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_task_in_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_task_matter_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_skill_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_heal_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_transmit_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_transport_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_proxy_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_storage_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_make_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_decompose_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_identify_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_war_towerbuild_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_resetprop_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_petname_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_petlearnskill_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_petforgetskill_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_equipbind_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_equipdestroy_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "id_equipundestroy_service", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "combined_services", _INT32 },
	{ "id_mine", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "", _TYPE_END },
};

__attribute__((packed)) struct npc_crafts {
	uint32_t id;
	char16_t name[32];
	uint32_t make_skill_id;
	uint32_t _produce_type;
	struct npc_crafts_page {
		char16_t title[8];
		uint32_t recipe_id[32];
	} pages[8];
};

static struct serializer npc_crafts_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "make_skill_id", _INT32 },
	{ "_produce_type", _INT32 },
	{ "pages", _ARRAY_START(8) },
		{ "title", _WSTRING(8) },
		{ "recipe_id", _ARRAY_START(32) },
			{ "", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
		{ "", _ARRAY_END },
	{ "", _ARRAY_END },
	{ "", _TYPE_END },
};

static struct serializer weapon_major_types_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "", _TYPE_END },
};

static struct serializer weapon_minor_types_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_hitgfx", _STRING(128) },
	{ "file_hitsfx", _STRING(128) },
	{ "probability_fastest", _FLOAT },
	{ "probability_fast", _FLOAT },
	{ "probability_normal", _FLOAT },
	{ "probability_slow", _FLOAT },
	{ "probability_slowest", _FLOAT },
	{ "attack_speed", _FLOAT },
	{ "attack_short_range", _FLOAT },
	{ "action_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer weapon_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(1) },
	{ "major_type", _INT32 },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "_require_projectile", _INT32 }, /* 1 whenever major_type == 13 (ranged) */
	{ "file_model_right", _STRING(128) },
	{ "file_model_left", _STRING(128) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "require_strength", _INT32 },
	{ "require_dexterity", _INT32 },
	{ "require_magic", _INT32 },
	{ "require_vitality", _INT32 },
	{ "character_combo_id", _INT32 },
	{ "require_level", _INT32 },
	{ "level", _INT32 },
	{ "fixed_props", _INT32 },
	{ "damage_low", _INT32 },
	{ "damage_high", _INT32 },
	{ "_damage_high_max", _INT32 }, /* must == damage_high, otherwise won't spawn */
	{ "magic_damage_low", _INT32 },
	{ "magic_damage_high", _INT32 },
	{ "_magic_damage_high_max", _INT32 }, /* must == magic_damage_high, otherwise won't spawn */
	{ "attack_range", _FLOAT },
	{ "_is_ranged", _INT32 }, /* 1 whenever major_type == 13 (ranged) */
	{ "durability_min", _INT32 },
	{ "durability_max", _INT32 },
	{ "levelup_addon", _INT32 },
	{ "mirages_to_refine", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "repairfee", _INT32 },
	{ "drop_socket_prob", _ARRAY_START(3) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "make_socket_prob", _ARRAY_START(3) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "addon_num_prob", _ARRAY_START(4) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "probability_unique", _FLOAT },
	{ "addons", _ARRAY_START(32) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "rands", _ARRAY_START(32) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "uniques", _ARRAY_START(16) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "durability_drop_min", _INT32 },
	{ "durability_drop_max", _INT32 },
	{ "decompose_price", _INT32 },
	{ "decompose_time", _INT32 },
	{ "element_id", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "element_num", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer armor_major_types_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "", _TYPE_END },
};

static struct serializer armor_minor_types_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "equip_mask", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer armor_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(2) },
	{ "major_type", _INT32 },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "realname", _STRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "equip_location", _INT32 },
	{ "level", _INT32 },
	{ "require_strength", _INT32 },
	{ "require_agility", _INT32 },
	{ "require_energy", _INT32 },
	{ "require_tili", _INT32 },
	{ "character_combo_id", _INT32 },
	{ "require_level", _INT32 },
	{ "fixed_props", _INT32 },
	{ "defence_low", _INT32 },
	{ "defence_high", _INT32 },
	{ "magic_def", _ARRAY_START(5) },
		{ "low", _INT32 },
		{ "high", _INT32 },
	{ "", _ARRAY_END },
	{ "mp_enhance_low", _INT32 },
	{ "mp_enhance_high", _INT32 },
	{ "hp_enhance_low", _INT32 },
	{ "hp_enhance_high", _INT32 },
	{ "armor_enhance_low", _INT32 },
	{ "armor_enhance_high", _INT32 },
	{ "durability_min", _INT32 },
	{ "durability_max", _INT32 },
	{ "levelup_addon", _INT32 },
	{ "material_need", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "repairfee", _INT32 },
	{ "drop_socket_prob", _ARRAY_START(5) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "make_socket_prob", _ARRAY_START(5) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "addon_num_prob", _ARRAY_START(4) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "addons", _ARRAY_START(32) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "rands", _ARRAY_START(32) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "durability_drop_min", _INT32 },
	{ "durability_drop_max", _INT32 },
	{ "decompose_price", _INT32 },
	{ "decompose_time", _INT32 },
	{ "element_id", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "element_num", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer decoration_major_types_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "", _TYPE_END },
};

static struct serializer decoration_minor_types_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "equip_mask", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer decoration_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(3) },
	{ "major_type", _INT32 },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_model", _STRING(128) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "level", _INT32 },
	{ "require_strength", _INT32 },
	{ "require_agility", _INT32 },
	{ "require_energy", _INT32 },
	{ "require_tili", _INT32 },
	{ "character_combo_id", _INT32 },
	{ "require_level", _INT32 },
	{ "fixed_props", _INT32 },
	{ "damage_low", _INT32 },
	{ "damage_high", _INT32 },
	{ "magic_damage_low", _INT32 },
	{ "magic_damage_high", _INT32 },
	{ "phys_def_low", _INT32 },
	{ "phys_def_high", _INT32 },
	{ "magic_def", _ARRAY_START(5) },
		{ "low", _INT32 },
		{ "high", _INT32 },
	{ "", _ARRAY_END },
	{ "armor_enhance_low", _INT32 },
	{ "armor_enhance_high", _INT32 },
	{ "durability_min", _INT32 },
	{ "durability_max", _INT32 },
	{ "levelup_addon", _INT32 },
	{ "material_need", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "repairfee", _INT32 },
	{ "addon_num_prob", _ARRAY_START(4) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "addons", _ARRAY_START(32) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "addons", _ARRAY_END },
	{ "rands", _ARRAY_START(32) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "durability_drop_min", _INT32 },
	{ "durability_drop_max", _INT32 },
	{ "decompose_price", _INT32 },
	{ "decompose_time", _INT32 },
	{ "element_id", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "element_num", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer medicine_major_types_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "", _TYPE_END },
};

static struct serializer medicine_minor_types_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "", _TYPE_END },
};

static struct serializer medicine_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(4) },
	{ "major_type", _INT32 },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "require_level", _INT32 },
	{ "cool_time", _INT32 },
	{ "hp_add_total", _INT32 },
	{ "hp_add_time", _INT32 },
	{ "mp_add_total", _INT32 },
	{ "mp_add_time", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer material_major_type_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "", _TYPE_END },
};

static struct serializer material_sub_type_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "", _TYPE_END },
};

static struct serializer material_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(5) },
	{ "major_type", _INT32 },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "decompose_price", _INT32 },
	{ "decompose_time", _INT32 },
	{ "element_id", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "element_num", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer damagerune_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(6) },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "is_magic", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "require_weapon_level_min", _INT32 },
	{ "require_weapon_level_max", _INT32 },
	{ "damage_increased", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer armorrune_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(7) },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "file_gfx", _STRING(128) },
	{ "file_sfx", _STRING(128) },
	{ "is_magic", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "require_player_level_min", _INT32 },
	{ "require_player_level_max", _INT32 },
	{ "damage_reduce_percent", _FLOAT },
	{ "damage_reduce_time", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer skilltome_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(8) },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer flysword_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(9) },
	{ "name", _WSTRING(32) },
	{ "file_model", _STRING(128) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "level", _INT32 },
	{ "require_player_level_min", _INT32 },
	{ "speed_increase_min", _FLOAT },
	{ "speed_increase_max", _FLOAT },
	{ "speed_rush_increase_min", _FLOAT },
	{ "speed_rush_increase_max", _FLOAT },
	{ "time_max_min", _FLOAT },
	{ "time_max_max", _FLOAT },
	{ "time_increase_per_element", _FLOAT },
	{ "fly_mode", _INT32 },
	{ "character_combo_id", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer wingmanwing_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(10) },
	{ "name", _WSTRING(32) },
	{ "file_model", _STRING(128) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "require_player_level_min", _INT32 },
	{ "speed_increase", _FLOAT },
	{ "mp_launch", _INT32 },
	{ "mp_per_second", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer townscroll_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(11) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "use_time", _FLOAT },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer revivescroll_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(12) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "use_time", _FLOAT },
	{ "cool_time", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer element_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(13) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "level", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer taskmatter_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(14) },
	{ "name", _WSTRING(32) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer tossmatter_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(15) },
	{ "name", _WSTRING(32) },
	{ "file_model", _STRING(128) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "file_firegfx", _STRING(128) },
	{ "file_hitgfx", _STRING(128) },
	{ "file_hitsfx", _STRING(128) },
	{ "require_strength", _INT32 },
	{ "require_agility", _INT32 },
	{ "require_level", _INT32 },
	{ "damage_low", _INT32 },
	{ "damage_high_min", _INT32 },
	{ "damage_high_max", _INT32 },
	{ "use_time", _FLOAT },
	{ "attack_range", _FLOAT },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer projectile_types_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "", _TYPE_END },
};

static struct serializer projectile_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(16) },
	{ "projectile_types", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_model", _STRING(128) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "file_firegfx", _STRING(128) },
	{ "file_hitgfx", _STRING(128) },
	{ "file_hitsfx", _STRING(128) },
	{ "require_weapon_level_min", _INT32 },
	{ "require_weapon_level_max", _INT32 },
	{ "damage_enhance", _INT32 },
	{ "damage_scale_enhance", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "addon_ids", _ARRAY_START(4) },
		{ "", _INT32 },
	{ "", _ARRAY_END },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer quiver_sub_type_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "", _TYPE_END },
};

static struct serializer quiver_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(17) },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "id_projectile", _INT32 },
	{ "num_min", _INT32 },
	{ "num_max", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer stone_types_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "", _TYPE_END },
};

static struct serializer stone_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(18) },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "level", _INT32 },
	{ "color", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "install_price", _INT32 },
	{ "uninstall_price", _INT32 },
	{ "id_addon_damage", _INT32 },
	{ "id_addon_defence", _INT32 },
	{ "weapon_desc", _WSTRING(16) },
	{ "armor_desc", _WSTRING(16) },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer taskdice_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(19) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "tasks", _ARRAY_START(8) },
		{ "id", _INT32 },
		{ "prob", _FLOAT },
	{ "", _ARRAY_END },
	{ "use_on_pick", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
};

static struct serializer tasknormalmatter_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(20) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer fashion_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(21) },
	{ "major_type", _INT32 },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "realname", _STRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "equip_location", _INT32 },
	{ "level", _INT32 },
	{ "require_level", _INT32 },
	{ "require_dye_count", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "gender", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer faceticket_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(22) },
	{ "major_type", _INT32 },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "require_level", _INT32 },
	{ "bound_file", _STRING(128) },
	{ "unsymmetrical", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer facepill_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(23) },
	{ "major_type", _INT32 },
	{ "minor_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "duration", _INT32 },
	{ "camera_scale", _FLOAT },
	{ "character_combo_id", _INT32 },
	{ "pllfiles", _ARRAY_START(16) },
		{ "", _STRING(128) },
	{ "", _ARRAY_END },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer gm_generator_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(24) },
	{ "id_type", _INT32 },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "id_object", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer pet_egg_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(25) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "id_pet", _INT32 },
	{ "money_hatched", _INT32 },
	{ "money_restored", _INT32 },
	{ "honor_point", _INT32 },
	{ "level", _INT32 },
	{ "exp", _INT32 },
	{ "skill_point", _INT32 },
	{ "skills", _ARRAY_START(32) },
		{ "id", _INT32 },
		{ "level", _INT32 },
	{ "", _ARRAY_END },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer pet_food_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(26) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "level", _INT32 },
	{ "hornor", _INT32 },
	{ "exp", _INT32 },
	{ "food_type", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer pet_faceticket_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(27) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer fireworks_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(28) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "file_fw", _STRING(128) },
	{ "level", _INT32 },
	{ "time_to_fire", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer war_tankcallin_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(29) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer skillmatter_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(30) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "level_required", _INT32 },
	{ "id_skill", _INT32 },
	{ "level_skill", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer refine_ticket_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(31) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "desc", _WSTRING(16) },
	{ "ext_reserved_prob", _FLOAT },
	{ "ext_succeed_prob", _FLOAT },
	{ "fail_reserve_level", _INT32 },
	{ "fail_ext_succeed_probs", _ARRAY_START(12) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer bible_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(32) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "addon_ids", _ARRAY_START(10) },
		{ "", _INT32 },
	{ "", _ARRAY_END },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer speaker_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(33) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "id_icon_set", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer autohp_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(34) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "total_hp", _INT32 },
	{ "trigger_amount", _FLOAT },
	{ "cool_time", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer automp_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(35) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "total_mp", _INT32 },
	{ "trigger_amount", _FLOAT },
	{ "cool_time", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer double_exp_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(36) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "double_exp_time", _INT32 },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer transmitscroll_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(37) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer dye_ticket_essence_serializer[] = {
	{ "id", _CUSTOM, serialize_item_id_fn, deserialize_item_id_fn },
	{ "type", _CONST_INT(38) },
	{ "name", _WSTRING(32) },
	{ "file_matter", _STRING(128) },
	{ "icon", _CUSTOM, icon_serialize_fn, icon_deserialize_fn },
	{ "h_min", _FLOAT },
	{ "h_max", _FLOAT },
	{ "s_min", _FLOAT },
	{ "s_max", _FLOAT },
	{ "v_min", _FLOAT },
	{ "v_max", _FLOAT },
	{ "price", _INT32 },
	{ "shop_price", _INT32 },
	{ "stack_max", _INT32 },
	{ "has_guid", _INT32 },
	{ "proc_type", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer armor_sets_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "max_equips", _INT32 },
	{ "equip_ids", _ARRAY_START(12) },
		{ "", _CUSTOM, serialize_elements_id_field_fn, deserialize_elements_id_field_fn },
	{ "", _ARRAY_END },
	{ "addon_ids", _ARRAY_START(11) },
		{ "", _INT32 },
	{ "", _ARRAY_END },
	{ "file_gfx", _STRING(128) },
	{ "", _TYPE_END },
};

struct __attribute__((packed)) param_adjust_config {
	int32_t id;
	uint16_t name[32];
	struct {
		int32_t level_diff;
		float exp;
		float sp;
		float money;
		float matter;
		float attack;
	} adjust[16];
	struct {
		float adjust_xp;
		float adjust_sp;
	} team_adjust[11];
	struct {
		float adjust_xp;
		float adjust_sp;
	} team_profession_adjust[9];
};

static struct serializer param_adjust_config_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "adjust", _ARRAY_START(16) },
		{ "level_diff", _INT32 },
		{ "exp", _FLOAT },
		{ "sp", _FLOAT },
		{ "money", _FLOAT },
		{ "matter", _FLOAT },
		{ "attack", _FLOAT },
	{ "", _ARRAY_END },
	{ "team_adjust", _ARRAY_START(11) },
		{ "exp", _FLOAT },
		{ "sp", _FLOAT },
	{ "", _ARRAY_END },
	{ "team_profession_adjust", _ARRAY_START(9) },
		{ "exp", _FLOAT },
		{ "sp", _FLOAT },
	{ "", _ARRAY_END },
	{ "", _TYPE_END },
};

struct __attribute__((packed)) player_levelexp_config {
	int32_t id;
	uint16_t name[32];
	int32_t exp[150];
};

static size_t
deserialize_tasks_id_field_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	int64_t val = JSi(f);

	if (f->type == CJSON_TYPE_NONE) {
		return 4;
	}

	if (val >= 0x80000000) {
		int rc = pw_idmap_get_async(g_tasks_map, val, 0, deserialize_id_field_async_fn, data);
		if (rc) {
			assert(false);
		}
	} else {
		deserialize_log(f, data);
		*(uint32_t *)(data) = (uint32_t)val;
	}

	return 4;
}

static size_t
serialize_tasks_id_field_fn(FILE *fp, struct serializer *f, void *data)
{
	uint32_t id = *(uint32_t *)data;

	fprintf(fp, "\"%s\":%d,", f->name, id);
	return 4;
}


static struct serializer npc_tasks_in_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "tasks", _ARRAY_START(32) },
		{ "", _CUSTOM, serialize_tasks_id_field_fn, deserialize_tasks_id_field_fn },
	{ "", _ARRAY_END },
	{ "", _TYPE_END },
};

static struct serializer npc_tasks_out_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "tasks", _ARRAY_START(32) },
		{ "", _CUSTOM, serialize_tasks_id_field_fn, deserialize_tasks_id_field_fn },
	{ "", _ARRAY_END },
	{ "", _TYPE_END },
};

static struct serializer npc_task_matter_service_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(32) },
	{ "tasks", _ARRAY_START(16) },
		{ "id", _INT32 },
		{ "items", _ARRAY_START(4) },
			{ "id", _INT32 },
			{ "num", _INT32 },
		{ "", _ARRAY_END },
	{ "", _ARRAY_END },
	{ "", _TYPE_END },
};

static struct serializer damagerune_sub_type_serializer[] = { { "", _TYPE_END } };
static struct serializer armorrune_sub_type_serializer[] = { { "", _TYPE_END } };
static struct serializer skilltome_sub_type_serializer[] = { { "", _TYPE_END } };
static struct serializer unionscroll_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer monster_addon_serializer[] = { { "", _TYPE_END } };
static struct serializer monster_type_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_talk_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_buy_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_repair_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_install_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_uninstall_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_skill_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_heal_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_transmit_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_transport_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_proxy_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_storage_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_decompose_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_type_serializer[] = { { "", _TYPE_END } };
static struct serializer face_texture_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer face_shape_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer face_emotion_type_serializer[] = { { "", _TYPE_END } };
static struct serializer face_expression_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer face_hair_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer face_moustache_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer colorpicker_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer customizedata_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer recipe_major_type_serializer[] = { { "", _TYPE_END } };
static struct serializer recipe_sub_type_serializer[] = { { "", _TYPE_END } };
static struct serializer enemy_faction_config_serializer[] = { { "", _TYPE_END } };
static struct serializer charracter_class_config_serializer[] = { { "", _TYPE_END } };
static struct serializer player_action_info_config_serializer[] = { { "", _TYPE_END } };
static struct serializer face_faling_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer player_levelexp_config_serializer[] = { { "", _TYPE_END } };
static struct serializer mine_type_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_identify_service_serializer[] = { { "", _TYPE_END } };
static struct serializer fashion_major_type_serializer[] = { { "", _TYPE_END } };
static struct serializer fashion_sub_type_serializer[] = { { "", _TYPE_END } };
static struct serializer faceticket_major_type_serializer[] = { { "", _TYPE_END } };
static struct serializer faceticket_sub_type_serializer[] = { { "", _TYPE_END } };
static struct serializer facepill_major_type_serializer[] = { { "", _TYPE_END } };
static struct serializer facepill_sub_type_serializer[] = { { "", _TYPE_END } };
static struct serializer gm_generator_type_serializer[] = { { "", _TYPE_END } };
static struct serializer pet_type_serializer[] = { { "", _TYPE_END } };
static struct serializer pet_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_war_towerbuild_service_serializer[] = { { "", _TYPE_END } };
static struct serializer player_secondlevel_config_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_resetprop_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_petname_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_petlearnskill_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_petforgetskill_service_serializer[] = { { "", _TYPE_END } };
static struct serializer destroying_essence_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_equipbind_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_equipdestroy_service_serializer[] = { { "", _TYPE_END } };
static struct serializer npc_equipundestroy_service_serializer[] = { { "", _TYPE_END } };

static struct pw_chain_table *
get_table(struct pw_elements *elements, const char *name)
{
	size_t i;

	for (i = 0; i < elements->tables_count; i++) {
		struct pw_chain_table *table = elements->tables[i];
		
		if (strcmp(table->name, name) == 0) {
			return table;
		}
	}

	return NULL;
}

#define EXPORT_TABLE(elements, table_name, filename) \
({ \
	FILE *fp = fopen(filename, "wb"); \
	struct pw_chain_table *table = get_table(elements, #table_name); \
	long sz; \
\
	if (fp == NULL) { \
		PWLOG(LOG_ERROR, "cant open %s for writing\n", filename); \
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
	EXPORT_TABLE(elements, quiver_sub_type, "quiver_types.json");
	EXPORT_TABLE(elements, stone_types, "stone_types.json");

	EXPORT_TABLE(elements, armor_sets, "armor_sets.json");
	EXPORT_TABLE(elements, equipment_addon, "equipment_addon.json");

	EXPORT_TABLE(elements, npc_tasks_in, "npc_tasks_in.json");
	EXPORT_TABLE(elements, npc_tasks_out, "npc_tasks_out.json");
	EXPORT_TABLE(elements, npc_task_matter_service, "npc_tasks_matter.json");

	FILE *fp = fopen("items.json", "wb");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "cant open items.json for writing\n");
		return;
	}
	size_t prev_sz, sz = 0;

#define STR(x, y) #x #y
#define EXPORT_ITEMS(table_name) \
({ \
	struct pw_chain_table *table = get_table(elements, STR(table_name, _essence)); \
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

#undef EXPORT_ITEMS

	fclose(fp);
	truncate("items.json", sz);
}

static int
pw_elements_get_idmap_type(struct pw_elements *elements, const char *obj_type)
{
	struct pw_chain_table *table = NULL;
	int i;

	for (i = 0; i < elements->tables_count; i++) {
		table = elements->tables[i];
		if (strcmp(table->name, obj_type) == 0) {
			break;
		}
	}

	if (i == elements->tables_count) {
		return -1;
	}

	return i + 1;
}

static const char *
get_item_type_by_id(unsigned type) {
	switch (type) {
		case 1: return "weapon_essence";
		case 2: return "armor_essence";
		case 3: return "decoration_essence";
		case 4: return "medicine_essence";
		case 5: return "material_essence";
		case 6: return "damagerune_essence";
		case 7: return "armorrune_essence";
		case 8: return "skilltome_essence";
		case 9: return "flysword_essence";
		case 10: return "wingmanwing_essence";
		case 11: return "townscroll_essence";
		case 12: return "revivescroll_essence";
		case 13: return "element_essence";
		case 14: return "taskmatter_essence";
		case 15: return "tossmatter_essence";
		case 16: return "projectile_essence";
		case 17: return "quiver_essence";
		case 18: return "stone_essence";
		case 19: return "taskdice_essence";
		case 20: return "tasknormalmatter_essence";
		case 21: return "fashion_essence";
		case 22: return "faceticket_essence";
		case 23: return "facepill_essence";
		case 24: return "gm_generator_essence";
		case 25: return "pet_egg_essence";
		case 26: return "pet_food_essence";
		case 27: return "pet_faceticket_essence";
		case 28: return "fireworks_essence";
		case 29: return "war_tankcallin_essence";
		case 30: return "skillmatter_essence";
		case 31: return "refine_ticket_essence";
		case 32: return "bible_essence";
		case 33: return "speaker_essence";
		case 34: return "autohp_essence";
		case 35: return "automp_essence";
		case 36: return "double_exp_essence";
		case 37: return "transmitscroll_essence";
		case 38: return "dye_ticket_essence";
	}

	return "unknown";
}

int
pw_elements_patch_obj(struct pw_elements *elements, struct cjson *obj)
{
	struct pw_chain_table *table = NULL;
	struct pw_idmap_el *node;
	void **table_el;
	const char *obj_type;
	int64_t id;
	int i;
	bool is_item;

	obj_type = JSs(obj, "_db", "type");
	if (!obj_type) {
		PWLOG(LOG_ERROR, "missing obj._db.type\n");
		return -1;
	}

	is_item = strcmp(obj_type, "items") == 0;
	if (is_item) {
		uint32_t type = JSi(obj, "type");
		obj_type = get_item_type_by_id(type);
	}

	id = JSi(obj, "id");
	if (!id) {
		PWLOG(LOG_ERROR, "missing obj.id\n");
		return -1;
	}

	for (i = 0; i < elements->tables_count; i++) {
		table = elements->tables[i];
		if (strcmp(table->name, obj_type) == 0) {
			break;
		}
	}

	if (i == elements->tables_count) {
		if (table && strcmp(table->name, "metadata") != 0) {
			PWLOG(LOG_ERROR, "unknown obj type\n");
		}
		return -1;
	}

	node = pw_idmap_get(g_elements_map, id, table->idmap_type);

	if (node) {
		table_el = node->data;
	} else {
		table_el = pw_chain_table_new_el(table);
		node = pw_idmap_set(g_elements_map, id, table->idmap_type, table_el);

		*(uint32_t *)table_el = node->id;

		if (strcmp(obj_type, "npcs") == 0) {
			*(uint32_t *)serializer_get_field(table->serializer, "base_monster_id", table_el) = 2111;
			*(uint32_t *)serializer_get_field(table->serializer, "id_type", table_el) = 3214;
		}

	}

	if (is_item) {
		const char *desc = JSs(obj, "desc");
		if (desc) {
			uint32_t id = *(uint32_t *)table_el;

			pw_item_desc_set(id, desc);
		}
	}

	deserialize(obj, table->serializer, table_el);
	return 0;
}

static void
pw_elements_load_table(struct pw_elements *elements, const char *name, uint32_t el_size, int skipped_offset, struct serializer *serializer, FILE *fp)
{
	struct pw_chain_table *table;
	struct pw_chain_el *chain;
	int32_t i, count;
	void *el;
	long idmap_type;

	table = calloc(1, sizeof(*table));
	if (!table) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		return;
	}

	table->el_size = el_size;
	table->name = name;
	table->serializer = serializer;

	fread(&count, 1, sizeof(count), fp);
	table->chain = table->chain_last = chain = calloc(1, sizeof(struct pw_chain_el) + count * el_size);
	if (!chain) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		return;
	}
	chain->count = chain->capacity = count;

	idmap_type = pw_idmap_register_type(g_elements_map);
	el = chain->data;
	for (i = 0; i < count; i++) {
		if (skipped_offset) {
			fread(el, 1, skipped_offset, fp);
			fread((char *)el + skipped_offset + 4, 1, el_size - skipped_offset - 4, fp);
		} else {
			fread(el, 1, el_size, fp);
		}

		unsigned id = *(uint32_t *)el;

		pw_idmap_set(g_elements_map, id, idmap_type, el);
		el += el_size;
	}

	table->idmap_type = idmap_type;
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
pw_elements_save_talk_proc(struct pw_elements *elements, FILE *fp)
{
	uint32_t count = elements->talk_proc_cnt;

	fwrite(&count, 1, sizeof(count), fp);
	for (int i = 0; i < count; ++i) {
		struct talk_proc *talk = elements->talk_proc + sizeof(*talk) * i;

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
pw_elements_load(struct pw_elements *el, const char *filename, const char *idmap_filename)
{
	FILE *fp = fopen(filename, "rb");

	if (fp == NULL) {
		PWLOG(LOG_ERROR, "cant open %s\n", filename);
		return 1;
	}

	g_elements_map = pw_idmap_init("elements", idmap_filename, g_idmap_can_set);
	if (!g_elements_map) {
		PWLOG(LOG_ERROR, "pw_idmap_init() failed\n");
		fclose(fp);
		return 1;
	}

	memset(el, 0, sizeof(*el));

	fread(&el->hdr, 1, sizeof(el->hdr), fp);
	if (el->hdr.version != 12 && el->hdr.version != 10) {
		PWLOG(LOG_ERROR, "element version mismatch, expected 10 or 12, found %d\n", el->hdr.version);
		fclose(fp);
		return 1;
	}

#define LOAD_ARR_OFFSET(arr_name, el_size, offset) \
	pw_elements_load_table(el, #arr_name, el_size, offset, arr_name ## _serializer, fp)

#define LOAD_ARR(arr_name, el_size) \
	LOAD_ARR_OFFSET(arr_name, el_size, 0)

	LOAD_ARR(equipment_addon, 84);
	LOAD_ARR(weapon_major_types, 68);
	LOAD_ARR(weapon_minor_types, 356);
	LOAD_ARR(weapon_essence, 1404);
	LOAD_ARR(armor_major_types, 68);
	LOAD_ARR(armor_minor_types, 72);
	LOAD_ARR(armor_essence, 1104);
	LOAD_ARR(decoration_major_types, 68);
	LOAD_ARR(decoration_minor_types, 72);
	LOAD_ARR(decoration_essence, 1156);
	LOAD_ARR(medicine_major_types, 68);
	LOAD_ARR(medicine_minor_types, 68);
	LOAD_ARR(medicine_essence, 376);
	LOAD_ARR(material_major_type, 68);
	LOAD_ARR(material_sub_type, 68);
	LOAD_ARR(material_essence, 368);
	LOAD_ARR(damagerune_sub_type, 68);
	LOAD_ARR(damagerune_essence, 364);
	LOAD_ARR(armorrune_sub_type, 68);
	LOAD_ARR(armorrune_essence, 624);
	load_control_block_0(&el->control_block0, fp);
	LOAD_ARR(skilltome_sub_type, 68);
	LOAD_ARR(skilltome_essence, 348);
	LOAD_ARR(flysword_essence, 516);
	LOAD_ARR(wingmanwing_essence, 488);
	LOAD_ARR(townscroll_essence, 348);
	LOAD_ARR(unionscroll_essence, 348);
	LOAD_ARR(revivescroll_essence, 352);
	LOAD_ARR(element_essence, 348);
	LOAD_ARR(taskmatter_essence, 208);
	LOAD_ARR(tossmatter_essence, 888);
	LOAD_ARR(projectile_types, 68);
	LOAD_ARR(projectile_essence, 892);
	LOAD_ARR(quiver_sub_type, 68);
	LOAD_ARR(quiver_essence, 340);
	LOAD_ARR(stone_types, 68);
	LOAD_ARR(stone_essence, 436);
	LOAD_ARR(monster_addon, 84);
	LOAD_ARR(monster_type, 196);
	LOAD_ARR(monsters, 1500);
	LOAD_ARR(npc_talk_service, 72);
	LOAD_ARR(npc_sells, 1224);
	LOAD_ARR(npc_buy_service, 72);
	LOAD_ARR(npc_repair_service, 72);
	LOAD_ARR(npc_install_service, 200);
	LOAD_ARR(npc_uninstall_service, 200);
	LOAD_ARR(npc_tasks_in, 196);
	LOAD_ARR(npc_tasks_out, 196);
	LOAD_ARR(npc_task_matter_service, 644);
	LOAD_ARR(npc_skill_service, 584);
	LOAD_ARR(npc_heal_service, 72);
	LOAD_ARR(npc_transmit_service, 460);
	LOAD_ARR(npc_transport_service, 328);
	LOAD_ARR(npc_proxy_service, 72);
	LOAD_ARR(npc_storage_service, 68);
	LOAD_ARR_OFFSET(npc_crafts, 1228, (el->hdr.version == 10) ? 72 : 0);
	LOAD_ARR(npc_decompose_service, 72);
	LOAD_ARR(npc_type, 68);
	LOAD_ARR(npcs, 848);
	pw_elements_load_talk_proc(el, fp);
	LOAD_ARR(face_texture_essence, 476);
	LOAD_ARR(face_shape_essence, 348);
	LOAD_ARR(face_emotion_type, 196);
	LOAD_ARR(face_expression_essence, 336);
	LOAD_ARR(face_hair_essence, 468);
	LOAD_ARR(face_moustache_essence, 340);
	LOAD_ARR(colorpicker_essence, 208);
	LOAD_ARR(customizedata_essence, 204);
	LOAD_ARR(recipe_major_type, 68);
	LOAD_ARR(recipe_sub_type, 68);
	LOAD_ARR_OFFSET(recipes, 404, (el->hdr.version == 10) ? 88 : 0);
	LOAD_ARR(enemy_faction_config, 196);
	LOAD_ARR(charracter_class_config, 160);
	LOAD_ARR(param_adjust_config, 612);
	LOAD_ARR(player_action_info_config, 488);
	LOAD_ARR(taskdice_essence, 404);
	LOAD_ARR(tasknormalmatter_essence, 344);
	LOAD_ARR(face_faling_essence, 340);
	LOAD_ARR(player_levelexp_config, 668);
	LOAD_ARR(mine_type, 68);
	LOAD_ARR(mines, 452);
	LOAD_ARR(npc_identify_service, 72);
	LOAD_ARR(fashion_major_type, 68);
	LOAD_ARR(fashion_sub_type, 72);
	LOAD_ARR(fashion_essence, 404);
	LOAD_ARR(faceticket_major_type, 68);
	LOAD_ARR(faceticket_sub_type, 68);
	LOAD_ARR(faceticket_essence, 488);
	LOAD_ARR(facepill_major_type, 68);
	LOAD_ARR(facepill_sub_type, 68);
	LOAD_ARR(facepill_essence, 2412);
	LOAD_ARR(armor_sets, 292);
	LOAD_ARR(gm_generator_type, 68);
	LOAD_ARR(gm_generator_essence, 344);
	LOAD_ARR(pet_type, 68);
	LOAD_ARR(pet_essence, 480);
	LOAD_ARR(pet_egg_essence, 628);
	LOAD_ARR(pet_food_essence, 360);
	LOAD_ARR(pet_faceticket_essence, 344);
	LOAD_ARR(fireworks_essence, 480);
	LOAD_ARR(war_tankcallin_essence, 344);
	load_control_block_1(&el->control_block1, fp);
	LOAD_ARR(npc_war_towerbuild_service, 148);
	LOAD_ARR(player_secondlevel_config, 1092);
	LOAD_ARR(npc_resetprop_service, 368);
	LOAD_ARR(npc_petname_service, 76);
	LOAD_ARR(npc_petlearnskill_service, 584);
	LOAD_ARR(npc_petforgetskill_service, 76);
	LOAD_ARR(skillmatter_essence, 356);
	LOAD_ARR(refine_ticket_essence, 436);
	LOAD_ARR(destroying_essence, 344);
	LOAD_ARR(npc_equipbind_service, 76);
	LOAD_ARR(npc_equipdestroy_service, 76);
	LOAD_ARR(npc_equipundestroy_service, 76);
	LOAD_ARR(bible_essence, 384);
	LOAD_ARR(speaker_essence, 348);
	LOAD_ARR(autohp_essence, 356);
	LOAD_ARR(automp_essence, 356);
	LOAD_ARR(double_exp_essence, 348);
	LOAD_ARR(transmitscroll_essence, 344);
	LOAD_ARR(dye_ticket_essence, 368);

#undef LOAD_ARR

	fclose(fp);

	g_elements_taskmatter_idmap_id = pw_elements_get_idmap_type(el, "taskmatter_essence");
	g_elements_recipes_idmap_id = pw_elements_get_idmap_type(el, "recipes");
	g_elements_npc_idmap_id = pw_elements_get_idmap_type(el, "npcs");

	return pw_item_desc_load("patcher/item_desc.data");
}

static void
pw_elements_save_table(struct pw_chain_table *table, FILE *fp, int skipped_offset)
{
	struct pw_chain_el *chain;
	size_t count_off;
	uint32_t count = 0;

	count_off = ftell(fp);
	/* item count goes here, but we don't know it yet */
	fseek(fp, 4, SEEK_CUR);

	chain = table->chain;
	while (chain) {
		for (uint32_t i = 0; i < chain->count; i++) {
			void *el = (void *)(chain->data + i * table->el_size);

			/* skip items with the *removed* bit set */
			if (*(uint32_t *)el & (1 << 31)) continue;
			if (skipped_offset) {
				fwrite(el, 1, skipped_offset, fp);
				fwrite((char *)el + skipped_offset + 4, 1, table->el_size - skipped_offset - 4, fp);
			} else {
				fwrite(el, 1, table->el_size, fp);
			}
			count++;
		}
		chain = chain->next;
	}

	size_t end_off = ftell(fp);
	fseek(fp, count_off, SEEK_SET);
	fwrite(&count, 1, sizeof(count), fp);
	fseek(fp, end_off, SEEK_SET);
}

int
pw_elements_save(struct pw_elements *el, const char *filename, bool is_server)
{
	FILE *fp;
	FILE *server_fp = NULL;

	fp = fopen(filename, "wb");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "cant open %s\n", filename);
		return 1;
	}

	if (is_server) {
		/* server and client are technically different version */
		server_fp = fopen("patcher/server_elements_hdr.data", "rb");
		if (fp == NULL) {
			PWLOG(LOG_ERROR, "cant open patcher/server_elements_hdr.data\n");
			return 1;
		}

		struct header hdr;

		fread(&hdr, 1, sizeof(hdr), server_fp);
		fwrite(&hdr, 1, sizeof(hdr), fp);
	} else {
		fwrite(&el->hdr, 1, sizeof(el->hdr), fp);
	}

	size_t i;
	for (i = 0; i < el->tables_count; i++) {
		struct pw_chain_table *table = el->tables[i];

		if (strcmp(table->name, "npc_crafts") == 0) {
			void *el;
			struct npc_crafts *crafts;
			struct pw_idmap_el *node;
			struct recipes *recipe;

			PW_CHAIN_TABLE_FOREACH(el, table) {
				crafts = el;
				for (int p = 0; p < 8; p++) {
					for (int ridx = 0; ridx < 32; ridx++) {
						uint32_t rid = crafts->pages[p].recipe_id[ridx];
						if (rid == 0) {
							continue;
						}

						node = pw_idmap_get(g_elements_map, rid, g_elements_recipes_idmap_id);

						if (node) {
							recipe = node->data;
							rid = recipe->targets[0].id ? rid : 0;
						} else {
							rid = 0;
						}

						crafts->pages[p].recipe_id[ridx] = rid;
					}
				}
			}

			pw_elements_save_table(table, fp, is_server ? 72 : 0);
		} else if (is_server && strcmp(table->name, "recipes") == 0) {
			pw_elements_save_table(table, fp, 88);
		} else {
			pw_elements_save_table(table, fp, 0);
		}

		/* save additional data after some specific tables */
		if (strcmp(table->name, "armorrune_essence") == 0) {
			if (is_server) {
				struct control_block0 cb0;

				load_control_block_0(&cb0, server_fp);
				save_control_block_0(&cb0, fp);
			} else {
				save_control_block_0(&el->control_block0, fp);
			}
		} else if (strcmp(table->name, "war_tankcallin_essence") == 0) {
			if (is_server) {
				struct control_block1 cb1;

				load_control_block_1(&cb1, server_fp);
				save_control_block_1(&cb1, fp);
			} else {
				save_control_block_1(&el->control_block1, fp);
			}
		} else if (strcmp(table->name, "npcs") == 0) {
			pw_elements_save_talk_proc(el, fp);
		}
	}

	fclose(fp);

	if (is_server) {
		return 0;
	}

	return pw_item_desc_save();
}

int
pw_elements_idmap_save(struct pw_elements *el, const char *filename)
{
	return pw_idmap_save(g_elements_map, filename);
}

static struct pw_chain_table *
get_chain_table(struct pw_elements *elements, const char *name)
{
	struct pw_chain_table *table;
	int i;

	for (i = 0; i < elements->tables_count; i++) {
		table = elements->tables[i];
		if (strcmp(table->name, name) == 0) {
			return table;
		}
	}

	return NULL;
}

void
pw_elements_prepare(struct pw_elements *elements)
{
	struct recipes *recipe;
	struct pw_chain_table *tbl = get_chain_table(elements, "recipes");
	void *el;

	PW_CHAIN_TABLE_FOREACH(el, tbl) {
		int next_idx = 0;
		recipe = el;

		for (int i = 0; i < 32; i++) {
			if (recipe->mats[i].id) {
				recipe->mats[next_idx].id = recipe->mats[i].id;
				recipe->mats[next_idx].num = recipe->mats[i].num;
				next_idx++;
			}

			if (i >= next_idx) {
				recipe->mats[i].id = 0;
				recipe->mats[i].num = 0;
			}
		}
	}
}

void
pw_elements_adjust_rates(struct pw_elements *elements, struct cjson *rates)
{
	struct pw_idmap_el *node;
	double xp_rate = JSf(rates, "mob", "xp");
	double sp_rate = JSf(rates, "mob", "sp");
	double coin_rate = JSf(rates, "mob", "coin");
	double pet_xp = JSf(rates, "pet_xp");

	PWLOG(LOG_INFO, "Adjusting rates:\n");
	PWLOG(LOG_INFO, "  mob xp:   %8.4f\n", xp_rate);
	PWLOG(LOG_INFO, "  mob sp:   %8.4f\n", sp_rate);
	PWLOG(LOG_INFO, "  mob coin: %8.4f\n", coin_rate);
	PWLOG(LOG_INFO, "  pet xp:   %8.4f\n", pet_xp);

	struct pw_chain_table *tbl = get_chain_table(elements, "param_adjust_config");
	node = pw_idmap_get(g_elements_map, 10, tbl->idmap_type);
	struct param_adjust_config *exp_penalty_cfg = (void *)node->data;

	for (int i = 0; i < 16; i++) {
		exp_penalty_cfg->adjust[i].matter = 1.0f;
	}

	void *el;

	tbl = get_chain_table(elements, "monsters");
	int monster_xp_off = serializer_get_offset(tbl->serializer, "exp");
	int monster_sp_off = serializer_get_offset(tbl->serializer, "sp");
	int monster_money_average_off = serializer_get_offset(tbl->serializer, "money_average");
	int monster_money_var_off = serializer_get_offset(tbl->serializer, "money_var");

	PW_CHAIN_TABLE_FOREACH(el, tbl) {
		*(uint32_t *)(el + monster_xp_off) *= xp_rate;
		*(uint32_t *)(el + monster_sp_off) *= sp_rate;
		*(uint32_t *)(el + monster_money_average_off) *= coin_rate;
		*(uint32_t *)(el + monster_money_var_off) *= coin_rate;
	}

	tbl = get_chain_table(elements, "player_levelexp_config");
	node = pw_idmap_get(g_elements_map, 592, tbl->idmap_type);
	struct player_levelexp_config *pet_exp_cfg = (void *)node->data;
	for (int i = 0; i < 150; i++) {
		pet_exp_cfg->exp[i] /= pet_xp;
	}
}
