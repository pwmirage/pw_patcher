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

int pw_elements_load(struct pw_elements *el, const char *filename);
void pw_elements_serialize(struct pw_elements *elements);
int pw_elements_patch_obj(struct pw_elements *elements, struct cjson *obj);

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
	} *talk_proc;

	int32_t equipment_addon_cnt;
	struct equipment_addon {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t num_params;
		int32_t param1;
		int32_t param2;
		int32_t param3;
	} *equipment_addon;

	int32_t weapon_major_type_cnt;
	int32_t weapon_major_type_ext_cnt;
	struct weapon_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *weapon_major_type;

	int32_t weapon_sub_type_cnt;
	int32_t weapon_sub_type_ext_cnt;
	struct weapon_sub_type {
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
	} *weapon_sub_type;

	int32_t weapon_essence_cnt;
	int32_t weapon_essence_ext_cnt;
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
	} *weapon_essence;

	int32_t armor_major_type_cnt;
	int32_t armor_major_type_ext_cnt;
	struct armor_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *armor_major_type;

	int32_t armor_sub_type_cnt;
	int32_t armor_sub_type_ext_cnt;
	struct armor_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t equip_mask;
	} *armor_sub_type;

	int32_t armor_essence_cnt;
	int32_t armor_essence_ext_cnt;
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
	} *armor_essence;

	int32_t decoration_major_type_cnt;
	int32_t decoration_major_type_ext_cnt;
	struct decoration_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *decoration_major_type;

	int32_t decoration_sub_type_cnt;
	int32_t decoration_sub_type_ext_cnt;
	struct decoration_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t equip_mask;
	} *decoration_sub_type;

	int32_t decoration_essence_cnt;
	int32_t decoration_essence_ext_cnt;
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
	} *decoration_essence;

	int32_t medicine_major_type_cnt;
	int32_t medicine_major_type_ext_cnt;
	struct medicine_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *medicine_major_type;

	int32_t medicine_sub_type_cnt;
	int32_t medicine_sub_type_ext_cnt;
	struct medicine_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *medicine_sub_type;

	int32_t medicine_essence_cnt;
	int32_t medicine_essence_ext_cnt;
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
	} *medicine_essence;

	int32_t material_major_type_cnt;
	int32_t material_major_type_ext_cnt;
	struct material_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *material_major_type;

	int32_t material_sub_type_cnt;
	int32_t material_sub_type_ext_cnt;
	struct material_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *material_sub_type;

	int32_t material_essence_cnt;
	int32_t material_essence_ext_cnt;
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
	} *material_essence;

	int32_t damagerune_sub_type_cnt;
	int32_t damagerune_sub_type_ext_cnt;
	struct damagerune_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *damagerune_sub_type;

	int32_t damagerune_essence_cnt;
	int32_t damagerune_essence_ext_cnt;
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
	} *damagerune_essence;

	int32_t armorrune_sub_type_cnt;
	int32_t armorrune_sub_type_ext_cnt;
	struct armorrune_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *armorrune_sub_type;

	int32_t armorrune_essence_cnt;
	int32_t armorrune_essence_ext_cnt;
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
	} *armorrune_essence;

	int32_t skilltome_sub_type_cnt;
	int32_t skilltome_sub_type_ext_cnt;
	struct skilltome_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *skilltome_sub_type;

	int32_t skilltome_essence_cnt;
	int32_t skilltome_essence_ext_cnt;
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
	} *skilltome_essence;

	int32_t flysword_essence_cnt;
	int32_t flysword_essence_ext_cnt;
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
	} *flysword_essence;

	int32_t wingmanwing_essence_cnt;
	int32_t wingmanwing_essence_ext_cnt;
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
	} *wingmanwing_essence;

	int32_t townscroll_essence_cnt;
	int32_t townscroll_essence_ext_cnt;
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
	} *townscroll_essence;

	int32_t unionscroll_essence_cnt;
	int32_t unionscroll_essence_ext_cnt;
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
	} *unionscroll_essence;

	int32_t revivescroll_essence_cnt;
	int32_t revivescroll_essence_ext_cnt;
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
	} *revivescroll_essence;

	int32_t element_essence_cnt;
	int32_t element_essence_ext_cnt;
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
	} *element_essence;

	int32_t taskmatter_essence_cnt;
	int32_t taskmatter_essence_ext_cnt;
	struct taskmatter_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_icon[128 / sizeof(char)];
		int32_t pile_num_max;
		int32_t has_guid;
		int32_t proc_type;
	} *taskmatter_essence;

	int32_t tossmatter_essence_cnt;
	int32_t tossmatter_essence_ext_cnt;
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
	} *tossmatter_essence;

	int32_t projectile_type_cnt;
	int32_t projectile_type_ext_cnt;
	struct projectile_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *projectile_type;

	int32_t projectile_essence_cnt;
	int32_t projectile_essence_ext_cnt;
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
	} *projectile_essence;

	int32_t quiver_sub_type_cnt;
	int32_t quiver_sub_type_ext_cnt;
	struct quiver_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *quiver_sub_type;

	int32_t quiver_essence_cnt;
	int32_t quiver_essence_ext_cnt;
	struct quiver_essence {
		int32_t id;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		char file_matter[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t id_projectile;
		int32_t num_min;
		int32_t num_max;
	} *quiver_essence;

	int32_t stone_sub_type_cnt;
	int32_t stone_sub_type_ext_cnt;
	struct stone_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *stone_sub_type;

	int32_t stone_essence_cnt;
	int32_t stone_essence_ext_cnt;
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
	} *stone_essence;

	int32_t monster_addon_cnt;
	int32_t monster_addon_ext_cnt;
	struct monster_addon {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t num_params;
		int32_t param1;
		int32_t param2;
		int32_t param3;
	} *monster_addon;

	int32_t monster_type_cnt;
	int32_t monster_type_ext_cnt;
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
	} *monster_type;

	int32_t monster_essence_cnt;
	int32_t monster_essence_ext_cnt;
	struct monster_essence {
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
	} *monster_essence;

	int32_t npc_talk_service_cnt;
	int32_t npc_talk_service_ext_cnt;
	struct npc_talk_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_dialog;
	} *npc_talk_service;

	int32_t npc_sell_service_cnt;
	int32_t npc_sell_service_ext_cnt;
	struct npc_sell_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		struct npc_sell_service_page {
			char16_t page_title[16 / sizeof(char16_t)];
			int32_t id_goods[32];
		} pages[8];
		int32_t id_dialog;
	} *npc_sell_service;

	int32_t npc_buy_service_cnt;
	int32_t npc_buy_service_ext_cnt;
	struct npc_buy_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_dialog;
	} *npc_buy_service;

	int32_t npc_repair_service_cnt;
	int32_t npc_repair_service_ext_cnt;
	struct npc_repair_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_dialog;
	} *npc_repair_service;

	int32_t npc_install_service_cnt;
	int32_t npc_install_service_ext_cnt;
	struct npc_install_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t _unused1[32];
		int32_t _unused2;
	} *npc_install_service;

	int32_t npc_uninstall_service_cnt;
	int32_t npc_uninstall_service_ext_cnt;
	struct npc_uninstall_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t _unused1[32];
		int32_t _unused2;
	} *npc_uninstall_service;

	int32_t npc_task_in_service_cnt;
	int32_t npc_task_in_service_ext_cnt;
	struct npc_task_in_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t task_id[32];
	} *npc_task_in_service;

	int32_t npc_task_out_service_cnt;
	int32_t npc_task_out_service_ext_cnt;
	struct npc_task_out_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t task_id[32];
	} *npc_task_out_service;

	int32_t npc_task_matter_service_cnt;
	int32_t npc_task_matter_service_ext_cnt;
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
	} *npc_task_matter_service;

	int32_t npc_skill_service_cnt;
	int32_t npc_skill_service_ext_cnt;
	struct npc_skill_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t skill_id[128];
		int32_t id_dialog;
	} *npc_skill_service;

	int32_t npc_heal_service_cnt;
	int32_t npc_heal_service_ext_cnt;
	struct npc_heal_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_dialog;
	} *npc_heal_service;

	int32_t npc_transmit_service_cnt;
	int32_t npc_transmit_service_ext_cnt;
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
	} *npc_transmit_service;

	int32_t npc_transport_service_cnt;
	int32_t npc_transport_service_ext_cnt;
	struct npc_transport_service { /* unused */
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		struct {
			int32_t id;
			int32_t fee;
		} route[32];
		int32_t id_dialog;
	} *npc_transport_service;

	int32_t npc_proxy_service_cnt;
	int32_t npc_proxy_service_ext_cnt;
	struct npc_proxy_service { /* unused */
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_dialog;
	} *npc_proxy_service;

	int32_t npc_storage_service_cnt;
	int32_t npc_storage_service_ext_cnt;
	struct npc_storage_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *npc_storage_service;

	int32_t npc_make_service_cnt;
	int32_t npc_make_service_ext_cnt;
	struct npc_make_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_make_skill;
		int32_t produce_type;
		struct npc_make_service_page {
			char16_t page_title[16 / sizeof(char16_t)];
			int32_t item_id[32];
		} pages[8];
	} *npc_make_service;

	int32_t npc_decompose_service_cnt;
	int32_t npc_decompose_service_ext_cnt;
	struct npc_decompose_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_decompose_skill;
	} *npc_decompose_service;

	int32_t npc_type_cnt;
	int32_t npc_type_ext_cnt;
	struct npc_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *npc_type;

	int32_t npc_essence_cnt;
	int32_t npc_essence_ext_cnt;
	struct npc_essence {
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
	} *npc_essence;

	int32_t talk_proc_cnt;
	int32_t talk_proc_ext_cnt;
	int32_t face_texture_essence_cnt;
	int32_t face_texture_essence_ext_cnt;
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
	} *face_texture_essence;

	int32_t face_shape_essence_cnt;
	int32_t face_shape_essence_ext_cnt;
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
	} *face_shape_essence;

	int32_t face_emotion_type_cnt;
	int32_t face_emotion_type_ext_cnt;
	struct face_emotion_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_icon[128 / sizeof(char)];
	} *face_emotion_type;

	int32_t face_expression_essence_cnt;
	int32_t face_expression_essence_ext_cnt;
	struct face_expression_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_expression[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t character_combo_id;
		int32_t gender_id;
		int32_t emotion_id;
	} *face_expression_essence;

	int32_t face_hair_essence_cnt;
	int32_t face_hair_essence_ext_cnt;
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
	} *face_hair_essence;

	int32_t face_moustache_essence_cnt;
	int32_t face_moustache_essence_ext_cnt;
	struct face_moustache_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_moustache_skin[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t character_combo_id;
		int32_t gender_id;
		int32_t visualize_id;
		int32_t facepill_only;
	} *face_moustache_essence;

	int32_t colorpicker_essence_cnt;
	int32_t colorpicker_essence_ext_cnt;
	struct colorpicker_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_colorpicker[128 / sizeof(char)];
		int32_t color_part_id;
		int32_t character_combo_id;
		int32_t gender_id;
	} *colorpicker_essence;

	int32_t customizedata_essence_cnt;
	int32_t customizedata_essence_ext_cnt;
	struct customizedata_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_data[128 / sizeof(char)];
		int32_t character_combo_id;
		int32_t gender_id;
	} *customizedata_essence;

	int32_t recipe_major_type_cnt;
	int32_t recipe_major_type_ext_cnt;
	struct recipe_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *recipe_major_type;

	int32_t recipe_sub_type_cnt;
	int32_t recipe_sub_type_ext_cnt;
	struct recipe_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *recipe_sub_type;

	int32_t recipe_essence_cnt;
	int32_t recipe_essence_ext_cnt;
	struct recipe_essence {
		int32_t id;
		int32_t id_major_type;
		int32_t id_sub_type;
		char16_t name[64 / sizeof(char16_t)];
		int32_t recipe_level;
		int32_t skill_id;
		int32_t skill_level;
		int32_t bind_type;
		struct recipe_essence_target {
			int32_t id;
			float prob;
		} targets[4];
		float fail_prob;
		int32_t num_to_make;
		int32_t coins;
		float duration;
		int32_t xp;
		int32_t sp;
		struct recipe_essence_material {
			int32_t id;
			int32_t num;
		} mats[32];
	} *recipe_essence;

	int32_t enemy_faction_config_cnt;
	int32_t enemy_faction_config_ext_cnt;
	struct enemy_faction_config {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t enemy_faction[32];
	} *enemy_faction_config;

	int32_t charracter_class_config_cnt;
	int32_t charracter_class_config_ext_cnt;
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
	} *charracter_class_config;

	int32_t param_adjust_config_cnt;
	int32_t param_adjust_config_ext_cnt;
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
	} *param_adjust_config;

	int32_t player_action_info_config_cnt;
	int32_t player_action_info_config_ext_cnt;
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
	} *player_action_info_config;

	int32_t taskdice_essence_cnt;
	int32_t taskdice_essence_ext_cnt;
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
	} *taskdice_essence;

	int32_t tasknormalmatter_essence_cnt;
	int32_t tasknormalmatter_essence_ext_cnt;
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
	} *tasknormalmatter_essence;

	int32_t face_faling_essence_cnt;
	int32_t face_faling_essence_ext_cnt;
	struct face_faling_essence {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		char file_faling_skin[128 / sizeof(char)];
		char file_icon[128 / sizeof(char)];
		int32_t character_combo_id;
		int32_t gender_id;
		int32_t visualize_id;
		int32_t facepill_only;
	} *face_faling_essence;

	int32_t player_levelexp_config_cnt;
	int32_t player_levelexp_config_ext_cnt;
	struct player_levelexp_config {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t exp[150];
	} *player_levelexp_config;

	int32_t mine_type_cnt;
	int32_t mine_type_ext_cnt;
	struct mine_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *mine_type;

	int32_t mine_essence_cnt;
	int32_t mine_essence_ext_cnt;
	struct mine_essence {
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
	} *mine_essence;

	int32_t npc_identify_service_cnt;
	int32_t npc_identify_service_ext_cnt;
	struct npc_identify_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t fee;
	} *npc_identify_service;

	int32_t fashion_major_type_cnt;
	int32_t fashion_major_type_ext_cnt;
	struct fashion_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *fashion_major_type;

	int32_t fashion_sub_type_cnt;
	int32_t fashion_sub_type_ext_cnt;
	struct fashion_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t equip_fashion_mask;
	} *fashion_sub_type;

	int32_t fashion_essence_cnt;
	int32_t fashion_essence_ext_cnt;
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
	} *fashion_essence;

	int32_t faceticket_major_type_cnt;
	int32_t faceticket_major_type_ext_cnt;
	struct faceticket_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *faceticket_major_type;

	int32_t faceticket_sub_type_cnt;
	int32_t faceticket_sub_type_ext_cnt;
	struct faceticket_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *faceticket_sub_type;

	int32_t faceticket_essence_cnt;
	int32_t faceticket_essence_ext_cnt;
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
	} *faceticket_essence;

	int32_t facepill_major_type_cnt;
	int32_t facepill_major_type_ext_cnt;
	struct facepill_major_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *facepill_major_type;

	int32_t facepill_sub_type_cnt;
	int32_t facepill_sub_type_ext_cnt;
	struct facepill_sub_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *facepill_sub_type;

	int32_t facepill_essence_cnt;
	int32_t facepill_essence_ext_cnt;
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
	} *facepill_essence;

	int32_t suite_essence_cnt;
	int32_t suite_essence_ext_cnt;
	struct suite_essence {
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
	} *suite_essence;

	int32_t gm_generator_type_cnt;
	int32_t gm_generator_type_ext_cnt;
	struct gm_generator_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *gm_generator_type;

	int32_t gm_generator_essence_cnt;
	int32_t gm_generator_essence_ext_cnt;
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
	} *gm_generator_essence;

	int32_t pet_type_cnt;
	int32_t pet_type_ext_cnt;
	struct pet_type {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
	} *pet_type;

	int32_t pet_essence_cnt;
	int32_t pet_essence_ext_cnt;
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
	} *pet_essence;

	int32_t pet_egg_essence_cnt;
	int32_t pet_egg_essence_ext_cnt;
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
	} *pet_egg_essence;

	int32_t pet_food_essence_cnt;
	int32_t pet_food_essence_ext_cnt;
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
	} *pet_food_essence;

	int32_t pet_faceticket_essence_cnt;
	int32_t pet_faceticket_essence_ext_cnt;
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
	} *pet_faceticket_essence;

	int32_t fireworks_essence_cnt;
	int32_t fireworks_essence_ext_cnt;
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
	} *fireworks_essence;

	int32_t war_tankcallin_essence_cnt;
	int32_t war_tankcallin_essence_ext_cnt;
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
	} *war_tankcallin_essence;

	int32_t npc_war_towerbuild_service_cnt;
	int32_t npc_war_towerbuild_service_ext_cnt;
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
	} *npc_war_towerbuild_service;

	int32_t player_secondlevel_config_cnt;
	int32_t player_secondlevel_config_ext_cnt;
	struct player_secondlevel_config {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		float exp_lost_1;
		float exp_lost_2;
		float exp_lost_3;
		float exp_lost_4;
		float exp_lost_5;
		float exp_lost_6;
		float exp_lost_7;
		float exp_lost_8;
		float exp_lost_9;
		float exp_lost_10;
		float exp_lost_11;
		float exp_lost_12;
		float exp_lost_13;
		float exp_lost_14;
		float exp_lost_15;
		float exp_lost_16;
		float exp_lost_17;
		float exp_lost_18;
		float exp_lost_19;
		float exp_lost_20;
		float exp_lost_21;
		float exp_lost_22;
		float exp_lost_23;
		float exp_lost_24;
		float exp_lost_25;
		float exp_lost_26;
		float exp_lost_27;
		float exp_lost_28;
		float exp_lost_29;
		float exp_lost_30;
		float exp_lost_31;
		float exp_lost_32;
		float exp_lost_33;
		float exp_lost_34;
		float exp_lost_35;
		float exp_lost_36;
		float exp_lost_37;
		float exp_lost_38;
		float exp_lost_39;
		float exp_lost_40;
		float exp_lost_41;
		float exp_lost_42;
		float exp_lost_43;
		float exp_lost_44;
		float exp_lost_45;
		float exp_lost_46;
		float exp_lost_47;
		float exp_lost_48;
		float exp_lost_49;
		float exp_lost_50;
		float exp_lost_51;
		float exp_lost_52;
		float exp_lost_53;
		float exp_lost_54;
		float exp_lost_55;
		float exp_lost_56;
		float exp_lost_57;
		float exp_lost_58;
		float exp_lost_59;
		float exp_lost_60;
		float exp_lost_61;
		float exp_lost_62;
		float exp_lost_63;
		float exp_lost_64;
		float exp_lost_65;
		float exp_lost_66;
		float exp_lost_67;
		float exp_lost_68;
		float exp_lost_69;
		float exp_lost_70;
		float exp_lost_71;
		float exp_lost_72;
		float exp_lost_73;
		float exp_lost_74;
		float exp_lost_75;
		float exp_lost_76;
		float exp_lost_77;
		float exp_lost_78;
		float exp_lost_79;
		float exp_lost_80;
		float exp_lost_81;
		float exp_lost_82;
		float exp_lost_83;
		float exp_lost_84;
		float exp_lost_85;
		float exp_lost_86;
		float exp_lost_87;
		float exp_lost_88;
		float exp_lost_89;
		float exp_lost_90;
		float exp_lost_91;
		float exp_lost_92;
		float exp_lost_93;
		float exp_lost_94;
		float exp_lost_95;
		float exp_lost_96;
		float exp_lost_97;
		float exp_lost_98;
		float exp_lost_99;
		float exp_lost_100;
		float exp_lost_101;
		float exp_lost_102;
		float exp_lost_103;
		float exp_lost_104;
		float exp_lost_105;
		float exp_lost_106;
		float exp_lost_107;
		float exp_lost_108;
		float exp_lost_109;
		float exp_lost_110;
		float exp_lost_111;
		float exp_lost_112;
		float exp_lost_113;
		float exp_lost_114;
		float exp_lost_115;
		float exp_lost_116;
		float exp_lost_117;
		float exp_lost_118;
		float exp_lost_119;
		float exp_lost_120;
		float exp_lost_121;
		float exp_lost_122;
		float exp_lost_123;
		float exp_lost_124;
		float exp_lost_125;
		float exp_lost_126;
		float exp_lost_127;
		float exp_lost_128;
		float exp_lost_129;
		float exp_lost_130;
		float exp_lost_131;
		float exp_lost_132;
		float exp_lost_133;
		float exp_lost_134;
		float exp_lost_135;
		float exp_lost_136;
		float exp_lost_137;
		float exp_lost_138;
		float exp_lost_139;
		float exp_lost_140;
		float exp_lost_141;
		float exp_lost_142;
		float exp_lost_143;
		float exp_lost_144;
		float exp_lost_145;
		float exp_lost_146;
		float exp_lost_147;
		float exp_lost_148;
		float exp_lost_149;
		float exp_lost_150;
		float exp_lost_151;
		float exp_lost_152;
		float exp_lost_153;
		float exp_lost_154;
		float exp_lost_155;
		float exp_lost_156;
		float exp_lost_157;
		float exp_lost_158;
		float exp_lost_159;
		float exp_lost_160;
		float exp_lost_161;
		float exp_lost_162;
		float exp_lost_163;
		float exp_lost_164;
		float exp_lost_165;
		float exp_lost_166;
		float exp_lost_167;
		float exp_lost_168;
		float exp_lost_169;
		float exp_lost_170;
		float exp_lost_171;
		float exp_lost_172;
		float exp_lost_173;
		float exp_lost_174;
		float exp_lost_175;
		float exp_lost_176;
		float exp_lost_177;
		float exp_lost_178;
		float exp_lost_179;
		float exp_lost_180;
		float exp_lost_181;
		float exp_lost_182;
		float exp_lost_183;
		float exp_lost_184;
		float exp_lost_185;
		float exp_lost_186;
		float exp_lost_187;
		float exp_lost_188;
		float exp_lost_189;
		float exp_lost_190;
		float exp_lost_191;
		float exp_lost_192;
		float exp_lost_193;
		float exp_lost_194;
		float exp_lost_195;
		float exp_lost_196;
		float exp_lost_197;
		float exp_lost_198;
		float exp_lost_199;
		float exp_lost_200;
		float exp_lost_201;
		float exp_lost_202;
		float exp_lost_203;
		float exp_lost_204;
		float exp_lost_205;
		float exp_lost_206;
		float exp_lost_207;
		float exp_lost_208;
		float exp_lost_209;
		float exp_lost_210;
		float exp_lost_211;
		float exp_lost_212;
		float exp_lost_213;
		float exp_lost_214;
		float exp_lost_215;
		float exp_lost_216;
		float exp_lost_217;
		float exp_lost_218;
		float exp_lost_219;
		float exp_lost_220;
		float exp_lost_221;
		float exp_lost_222;
		float exp_lost_223;
		float exp_lost_224;
		float exp_lost_225;
		float exp_lost_226;
		float exp_lost_227;
		float exp_lost_228;
		float exp_lost_229;
		float exp_lost_230;
		float exp_lost_231;
		float exp_lost_232;
		float exp_lost_233;
		float exp_lost_234;
		float exp_lost_235;
		float exp_lost_236;
		float exp_lost_237;
		float exp_lost_238;
		float exp_lost_239;
		float exp_lost_240;
		float exp_lost_241;
		float exp_lost_242;
		float exp_lost_243;
		float exp_lost_244;
		float exp_lost_245;
		float exp_lost_246;
		float exp_lost_247;
		float exp_lost_248;
		float exp_lost_249;
		float exp_lost_250;
		float exp_lost_251;
		float exp_lost_252;
		float exp_lost_253;
		float exp_lost_254;
		float exp_lost_255;
		float exp_lost_256;
	} *player_secondlevel_config;

	int32_t npc_resetprop_service_cnt;
	int32_t npc_resetprop_service_ext_cnt;
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
	} *npc_resetprop_service;

	int32_t npc_petname_service_cnt;
	int32_t npc_petname_service_ext_cnt;
	struct npc_petname_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_object_need;
		int32_t price;
	} *npc_petname_service;

	int32_t npc_petlearnskill_service_cnt;
	int32_t npc_petlearnskill_service_ext_cnt;
	struct npc_petlearnskill_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_skills_1;
		int32_t id_skills_2;
		int32_t id_skills_3;
		int32_t id_skills_4;
		int32_t id_skills_5;
		int32_t id_skills_6;
		int32_t id_skills_7;
		int32_t id_skills_8;
		int32_t id_skills_9;
		int32_t id_skills_10;
		int32_t id_skills_11;
		int32_t id_skills_12;
		int32_t id_skills_13;
		int32_t id_skills_14;
		int32_t id_skills_15;
		int32_t id_skills_16;
		int32_t id_skills_17;
		int32_t id_skills_18;
		int32_t id_skills_19;
		int32_t id_skills_20;
		int32_t id_skills_21;
		int32_t id_skills_22;
		int32_t id_skills_23;
		int32_t id_skills_24;
		int32_t id_skills_25;
		int32_t id_skills_26;
		int32_t id_skills_27;
		int32_t id_skills_28;
		int32_t id_skills_29;
		int32_t id_skills_30;
		int32_t id_skills_31;
		int32_t id_skills_32;
		int32_t id_skills_33;
		int32_t id_skills_34;
		int32_t id_skills_35;
		int32_t id_skills_36;
		int32_t id_skills_37;
		int32_t id_skills_38;
		int32_t id_skills_39;
		int32_t id_skills_40;
		int32_t id_skills_41;
		int32_t id_skills_42;
		int32_t id_skills_43;
		int32_t id_skills_44;
		int32_t id_skills_45;
		int32_t id_skills_46;
		int32_t id_skills_47;
		int32_t id_skills_48;
		int32_t id_skills_49;
		int32_t id_skills_50;
		int32_t id_skills_51;
		int32_t id_skills_52;
		int32_t id_skills_53;
		int32_t id_skills_54;
		int32_t id_skills_55;
		int32_t id_skills_56;
		int32_t id_skills_57;
		int32_t id_skills_58;
		int32_t id_skills_59;
		int32_t id_skills_60;
		int32_t id_skills_61;
		int32_t id_skills_62;
		int32_t id_skills_63;
		int32_t id_skills_64;
		int32_t id_skills_65;
		int32_t id_skills_66;
		int32_t id_skills_67;
		int32_t id_skills_68;
		int32_t id_skills_69;
		int32_t id_skills_70;
		int32_t id_skills_71;
		int32_t id_skills_72;
		int32_t id_skills_73;
		int32_t id_skills_74;
		int32_t id_skills_75;
		int32_t id_skills_76;
		int32_t id_skills_77;
		int32_t id_skills_78;
		int32_t id_skills_79;
		int32_t id_skills_80;
		int32_t id_skills_81;
		int32_t id_skills_82;
		int32_t id_skills_83;
		int32_t id_skills_84;
		int32_t id_skills_85;
		int32_t id_skills_86;
		int32_t id_skills_87;
		int32_t id_skills_88;
		int32_t id_skills_89;
		int32_t id_skills_90;
		int32_t id_skills_91;
		int32_t id_skills_92;
		int32_t id_skills_93;
		int32_t id_skills_94;
		int32_t id_skills_95;
		int32_t id_skills_96;
		int32_t id_skills_97;
		int32_t id_skills_98;
		int32_t id_skills_99;
		int32_t id_skills_100;
		int32_t id_skills_101;
		int32_t id_skills_102;
		int32_t id_skills_103;
		int32_t id_skills_104;
		int32_t id_skills_105;
		int32_t id_skills_106;
		int32_t id_skills_107;
		int32_t id_skills_108;
		int32_t id_skills_109;
		int32_t id_skills_110;
		int32_t id_skills_111;
		int32_t id_skills_112;
		int32_t id_skills_113;
		int32_t id_skills_114;
		int32_t id_skills_115;
		int32_t id_skills_116;
		int32_t id_skills_117;
		int32_t id_skills_118;
		int32_t id_skills_119;
		int32_t id_skills_120;
		int32_t id_skills_121;
		int32_t id_skills_122;
		int32_t id_skills_123;
		int32_t id_skills_124;
		int32_t id_skills_125;
		int32_t id_skills_126;
		int32_t id_skills_127;
		int32_t id_skills_128;
		int32_t id_dialog;
	} *npc_petlearnskill_service;

	int32_t npc_petforgetskill_service_cnt;
	int32_t npc_petforgetskill_service_ext_cnt;
	struct npc_petforgetskill_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_object_need;
		int32_t price;
	} *npc_petforgetskill_service;

	int32_t skillmatter_essence_cnt;
	int32_t skillmatter_essence_ext_cnt;
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
	} *skillmatter_essence;

	int32_t refine_ticket_essence_cnt;
	int32_t refine_ticket_essence_ext_cnt;
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
	} *refine_ticket_essence;

	int32_t destroying_essence_cnt;
	int32_t destroying_essence_ext_cnt;
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
	} *destroying_essence;

	int32_t npc_equipbind_service_cnt;
	int32_t npc_equipbind_service_ext_cnt;
	struct npc_equipbind_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_object_need;
		int32_t price;
	} *npc_equipbind_service;

	int32_t npc_equipdestroy_service_cnt;
	int32_t npc_equipdestroy_service_ext_cnt;
	struct npc_equipdestroy_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_object_need;
		int32_t price;
	} *npc_equipdestroy_service;

	int32_t npc_equipundestroy_service_cnt;
	int32_t npc_equipundestroy_service_ext_cnt;
	struct npc_equipundestroy_service {
		int32_t id;
		char16_t name[64 / sizeof(char16_t)];
		int32_t id_object_need;
		int32_t price;
	} *npc_equipundestroy_service;

	int32_t bible_essence_cnt;
	int32_t bible_essence_ext_cnt;
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
	} *bible_essence;

	int32_t speaker_essence_cnt;
	int32_t speaker_essence_ext_cnt;
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
	} *speaker_essence;

	int32_t autohp_essence_cnt;
	int32_t autohp_essence_ext_cnt;
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
	} *autohp_essence;

	int32_t automp_essence_cnt;
	int32_t automp_essence_ext_cnt;
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
	} *automp_essence;

	int32_t double_exp_essence_cnt;
	int32_t double_exp_essence_ext_cnt;
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
	} *double_exp_essence;

	int32_t transmitscroll_essence_cnt;
	int32_t transmitscroll_essence_ext_cnt;
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
	} *transmitscroll_essence;

	int32_t dye_ticket_essence_cnt;
	int32_t dye_ticket_essence_ext_cnt;
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
	} *dye_ticket_essence;
};

#endif /* PW_ELEMENTS_H */
