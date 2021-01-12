/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_ELEMENTS_H
#define PW_ELEMENTS_H

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#include <uchar.h>

struct pw_elements;
struct cjson;
struct pw_idmap;

extern uint32_t g_elements_last_id;
extern struct pw_idmap *g_elements_map;

int pw_elements_load(struct pw_elements *el, const char *filename);
void pw_elements_serialize(struct pw_elements *elements);
int pw_elements_patch_obj(struct pw_elements *elements, struct cjson *obj);

struct serializer;
struct pw_elements_chain;
struct pw_elements_table {
	const char *name;
	struct serializer *serializer;
	size_t el_size;
	struct pw_elements_chain *chain;
	struct pw_elements_chain *chain_last;
};

struct pw_elements_chain {
	struct pw_elements_table_el *next;
	size_t capacity;
	size_t count;
	char data[0];
};

struct pw_elements {
	struct header {
			int16_t version;
			int16_t signature;
			int32_t unk1;
	} hdr;

	struct control_block0 {
			int32_t unk1;
			int32_t size;
			char *unk2;
			int32_t unk3;
	} control_block0;

	struct control_block1 {
			int32_t unk1;
			int32_t size;
			char *unk2;
	} control_block1;

	int32_t talk_proc_cnt;
	struct talk_proc {
			int32_t id;
			char16_t name[64];
			int32_t questions_cnt;

			struct question {
					int32_t id;
					int32_t control;
					int32_t text_size;
					char16_t *text;
					int32_t choices_cnt;

					struct choice {
							int32_t id;
							char16_t text[64];
							int32_t param;
					} *choices;
			} *questions;
	};
	struct pw_elements_table *talk_proc;

	int32_t equipment_addon_cnt;
	struct equipment_addon {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t num_params;
		int32_t param1;
		int32_t param2;
		int32_t param3;
	};
	struct pw_elements_table *equipment_addon;

	int32_t weapon_major_types_cnt;
	struct weapon_major_types {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
	struct pw_elements_table *weapon_major_types;

	int32_t weapon_minor_types_cnt;
	struct weapon_minor_types {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_hitgfx[128 / sizeof(char)];
		char file_hitsfx[128 / sizeof(char)];
		float probability_fastest;
		float probability_fast;
		float probability_normal;
		float probability_slow;
		float probability_slowest;
		float attack_speed;
		float attack_short_range;
		int32_t action_type;
	};
	struct pw_elements_table *weapon_minor_types;

	int32_t weapon_essence_cnt;
	struct weapon_essence {
		int32_t id;
		int32_t id_major_type;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		int32_t require_projectile;
		char file_model_right[128 / sizeof(char)];
		char file_model_left[128 / sizeof(char)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t require_strength;
		int32_t require_agility;
		int32_t require_energy;
		int32_t require_tili;
		int32_t character_combo_id;
		int32_t require_level;
		int32_t level;
		int32_t fixed_props;
		int32_t damage_low;
		int32_t damage_high_min;
		int32_t damage_high_max;
		int32_t magic_damage_low;
		int32_t magic_damage_high_min;
		int32_t magic_damage_high_max;
		float attack_range;
		int32_t short_range_mode;
		int32_t durability_min;
		int32_t durability_max;
		int32_t levelup_addon;
		int32_t material_need;
		int32_t price;
		int32_t shop_price;
		int32_t repairfee;
		float drop_socket_prob[3];
		float make_socket_prob[3];
		float addon_num_prob[4];
		float probability_unique;
        struct {
            int32_t id;
            float prob;
        } addons[32];
        struct {
            int32_t id;
            float prob;
        } rands[32];
        struct {
            int32_t id;
            float prob;
        } uniques[16];
		int32_t durability_drop_min;
		int32_t durability_drop_max;
		int32_t decompose_price;
		int32_t decompose_time;
		int32_t element_id;
		int32_t element_num;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *weapon_essence;

	int32_t armor_major_types_cnt;
	struct armor_major_types {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *armor_major_types;

	int32_t armor_minor_types_cnt;
	struct armor_minor_types {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t equip_mask;
	};
        struct pw_elements_table *armor_minor_types;

	int32_t armor_essence_cnt;
	struct armor_essence {
		int32_t id;
		int32_t id_major_type;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char realname[32 / sizeof(char)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t equip_location;
		int32_t level;
		int32_t require_strength;
		int32_t require_agility;
		int32_t require_energy;
		int32_t require_tili;
		int32_t character_combo_id;
		int32_t require_level;
		int32_t fixed_props;
		int32_t phys_def_low;
		int32_t phys_def_high;
		struct {
		    int32_t low;
		    int32_t high;
		} magic_def[5];
		int32_t mp_enhance_low;
		int32_t mp_enhance_high;
		int32_t hp_enhance_low;
		int32_t hp_enhance_high;
		int32_t armor_enhance_low;
		int32_t armor_enhance_high;
		int32_t durability_min;
		int32_t durability_max;
		int32_t levelup_addon;
		int32_t material_need;
		int32_t price;
		int32_t shop_price;
		int32_t repairfee;
		float drop_socket_prob[5];
		float make_socket_prob[5];
		float addon_num_prob[4];
        struct {
            int32_t id;
            float prob;
        } addons[32];
        struct {
            int32_t id;
            float prob;
        } rands[32];
		int32_t durability_drop_min;
		int32_t durability_drop_max;
		int32_t decompose_price;
		int32_t decompose_time;
		int32_t element_id;
		int32_t element_num;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *armor_essence;

	int32_t decoration_major_types_cnt;
	struct decoration_major_types {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *decoration_major_types;

	int32_t decoration_minor_types_cnt;
	struct decoration_minor_types {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t equip_mask;
	};
        struct pw_elements_table *decoration_minor_types;

	int32_t decoration_essence_cnt;
	struct decoration_essence {
		int32_t id;
		int32_t id_major_type;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_model[128 / sizeof(char)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t level;
		int32_t require_strength;
		int32_t require_agility;
		int32_t require_energy;
		int32_t require_tili;
		int32_t character_combo_id;
		int32_t require_level;
		int32_t fixed_props;
		int32_t damage_low;
		int32_t damage_high;
		int32_t magic_damage_low;
		int32_t magic_damage_high;
		int32_t phys_def_low;
		int32_t phys_def_high;
        struct {
            int32_t low;
            int32_t high;
        } magic_def[5];
		int32_t armor_enhance_low;
		int32_t armor_enhance_high;
		int32_t durability_min;
		int32_t durability_max;
		int32_t levelup_addon;
		int32_t material_need;
		int32_t price;
		int32_t shop_price;
		int32_t repairfee;
		float addon_num_prob[4];
        struct {
            int32_t id;
            float prob;
        } addons[32];
        struct {
            int32_t id;
            float prob;
        } rands[32];
		int32_t durability_drop_min;
		int32_t durability_drop_max;
		int32_t decompose_price;
		int32_t decompose_time;
		int32_t element_id;
		int32_t element_num;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *decoration_essence;

	int32_t medicine_major_types_cnt;
	struct medicine_major_types {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *medicine_major_types;

	int32_t medicine_minor_types_cnt;
	struct medicine_minor_types {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *medicine_minor_types;

	int32_t medicine_essence_cnt;
	struct medicine_essence {
		int32_t id;
		int32_t id_major_type;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t require_level;
		int32_t cool_time;
		int32_t hp_add_total;
		int32_t hp_add_time;
		int32_t mp_add_total;
		int32_t mp_add_time;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *medicine_essence;

	int32_t material_major_type_cnt;
	struct material_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *material_major_type;

	int32_t material_sub_type_cnt;
	struct material_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *material_sub_type;

	int32_t material_essence_cnt;
	struct material_essence {
		int32_t id;
		int32_t id_major_type;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t price;
		int32_t shop_price;
		int32_t decompose_price;
		int32_t decompose_time;
		int32_t element_id;
		int32_t element_num;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *material_essence;

	int32_t damagerune_sub_type_cnt;
	struct damagerune_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *damagerune_sub_type;

	int32_t damagerune_essence_cnt;
	struct damagerune_essence {
		int32_t id;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t is_magic;
		int32_t price;
		int32_t shop_price;
		int32_t require_weapon_level_min;
		int32_t require_weapon_level_max;
		int32_t damage_increased;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *damagerune_essence;

	int32_t armorrune_sub_type_cnt;
	struct armorrune_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
	struct pw_elements_table *armorrune_sub_type;

	int32_t armorrune_essence_cnt;
	struct armorrune_essence {
		int32_t id;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		char file_gfx[128 / sizeof(char)];
		char file_sfx[128 / sizeof(char)];
		int32_t is_magic;
		int32_t price;
		int32_t shop_price;
		int32_t require_player_level_min;
		int32_t require_player_level_max;
		float damage_reduce_percent;
		int32_t damage_reduce_time;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *armorrune_essence;

	int32_t skilltome_sub_type_cnt;
	struct skilltome_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *skilltome_sub_type;

	int32_t skilltome_essence_cnt;
	struct skilltome_essence {
		int32_t id;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *skilltome_essence;

	int32_t flysword_essence_cnt;
	struct flysword_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_model[128 / sizeof(char)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t price;
		int32_t shop_price;
		int32_t level;
		int32_t require_player_level_min;
		float speed_increase_min;
		float speed_increase_max;
		float speed_rush_increase_min;
		float speed_rush_increase_max;
		float time_max_min;
		float time_max_max;
		float time_increase_per_element;
		int32_t fly_mode;
		int32_t character_combo_id;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *flysword_essence;

	int32_t wingmanwing_essence_cnt;
	struct wingmanwing_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_model[128 / sizeof(char)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t price;
		int32_t shop_price;
		int32_t require_player_level_min;
		float speed_increase;
		int32_t mp_launch;
		int32_t mp_per_second;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *wingmanwing_essence;

	int32_t townscroll_essence_cnt;
	struct townscroll_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		float use_time;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *townscroll_essence;

	int32_t unionscroll_essence_cnt;
	struct unionscroll_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		float use_time;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *unionscroll_essence;

	int32_t revivescroll_essence_cnt;
	struct revivescroll_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		float use_time;
		int32_t cool_time;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *revivescroll_essence;

	int32_t element_essence_cnt;
	struct element_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t level;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *element_essence;

	int32_t taskmatter_essence_cnt;
	struct taskmatter_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_icon[128 / sizeof(char)];
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *taskmatter_essence;

	int32_t tossmatter_essence_cnt;
	struct tossmatter_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_model[128 / sizeof(char)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		char file_firegfx[128 / sizeof(char)];
		char file_hitgfx[128 / sizeof(char)];
		char file_hitsfx[128 / sizeof(char)];
		int32_t require_strength;
		int32_t require_agility;
		int32_t require_level;
		int32_t damage_low;
		int32_t damage_high_min;
		int32_t damage_high_max;
		float use_time;
		float attack_range;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *tossmatter_essence;

	int32_t projectile_types_cnt;
	struct projectile_types {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *projectile_types;

	int32_t projectile_essence_cnt;
	struct projectile_essence {
		int32_t id;
		int32_t type;
		char16_t name[64 / sizeof(char16_t)];
		char file_model[128 / sizeof(char)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		char file_firegfx[128 / sizeof(char)];
		char file_hitgfx[128 / sizeof(char)];
		char file_hitsfx[128 / sizeof(char)];
		int32_t require_weapon_level_min;
		int32_t require_weapon_level_max;
		int32_t damage_enhance;
		int32_t damage_scale_enhance;
		int32_t price;
		int32_t shop_price;
		int32_t id_addon0;
		int32_t id_addon1;
		int32_t id_addon2;
		int32_t id_addon3;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *projectile_essence;

	int32_t quiver_sub_type_cnt;
	struct quiver_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *quiver_sub_type;

	int32_t quiver_essence_cnt;
	struct quiver_essence {
		int32_t id;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t id_projectile;
		int32_t num_min;
		int32_t num_max;
	};
        struct pw_elements_table *quiver_essence;

	int32_t stone_types_cnt;
	struct stone_types {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *stone_types;

	int32_t stone_essence_cnt;
	struct stone_essence {
		int32_t id;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t level;
		int32_t color;
		int32_t price;
		int32_t shop_price;
		int32_t install_price;
		int32_t uninstall_price;
		int32_t id_addon_damage;
		int32_t id_addon_defence;
		char16_t weapon_desc[32 / sizeof(char16_t)];
		char16_t armor_desc[32 / sizeof(char16_t)];
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *stone_essence;

	int32_t monster_addon_cnt;
	struct monster_addon {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t num_params;
		int32_t param1;
		int32_t param2;
		int32_t param3;
	};
        struct pw_elements_table *monster_addon;

	int32_t monster_type_cnt;
	struct monster_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t addons_1_id_addon;
		float addons_1_probability_addon;
		int32_t addons_2_id_addon;
		float addons_2_probability_addon;
		int32_t addons_3_id_addon;
		float addons_3_probability_addon;
		int32_t addons_4_id_addon;
		float addons_4_probability_addon;
		int32_t addons_5_id_addon;
		float addons_5_probability_addon;
		int32_t addons_6_id_addon;
		float addons_6_probability_addon;
		int32_t addons_7_id_addon;
		float addons_7_probability_addon;
		int32_t addons_8_id_addon;
		float addons_8_probability_addon;
		int32_t addons_9_id_addon;
		float addons_9_probability_addon;
		int32_t addons_10_id_addon;
		float addons_10_probability_addon;
		int32_t addons_11_id_addon;
		float addons_11_probability_addon;
		int32_t addons_12_id_addon;
		float addons_12_probability_addon;
		int32_t addons_13_id_addon;
		float addons_13_probability_addon;
		int32_t addons_14_id_addon;
		float addons_14_probability_addon;
		int32_t addons_15_id_addon;
		float addons_15_probability_addon;
		int32_t addons_16_id_addon;
		float addons_16_probability_addon;
	};
        struct pw_elements_table *monster_type;

	int32_t monsters_cnt;
	struct monsters {
		int32_t id;
		int32_t id_type;
		char16_t name[64 / sizeof(char16_t)];
		char16_t prop[32 / sizeof(char16_t)];
		char16_t desc[32 / sizeof(char16_t)];
		int32_t faction;
		int32_t monster_faction;
		char file_model[128 / sizeof(char)];
		char file_gfx_short[128 / sizeof(char)];
		char file_gfx_short_hit[128 / sizeof(char)];
		float size;
		float damage_delay;
		int32_t id_strategy;
		int32_t role_in_war;
		int32_t level;
		int32_t show_level;
		int32_t id_pet_egg;
		int32_t hp;
		int32_t phys_def;
		int32_t magic_def[5];
		int32_t immune_type;
		int32_t exp;
		int32_t sp;
		int32_t money_average;
		int32_t money_var;
		int32_t short_range_mode;
		int32_t sight_range;
		int32_t attack;
		int32_t armor;
		int32_t damage_min;
		int32_t damage_max;
		struct {
				int32_t min;
				int32_t max;
		} magic_damage_ext[5];
		float attack_range;
		float attack_speed;
		int32_t magic_damage_min;
		int32_t magic_damage_max;
		int32_t skill_id;
		int32_t skill_level;
		int32_t hp_regenerate;
		int32_t is_aggressive;
		int32_t monster_faction_ask_help;
		int32_t monster_faction_can_help;
		float aggro_range;
		float aggro_time;
		int32_t inhabit_type;
		int32_t patrol_mode;
		int32_t stand_mode;
		float walk_speed;
		float run_speed;
		float fly_speed;
		float swim_speed;
		int32_t common_strategy;
		struct {
				int32_t id;
				float probability;
		} aggro_strategy[4];
		struct {
			struct {
				int32_t id;
				int32_t level;
				float prob;
			} skill[5];
			/* 75% hp, 50% hp, 25% hp */
		} hp_autoskill[3];
		int32_t skill_on_death;
		struct {
			int32_t id;
			int32_t level;
		} skill[32];
		float probability_drop_num[4]; /* 0 1 2 3 */
		int32_t drop_times;
		int32_t drop_protected;
		struct {
			int32_t id;
			float prob;
		} drop_matter[32];
	};
        struct pw_elements_table *monsters;

	int32_t npc_talk_service_cnt;
	struct npc_talk_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_dialog;
	};
        struct pw_elements_table *npc_talk_service;

	int32_t npc_sells_cnt;
	struct npc_sells {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		struct npc_sells_page {
			char16_t page_title[16 / sizeof(char16_t)];
			int32_t id_goods[32];
		} pages[8];
		int32_t id_dialog;
	};
        struct pw_elements_table *npc_sells;

	int32_t npc_buy_service_cnt;
	struct npc_buy_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_dialog;
	};
        struct pw_elements_table *npc_buy_service;

	int32_t npc_repair_service_cnt;
	struct npc_repair_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_dialog;
	};
        struct pw_elements_table *npc_repair_service;

	int32_t npc_install_service_cnt;
	struct npc_install_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t _unused1[32];
		int32_t _unused2;
	};
	struct pw_elements_table *npc_install_service;

	int32_t npc_uninstall_service_cnt;
	struct npc_uninstall_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t _unused1[32];
		int32_t _unused2;
	};
        struct pw_elements_table *npc_uninstall_service;

	int32_t npc_task_in_service_cnt;
	struct npc_task_in_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t task_id[32];
	};
        struct pw_elements_table *npc_task_in_service;

	int32_t npc_task_out_service_cnt;
	struct npc_task_out_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t task_id[32];
	};
        struct pw_elements_table *npc_task_out_service;

	int32_t npc_task_matter_service_cnt;
	struct npc_task_matter_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		struct {
			int32_t task;
			struct {
				int32_t id;
				int32_t num;
			} item[4];
		} task[16];
	};
        struct pw_elements_table *npc_task_matter_service;

	int32_t npc_skill_service_cnt;
	struct npc_skill_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t skill_id[128];
		int32_t id_dialog;
	};
        struct pw_elements_table *npc_skill_service;

	int32_t npc_heal_service_cnt;
	struct npc_heal_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_dialog;
	};
        struct pw_elements_table *npc_heal_service;

	int32_t npc_transmit_service_cnt;
	struct npc_transmit_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t num_targets;
		struct {
			int32_t id;
			int32_t fee;
			int32_t required_level;
		} target[32];
		int32_t id_dialog;
	};
        struct pw_elements_table *npc_transmit_service;

	int32_t npc_transport_service_cnt;
	struct npc_transport_service { /* unused */
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		struct {
			int32_t id;
			int32_t fee;
		} route[32];
		int32_t id_dialog;
	};
        struct pw_elements_table *npc_transport_service;

	int32_t npc_proxy_service_cnt;
	struct npc_proxy_service { /* unused */
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_dialog;
	};
        struct pw_elements_table *npc_proxy_service;

	int32_t npc_storage_service_cnt;
	struct npc_storage_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *npc_storage_service;

	int32_t npc_crafts_cnt;
	struct npc_crafts {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_make_skill;
		int32_t produce_type;
		struct npc_crafts_page {
			char16_t page_title[16 / sizeof(char16_t)];
			int32_t item_id[32];
		} pages[8];
	};
        struct pw_elements_table *npc_crafts;

	int32_t npc_decompose_service_cnt;
	struct npc_decompose_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_decompose_skill;
	};
        struct pw_elements_table *npc_decompose_service;

	int32_t npc_type_cnt;
	struct npc_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *npc_type;

	int32_t npcs_cnt;
	struct npcs {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_type;
		float refresh_time;
		int32_t attack_rule;
		char file_model[128 / sizeof(char)];
		float tax_rate;
		int32_t base_monster_id;
		char16_t greeting[512 / sizeof(char16_t)];
		int32_t target_id;
		int32_t domain_related;
		int32_t id_talk_service;
		int32_t id_sell_service;
		int32_t id_buy_service;
		int32_t id_repair_service;
		int32_t id_install_service;
		int32_t id_uninstall_service;
		int32_t id_task_out_service;
		int32_t id_task_in_service;
		int32_t id_task_matter_service;
		int32_t id_skill_service;
		int32_t id_heal_service;
		int32_t id_transmit_service;
		int32_t id_transport_service;
		int32_t id_proxy_service;
		int32_t id_storage_service;
		int32_t id_make_service;
		int32_t id_decompose_service;
		int32_t id_identify_service;
		int32_t id_war_towerbuild_service;
		int32_t id_resetprop_service;
		int32_t id_petname_service;
		int32_t id_petlearnskill_service;
		int32_t id_petforgetskill_service;
		int32_t id_equipbind_service;
		int32_t id_equipdestroy_service;
		int32_t id_equipundestroy_service;
		int32_t combined_services;
		int32_t id_mine;
	};
        struct pw_elements_table *npcs;

	int32_t face_texture_essence_cnt;
	struct face_texture_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_base_tex[128 / sizeof(char)];
		char file_high_tex[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t tex_part_id;
		int32_t character_combo_id;
		int32_t gender_id;
		int32_t visualize_id;
		int32_t user_data;
		int32_t facepill_only;
	};
        struct pw_elements_table *face_texture_essence;

	int32_t face_shape_essence_cnt;
	struct face_shape_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_shape[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t shape_part_id;
		int32_t character_combo_id;
		int32_t gender_id;
		int32_t visualize_id;
		int32_t user_data;
		int32_t facepill_only;
	};
        struct pw_elements_table *face_shape_essence;

	int32_t face_emotion_type_cnt;
	struct face_emotion_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_icon[128 / sizeof(char)];
	};
        struct pw_elements_table *face_emotion_type;

	int32_t face_expression_essence_cnt;
	struct face_expression_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_expression[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t character_combo_id;
		int32_t gender_id;
		int32_t emotion_id;
	};
        struct pw_elements_table *face_expression_essence;

	int32_t face_hair_essence_cnt;
	struct face_hair_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_hair_skin[128 / sizeof(char)];
		char file_hair_model[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t character_combo_id;
		int32_t gender_id;
		int32_t visualize_id;
		int32_t facepill_only;
	};
        struct pw_elements_table *face_hair_essence;

	int32_t face_moustache_essence_cnt;
	struct face_moustache_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_moustache_skin[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t character_combo_id;
		int32_t gender_id;
		int32_t visualize_id;
		int32_t facepill_only;
	};
        struct pw_elements_table *face_moustache_essence;

	int32_t colorpicker_essence_cnt;
	struct colorpicker_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_colorpicker[128 / sizeof(char)];
		int32_t color_part_id;
		int32_t character_combo_id;
		int32_t gender_id;
	};
        struct pw_elements_table *colorpicker_essence;

	int32_t customizedata_essence_cnt;
	struct customizedata_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_data[128 / sizeof(char)];
		int32_t character_combo_id;
		int32_t gender_id;
	};
        struct pw_elements_table *customizedata_essence;

	int32_t recipe_major_type_cnt;
	struct recipe_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *recipe_major_type;

	int32_t recipe_sub_type_cnt;
	struct recipe_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *recipe_sub_type;

	int32_t recipes_cnt;
	struct recipes {
		int32_t id;
		int32_t id_major_type;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		int32_t recipe_level;
		int32_t skill_id;
		int32_t skill_level;
		int32_t bind_type;
		struct recipes_target {
			int32_t id;
			float prob;
		} targets[4];
		float fail_prob;
		int32_t num_to_make;
		int32_t coins;
		float duration;
		int32_t xp;
		int32_t sp;
		struct recipes_material {
			int32_t id;
			int32_t num;
		} mats[32];
	};
        struct pw_elements_table *recipes;

	int32_t enemy_faction_config_cnt;
	struct enemy_faction_config {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t enemy_faction[32];
	};
        struct pw_elements_table *enemy_faction_config;

	int32_t charracter_class_config_cnt;
	struct charracter_class_config {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t character_class_id;
		int32_t faction;
		int32_t enemy_faction;
		float attack_speed;
		float attack_range;
		int32_t hp_gen;
		int32_t mp_gen;
		float walk_speed;
		float run_speed;
		float swim_speed;
		float fly_speed;
		int32_t crit_rate;
		int32_t vit_hp;
		int32_t eng_mp;
		int32_t agi_attack;
		int32_t agi_armor;
		int32_t lvlup_hp;
		int32_t lvlup_mp;
		float lvlup_dmg;
		float lvlup_magic;
		float lvlup_defense;
		float lvlup_magicdefence;
		int32_t angro_increase;
	};
        struct pw_elements_table *charracter_class_config;

	int32_t param_adjust_config_cnt;
	struct param_adjust_config {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t level_diff_adjust_1_level_diff;
		float level_diff_adjust_1_adjust_exp;
		float level_diff_adjust_1_adjust_sp;
		float level_diff_adjust_1_adjust_money;
		float level_diff_adjust_1_adjust_matter;
		float level_diff_adjust_1_adjust_attack;
		int32_t level_diff_adjust_2_level_diff;
		float level_diff_adjust_2_adjust_exp;
		float level_diff_adjust_2_adjust_sp;
		float level_diff_adjust_2_adjust_money;
		float level_diff_adjust_2_adjust_matter;
		float level_diff_adjust_2_adjust_attack;
		int32_t level_diff_adjust_3_level_diff;
		float level_diff_adjust_3_adjust_exp;
		float level_diff_adjust_3_adjust_sp;
		float level_diff_adjust_3_adjust_money;
		float level_diff_adjust_3_adjust_matter;
		float level_diff_adjust_3_adjust_attack;
		int32_t level_diff_adjust_4_level_diff;
		float level_diff_adjust_4_adjust_exp;
		float level_diff_adjust_4_adjust_sp;
		float level_diff_adjust_4_adjust_money;
		float level_diff_adjust_4_adjust_matter;
		float level_diff_adjust_4_adjust_attack;
		int32_t level_diff_adjust_5_level_diff;
		float level_diff_adjust_5_adjust_exp;
		float level_diff_adjust_5_adjust_sp;
		float level_diff_adjust_5_adjust_money;
		float level_diff_adjust_5_adjust_matter;
		float level_diff_adjust_5_adjust_attack;
		int32_t level_diff_adjust_6_level_diff;
		float level_diff_adjust_6_adjust_exp;
		float level_diff_adjust_6_adjust_sp;
		float level_diff_adjust_6_adjust_money;
		float level_diff_adjust_6_adjust_matter;
		float level_diff_adjust_6_adjust_attack;
		int32_t level_diff_adjust_7_level_diff;
		float level_diff_adjust_7_adjust_exp;
		float level_diff_adjust_7_adjust_sp;
		float level_diff_adjust_7_adjust_money;
		float level_diff_adjust_7_adjust_matter;
		float level_diff_adjust_7_adjust_attack;
		int32_t level_diff_adjust_8_level_diff;
		float level_diff_adjust_8_adjust_exp;
		float level_diff_adjust_8_adjust_sp;
		float level_diff_adjust_8_adjust_money;
		float level_diff_adjust_8_adjust_matter;
		float level_diff_adjust_8_adjust_attack;
		int32_t level_diff_adjust_9_level_diff;
		float level_diff_adjust_9_adjust_exp;
		float level_diff_adjust_9_adjust_sp;
		float level_diff_adjust_9_adjust_money;
		float level_diff_adjust_9_adjust_matter;
		float level_diff_adjust_9_adjust_attack;
		int32_t level_diff_adjust_10_level_diff;
		float level_diff_adjust_10_adjust_exp;
		float level_diff_adjust_10_adjust_sp;
		float level_diff_adjust_10_adjust_money;
		float level_diff_adjust_10_adjust_matter;
		float level_diff_adjust_10_adjust_attack;
		int32_t level_diff_adjust_11_level_diff;
		float level_diff_adjust_11_adjust_exp;
		float level_diff_adjust_11_adjust_sp;
		float level_diff_adjust_11_adjust_money;
		float level_diff_adjust_11_adjust_matter;
		float level_diff_adjust_11_adjust_attack;
		int32_t level_diff_adjust_12_level_diff;
		float level_diff_adjust_12_adjust_exp;
		float level_diff_adjust_12_adjust_sp;
		float level_diff_adjust_12_adjust_money;
		float level_diff_adjust_12_adjust_matter;
		float level_diff_adjust_12_adjust_attack;
		int32_t level_diff_adjust_13_level_diff;
		float level_diff_adjust_13_adjust_exp;
		float level_diff_adjust_13_adjust_sp;
		float level_diff_adjust_13_adjust_money;
		float level_diff_adjust_13_adjust_matter;
		float level_diff_adjust_13_adjust_attack;
		int32_t level_diff_adjust_14_level_diff;
		float level_diff_adjust_14_adjust_exp;
		float level_diff_adjust_14_adjust_sp;
		float level_diff_adjust_14_adjust_money;
		float level_diff_adjust_14_adjust_matter;
		float level_diff_adjust_14_adjust_attack;
		int32_t level_diff_adjust_15_level_diff;
		float level_diff_adjust_15_adjust_exp;
		float level_diff_adjust_15_adjust_sp;
		float level_diff_adjust_15_adjust_money;
		float level_diff_adjust_15_adjust_matter;
		float level_diff_adjust_15_adjust_attack;
		int32_t level_diff_adjust_16_level_diff;
		float level_diff_adjust_16_adjust_exp;
		float level_diff_adjust_16_adjust_sp;
		float level_diff_adjust_16_adjust_money;
		float level_diff_adjust_16_adjust_matter;
		float level_diff_adjust_16_adjust_attack;
		float team_adjust_1_adjust_exp;
		float team_adjust_1_adjust_sp;
		float team_adjust_2_adjust_exp;
		float team_adjust_2_adjust_sp;
		float team_adjust_3_adjust_exp;
		float team_adjust_3_adjust_sp;
		float team_adjust_4_adjust_exp;
		float team_adjust_4_adjust_sp;
		float team_adjust_5_adjust_exp;
		float team_adjust_5_adjust_sp;
		float team_adjust_6_adjust_exp;
		float team_adjust_6_adjust_sp;
		float team_adjust_7_adjust_exp;
		float team_adjust_7_adjust_sp;
		float team_adjust_8_adjust_exp;
		float team_adjust_8_adjust_sp;
		float team_adjust_9_adjust_exp;
		float team_adjust_9_adjust_sp;
		float team_adjust_10_adjust_exp;
		float team_adjust_10_adjust_sp;
		float team_adjust_11_adjust_exp;
		float team_adjust_11_adjust_sp;
		float team_profession_adjust_1_adjust_exp;
		float team_profession_adjust_1_adjust_sp;
		float team_profession_adjust_2_adjust_exp;
		float team_profession_adjust_2_adjust_sp;
		float team_profession_adjust_3_adjust_exp;
		float team_profession_adjust_3_adjust_sp;
		float team_profession_adjust_4_adjust_exp;
		float team_profession_adjust_4_adjust_sp;
		float team_profession_adjust_5_adjust_exp;
		float team_profession_adjust_5_adjust_sp;
		float team_profession_adjust_6_adjust_exp;
		float team_profession_adjust_6_adjust_sp;
		float team_profession_adjust_7_adjust_exp;
		float team_profession_adjust_7_adjust_sp;
		float team_profession_adjust_8_adjust_exp;
		float team_profession_adjust_8_adjust_sp;
		float team_profession_adjust_9_adjust_exp;
		float team_profession_adjust_9_adjust_sp;
	};
        struct pw_elements_table *param_adjust_config;

	int32_t player_action_info_config_cnt;
	struct player_action_info_config {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char action_name[32 / sizeof(char)];
		char action_prefix[32 / sizeof(char)];
		char action_weapon_suffix_1_suffix[32 / sizeof(char)];
		char action_weapon_suffix_2_suffix[32 / sizeof(char)];
		char action_weapon_suffix_3_suffix[32 / sizeof(char)];
		char action_weapon_suffix_4_suffix[32 / sizeof(char)];
		char action_weapon_suffix_5_suffix[32 / sizeof(char)];
		char action_weapon_suffix_6_suffix[32 / sizeof(char)];
		char action_weapon_suffix_7_suffix[32 / sizeof(char)];
		char action_weapon_suffix_8_suffix[32 / sizeof(char)];
		char action_weapon_suffix_9_suffix[32 / sizeof(char)];
		char action_weapon_suffix_10_suffix[32 / sizeof(char)];
		char action_weapon_suffix_11_suffix[32 / sizeof(char)];
		int32_t hide_weapon;
	};
        struct pw_elements_table *player_action_info_config;

	int32_t taskdice_essence_cnt;
	struct taskdice_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
        struct {
            int32_t id;
            float prob;
        } tasks[8];
		int32_t use_on_pick;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *taskdice_essence;

	int32_t tasknormalmatter_essence_cnt;
	struct tasknormalmatter_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *tasknormalmatter_essence;

	int32_t face_faling_essence_cnt;
	struct face_faling_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_faling_skin[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t character_combo_id;
		int32_t gender_id;
		int32_t visualize_id;
		int32_t facepill_only;
	};
        struct pw_elements_table *face_faling_essence;

	int32_t player_levelexp_config_cnt;
	struct player_levelexp_config {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t exp[150];
	};
        struct pw_elements_table *player_levelexp_config;

	int32_t mine_type_cnt;
	struct mine_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *mine_type;

	int32_t mines_cnt;
	struct mines {
		int32_t id;
		int32_t id_type;
		char16_t name[64 / sizeof(char16_t)];
		int32_t level;
		int32_t level_required;
		int32_t item_required;
		int32_t use_up_tool;
		int32_t time_min;
		int32_t time_max;
		int32_t exp;
		int32_t sp;
		char file_model[128 / sizeof(char)];
        struct {
            int32_t id;
            float prob;
        } mat_item[16];
        struct {
            int32_t num;
            float prob;
		} mat_count[2];
		int32_t task_in;
		int32_t task_out;
		int32_t uninterruptible;
		struct {
			int32_t mob_id;
			int32_t num;
			float radius;
		} npcgen[4];
		int32_t aggro_monster_faction;
		float aggro_radius;
		int32_t aggro_num;
		int32_t permanent;
	};
        struct pw_elements_table *mines;

	int32_t npc_identify_service_cnt;
	struct npc_identify_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t fee;
	};
        struct pw_elements_table *npc_identify_service;

	int32_t fashion_major_type_cnt;
	struct fashion_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *fashion_major_type;

	int32_t fashion_sub_type_cnt;
	struct fashion_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t equip_fashion_mask;
	};
        struct pw_elements_table *fashion_sub_type;

	int32_t fashion_essence_cnt;
	struct fashion_essence {
		int32_t id;
		int32_t id_major_type;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char realname[32 / sizeof(char)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t equip_location;
		int32_t level;
		int32_t require_level;
		int32_t require_dye_count;
		int32_t price;
		int32_t shop_price;
		int32_t gender;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *fashion_essence;

	int32_t faceticket_major_type_cnt;
	struct faceticket_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *faceticket_major_type;

	int32_t faceticket_sub_type_cnt;
	struct faceticket_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *faceticket_sub_type;

	int32_t faceticket_essence_cnt;
	struct faceticket_essence {
		int32_t id;
		int32_t id_major_type;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t require_level;
		char bound_file[128 / sizeof(char)];
		int32_t unsymmetrical;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *faceticket_essence;

	int32_t facepill_major_type_cnt;
	struct facepill_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *facepill_major_type;

	int32_t facepill_sub_type_cnt;
	struct facepill_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *facepill_sub_type;

	int32_t facepill_essence_cnt;
	struct facepill_essence {
		int32_t id;
		int32_t id_major_type;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t duration;
		float camera_scale;
		int32_t character_combo_id;
		char pllfiles_1_file[128 / sizeof(char)];
		char pllfiles_2_file[128 / sizeof(char)];
		char pllfiles_3_file[128 / sizeof(char)];
		char pllfiles_4_file[128 / sizeof(char)];
		char pllfiles_5_file[128 / sizeof(char)];
		char pllfiles_6_file[128 / sizeof(char)];
		char pllfiles_7_file[128 / sizeof(char)];
		char pllfiles_8_file[128 / sizeof(char)];
		char pllfiles_9_file[128 / sizeof(char)];
		char pllfiles_10_file[128 / sizeof(char)];
		char pllfiles_11_file[128 / sizeof(char)];
		char pllfiles_12_file[128 / sizeof(char)];
		char pllfiles_13_file[128 / sizeof(char)];
		char pllfiles_14_file[128 / sizeof(char)];
		char pllfiles_15_file[128 / sizeof(char)];
		char pllfiles_16_file[128 / sizeof(char)];
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *facepill_essence;

	int32_t armor_sets_cnt;
	struct armor_sets {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t max_equips;
		int32_t equipments_1_id;
		int32_t equipments_2_id;
		int32_t equipments_3_id;
		int32_t equipments_4_id;
		int32_t equipments_5_id;
		int32_t equipments_6_id;
		int32_t equipments_7_id;
		int32_t equipments_8_id;
		int32_t equipments_9_id;
		int32_t equipments_10_id;
		int32_t equipments_11_id;
		int32_t equipments_12_id;
		int32_t addons_1_id;
		int32_t addons_2_id;
		int32_t addons_3_id;
		int32_t addons_4_id;
		int32_t addons_5_id;
		int32_t addons_6_id;
		int32_t addons_7_id;
		int32_t addons_8_id;
		int32_t addons_9_id;
		int32_t addons_10_id;
		int32_t addons_11_id;
		char file_gfx[128 / sizeof(char)];
	};
        struct pw_elements_table *armor_sets;

	int32_t gm_generator_type_cnt;
	struct gm_generator_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *gm_generator_type;

	int32_t gm_generator_essence_cnt;
	struct gm_generator_essence {
		int32_t id;
		int32_t id_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t id_object;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *gm_generator_essence;

	int32_t pet_type_cnt;
	struct pet_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	};
        struct pw_elements_table *pet_type;

	int32_t pet_essence_cnt;
	struct pet_essence {
		int32_t id;
		int32_t id_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_model[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t character_combo_id;
		int32_t level_max;
		int32_t level_require;
		int32_t pet_snd_type;
		float hp_a;
		float hp_b;
		float hp_c;
		float hp_gen_a;
		float hp_gen_b;
		float hp_gen_c;
		float damage_a;
		float damage_b;
		float damage_c;
		float damage_d;
		float speed_a;
		float speed_b;
		float attack_a;
		float attack_b;
		float attack_c;
		float armor_a;
		float armor_b;
		float armor_c;
		float physic_defence_a;
		float physic_defence_b;
		float physic_defence_c;
		float physic_defence_d;
		float magic_defence_a;
		float magic_defence_b;
		float magic_defence_c;
		float magic_defence_d;
		float size;
		float damage_delay;
		float attack_range;
		float attack_speed;
		int32_t sight_range;
		int32_t food_mask;
		int32_t inhabit_type;
		int32_t stand_mode;
	};
        struct pw_elements_table *pet_essence;

	int32_t pet_egg_essence_cnt;
	struct pet_egg_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t id_pet;
		int32_t money_hatched;
		int32_t money_restored;
		int32_t honor_point;
		int32_t level;
		int32_t exp;
		int32_t skill_point;
		struct {
			int32_t id;
			int32_t level;
		} skills[32];
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *pet_egg_essence;

	int32_t pet_food_essence_cnt;
	struct pet_food_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t level;
		int32_t hornor;
		int32_t exp;
		int32_t food_type;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *pet_food_essence;

	int32_t pet_faceticket_essence_cnt;
	struct pet_faceticket_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *pet_faceticket_essence;

	int32_t fireworks_essence_cnt;
	struct fireworks_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		char file_fw[128 / sizeof(char)];
		int32_t level;
		int32_t time_to_fire;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *fireworks_essence;

	int32_t war_tankcallin_essence_cnt;
	struct war_tankcallin_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *war_tankcallin_essence;

	int32_t npc_war_towerbuild_service_cnt;
	struct npc_war_towerbuild_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t build_info_1_id_in_build;
		int32_t build_info_1_id_buildup;
		int32_t build_info_1_id_object_need;
		int32_t build_info_1_time_use;
		int32_t build_info_1_fee;
		int32_t build_info_2_id_in_build;
		int32_t build_info_2_id_buildup;
		int32_t build_info_2_id_object_need;
		int32_t build_info_2_time_use;
		int32_t build_info_2_fee;
		int32_t build_info_3_id_in_build;
		int32_t build_info_3_id_buildup;
		int32_t build_info_3_id_object_need;
		int32_t build_info_3_time_use;
		int32_t build_info_3_fee;
		int32_t build_info_4_id_in_build;
		int32_t build_info_4_id_buildup;
		int32_t build_info_4_id_object_need;
		int32_t build_info_4_time_use;
		int32_t build_info_4_fee;
	};
        struct pw_elements_table *npc_war_towerbuild_service;

	int32_t player_secondlevel_config_cnt;
	struct player_secondlevel_config {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		float exp_lost[256];
	};
        struct pw_elements_table *player_secondlevel_config;

	int32_t npc_resetprop_service_cnt;
	struct npc_resetprop_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t prop_entry_1_id_object_need;
		int32_t prop_entry_1_strength_delta;
		int32_t prop_entry_1_agility_delta;
		int32_t prop_entry_1_vital_delta;
		int32_t prop_entry_1_energy_delta;
		int32_t prop_entry_2_id_object_need;
		int32_t prop_entry_2_strength_delta;
		int32_t prop_entry_2_agility_delta;
		int32_t prop_entry_2_vital_delta;
		int32_t prop_entry_2_energy_delta;
		int32_t prop_entry_3_id_object_need;
		int32_t prop_entry_3_strength_delta;
		int32_t prop_entry_3_agility_delta;
		int32_t prop_entry_3_vital_delta;
		int32_t prop_entry_3_energy_delta;
		int32_t prop_entry_4_id_object_need;
		int32_t prop_entry_4_strength_delta;
		int32_t prop_entry_4_agility_delta;
		int32_t prop_entry_4_vital_delta;
		int32_t prop_entry_4_energy_delta;
		int32_t prop_entry_5_id_object_need;
		int32_t prop_entry_5_strength_delta;
		int32_t prop_entry_5_agility_delta;
		int32_t prop_entry_5_vital_delta;
		int32_t prop_entry_5_energy_delta;
		int32_t prop_entry_6_id_object_need;
		int32_t prop_entry_6_strength_delta;
		int32_t prop_entry_6_agility_delta;
		int32_t prop_entry_6_vital_delta;
		int32_t prop_entry_6_energy_delta;
		int32_t prop_entry_7_id_object_need;
		int32_t prop_entry_7_strength_delta;
		int32_t prop_entry_7_agility_delta;
		int32_t prop_entry_7_vital_delta;
		int32_t prop_entry_7_energy_delta;
		int32_t prop_entry_8_id_object_need;
		int32_t prop_entry_8_strength_delta;
		int32_t prop_entry_8_agility_delta;
		int32_t prop_entry_8_vital_delta;
		int32_t prop_entry_8_energy_delta;
		int32_t prop_entry_9_id_object_need;
		int32_t prop_entry_9_strength_delta;
		int32_t prop_entry_9_agility_delta;
		int32_t prop_entry_9_vital_delta;
		int32_t prop_entry_9_energy_delta;
		int32_t prop_entry_10_id_object_need;
		int32_t prop_entry_10_strength_delta;
		int32_t prop_entry_10_agility_delta;
		int32_t prop_entry_10_vital_delta;
		int32_t prop_entry_10_energy_delta;
		int32_t prop_entry_11_id_object_need;
		int32_t prop_entry_11_strength_delta;
		int32_t prop_entry_11_agility_delta;
		int32_t prop_entry_11_vital_delta;
		int32_t prop_entry_11_energy_delta;
		int32_t prop_entry_12_id_object_need;
		int32_t prop_entry_12_strength_delta;
		int32_t prop_entry_12_agility_delta;
		int32_t prop_entry_12_vital_delta;
		int32_t prop_entry_12_energy_delta;
		int32_t prop_entry_13_id_object_need;
		int32_t prop_entry_13_strength_delta;
		int32_t prop_entry_13_agility_delta;
		int32_t prop_entry_13_vital_delta;
		int32_t prop_entry_13_energy_delta;
		int32_t prop_entry_14_id_object_need;
		int32_t prop_entry_14_strength_delta;
		int32_t prop_entry_14_agility_delta;
		int32_t prop_entry_14_vital_delta;
		int32_t prop_entry_14_energy_delta;
		int32_t prop_entry_15_id_object_need;
		int32_t prop_entry_15_strength_delta;
		int32_t prop_entry_15_agility_delta;
		int32_t prop_entry_15_vital_delta;
		int32_t prop_entry_15_energy_delta;
	};
        struct pw_elements_table *npc_resetprop_service;

	int32_t npc_petname_service_cnt;
	struct npc_petname_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_object_need;
		int32_t price;
	};
        struct pw_elements_table *npc_petname_service;

	int32_t npc_petlearnskill_service_cnt;
	struct npc_petlearnskill_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_skills[128];
		int32_t id_dialog;
	};
        struct pw_elements_table *npc_petlearnskill_service;

	int32_t npc_petforgetskill_service_cnt;
	struct npc_petforgetskill_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_object_need;
		int32_t price;
	};
        struct pw_elements_table *npc_petforgetskill_service;

	int32_t skillmatter_essence_cnt;
	struct skillmatter_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t level_required;
		int32_t id_skill;
		int32_t level_skill;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *skillmatter_essence;

	int32_t refine_ticket_essence_cnt;
	struct refine_ticket_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		char16_t desc[32 / sizeof(char16_t)];
		float ext_reserved_prob;
		float ext_succeed_prob;
		int32_t fail_reserve_level;
		float fail_ext_succeed_prob_1;
		float fail_ext_succeed_prob_2;
		float fail_ext_succeed_prob_3;
		float fail_ext_succeed_prob_4;
		float fail_ext_succeed_prob_5;
		float fail_ext_succeed_prob_6;
		float fail_ext_succeed_prob_7;
		float fail_ext_succeed_prob_8;
		float fail_ext_succeed_prob_9;
		float fail_ext_succeed_prob_10;
		float fail_ext_succeed_prob_11;
		float fail_ext_succeed_prob_12;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *refine_ticket_essence;

	int32_t destroying_essence_cnt;
	struct destroying_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *destroying_essence;

	int32_t npc_equipbind_service_cnt;
	struct npc_equipbind_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_object_need;
		int32_t price;
	};
        struct pw_elements_table *npc_equipbind_service;

	int32_t npc_equipdestroy_service_cnt;
	struct npc_equipdestroy_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_object_need;
		int32_t price;
	};
        struct pw_elements_table *npc_equipdestroy_service;

	int32_t npc_equipundestroy_service_cnt;
	struct npc_equipundestroy_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_object_need;
		int32_t price;
	};
        struct pw_elements_table *npc_equipundestroy_service;

	int32_t bible_essence_cnt;
	struct bible_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t id_addons_1;
		int32_t id_addons_2;
		int32_t id_addons_3;
		int32_t id_addons_4;
		int32_t id_addons_5;
		int32_t id_addons_6;
		int32_t id_addons_7;
		int32_t id_addons_8;
		int32_t id_addons_9;
		int32_t id_addons_10;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *bible_essence;

	int32_t speaker_essence_cnt;
	struct speaker_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t id_icon_set;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *speaker_essence;

	int32_t autohp_essence_cnt;
	struct autohp_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t total_hp;
		float trigger_amount;
		int32_t cool_time;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *autohp_essence;

	int32_t automp_essence_cnt;
	struct automp_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t total_mp;
		float trigger_amount;
		int32_t cool_time;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *automp_essence;

	int32_t double_exp_essence_cnt;
	struct double_exp_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t double_exp_time;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *double_exp_essence;

	int32_t transmitscroll_essence_cnt;
	struct transmitscroll_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *transmitscroll_essence;

	int32_t dye_ticket_essence_cnt;
	struct dye_ticket_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		float h_min;
		float h_max;
		float s_min;
		float s_max;
		float v_min;
		float v_max;
		int32_t price;
		int32_t shop_price;
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	};
        struct pw_elements_table *dye_ticket_essence;

	struct pw_elements_table *tables[256];
	size_t tables_count;
};

#endif /* PW_ELEMENTS_H */
