/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
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

#include "cjson.h"
#include "cjson_ext.h"
#include "common.h"
#include "idmap.h"
#include "serializer.h"
#include "chain_arr.h"
#include "pw_npc.h"
#include "pw_tasks.h"

extern struct pw_idmap *g_elements_map;
struct pw_idmap *g_tasks_map;

#define TASK_FILE_MAGIC 0x93858361

#ifndef ENODATA
#define ENODATA 87
#endif

enum pw_task_service_type
{
  NPC_TALK = 0x80000000,
  NPC_SELL,
  NPC_BUY,
  NPC_REPAIR,
  NPC_INSTALL,
  NPC_UNINSTALL,
  NPC_GIVE_TASK,
  NPC_COMPLETE_TASK,
  NPC_GIVE_TASK_MATTER,
  NPC_SKILL,
  NPC_HEAL,
  NPC_TRANSMIT,
  NPC_TRANSPORT,
  NPC_PROXY,
  NPC_STORAGE,
  NPC_MAKE,
  NPC_DECOMPOSE,
  TALK_RETURN,
  TALK_EXIT,
  NPC_STORAGE_PASSWORD,
  NPC_IDENTIFY,
  TALK_GIVEUP_TASK,
  NPC_WAR_TOWERBUILD,
  NPC_RESETPROP,
  NPC_PETNAME,
  NPC_PETLEARNSKILL,
  NPC_PETFORGETSKILL,
  NPC_EQUIPBIND,
  NPC_EQUIPDESTROY,
  NPC_EQUIPUNDESTROY,
};

static void
xor_bytes(uint16_t *str, uint32_t len, uint32_t id) {
	int i = 0;

	while (i < len) {
		str[i] ^= id;
		i++;
	}
}

static size_t
serialize_pascal_wstr_fn(FILE *fp, struct serializer *f, void *data)
{
	uint32_t len = *(uint32_t *)data;
	const uint16_t *wstr = *(void **)(data + 4);

	fprintf(fp, "\"%s\":", f->name);
	fprintf(fp, "\"");
	fwsprint(fp, wstr, len);
	fprintf(fp, "\",");

	return 4 + sizeof(wstr);
}

static size_t
deserialize_pascal_wstr_fn(struct cjson *f, struct serializer *slzr, void *data)
{
	uint32_t *len_p = (uint32_t *)data;
	uint32_t len = *len_p;
	uint16_t *wstr = *(void **)(data + 4);
	int rc;

	if (f->type == CJSON_TYPE_NONE) {
		return 4 + sizeof(wstr);
	}

	normalize_json_string(f->s);
	uint32_t newlen = strlen(f->s);
	deserialize_log(f, data);
	memset(wstr, 0, len * 2);
	if (newlen == 0) {
		*len_p = 0;
		return 4 + sizeof(wstr);
	}

	rc = change_charset("UTF-8", "UTF-16LE", f->s, newlen, (char *)wstr, len * 2);
	while (rc < 0) {
		newlen *= 2;

		free(wstr);
		wstr = *(void **)(data + 4) = calloc(1, newlen * 2);
		if (!wstr) {
			PWLOG(LOG_ERROR, "calloc() failed\n");
			return 4 + sizeof(wstr);
		}
		rc = change_charset("UTF-8", "UTF-16LE", f->s, strlen(f->s), (char *)wstr, newlen * 2);
		if (rc >= 0) {
			break;
		}
	}

	newlen = 0;
	while (*wstr) {
		newlen++;
	}
	*len_p = newlen;
	return 4 + sizeof(wstr);
}

static struct serializer pw_task_location_serializer[] = {
	{ "east", _FLOAT },
	{ "bottom", _FLOAT },
	{ "south", _FLOAT },
	{ "west", _FLOAT },
	{ "top", _FLOAT },
	{ "north", _FLOAT },
	{ "", _TYPE_END },
};

static struct serializer pw_task_point_serializer[] = {
	{ "world", _INT32 },
	{ "x", _FLOAT },
	{ "y", _FLOAT },
	{ "z", _FLOAT },
	{ "", _TYPE_END },
};

static struct serializer pw_task_date_serializer[] = {
	{ "year", _INT32 },
	{ "month", _INT32 },
	{ "day", _INT32 },
	{ "hour", _INT32 },
	{ "minute", _INT32 },
	{ "weekday", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer pw_task_date_span_serializer[] = {
	{ "start", _OBJECT_START, NULL, NULL, pw_task_date_serializer },
	{ "end", _OBJECT_START, NULL, NULL, pw_task_date_serializer },
	{ "", _TYPE_END },
};

static struct serializer pw_task_item_serializer[] = {
	{ "id", _INT32 },
	{ "is_common", _INT8 },
	{ "amount", _INT32 },
	{ "probability", _FLOAT },
	{ "", _TYPE_END },
};

static struct serializer pw_task_player_serializer[] = {
	{ "level_min", _INT32 },
	{ "level_max", _INT32 },
	{ "race", _INT32 },
	{ "occupation", _INT32 },
	{ "gender", _INT32 },
	{ "amount_min", _INT32 },
	{ "amount_max", _INT32 },
	{ "quest", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer pw_task_mob_serializer[] = {
	{ "id", _INT32 },
	{ "count", _INT32 },
	{ "drop_item_id", _INT32 },
	{ "drop_item_cnt", _INT32 },
	{ "drop_item_is_common", _INT8 },
	{ "drop_item_probability", _FLOAT },
	{ "lvl_diff_gt8_doesnt_cnt", _INT8 },
	{ "", _TYPE_END },
};

static struct serializer pw_task_item_group_serializer[] = {
	{ "chosen_randomly", _INT8 },
	{ "_items_cnt", _INT32 },
	{ "items", _CHAIN_TABLE, pw_task_item_serializer },
	{ "", _TYPE_END },
};

struct __attribute__((packed)) pw_task_award {
	uint32_t coins;
	uint32_t xp;
	uint32_t new_quest;
	uint32_t sp;
	uint32_t rep;
	uint32_t culti;
	uint32_t new_waypoint;
	uint32_t storage_slots;

	uint32_t inventory_slots;
	uint32_t petbag_slots;
	uint32_t chi;
	/* ... and more */
};

static struct serializer pw_task_award_serializer[] = {
	{ "coins", _INT32 },
	{ "xp", _INT32 },
	{ "new_quest", _INT32 },
	{ "sp", _INT32 },
	{ "rep", _INT32 },
	{ "culti", _INT32 },
	{ "new_waypoint", _INT32 },
	{ "storage_slots", _INT32 },
	{ "inventory_slots", _INT32 },
	{ "petbag_slots", _INT32 },
	{ "chi", _INT32 },
	{ "tp", _OBJECT_START, NULL, NULL, pw_task_point_serializer },
	{ "ai_trigger", _INT32 },
	{ "ai_trigger_enable", _INT8 },
	{ "level_multiplier", _INT8 },
	{ "divorce", _INT8 },
	{ "_item_groups_cnt", _INT32 },
	{ "ptr", _INT32 },
	{ "item_groups", _CHAIN_TABLE, pw_task_item_group_serializer },
	{ "", _TYPE_END },
};

static struct serializer pw_task_award_timed_serializer[] = {
	{ "_awards_cnt", _INT32 },
	{ "time_spent_ratio", _ARRAY_START(5) },
		{ "", _FLOAT },
	{ "", _ARRAY_END },
	{ "awards", _CHAIN_TABLE, pw_task_award_serializer },
	{ "", _TYPE_END },
};

static struct serializer pw_task_award_scaled_serializer[] = {
	{ "_awards_cnt", _INT32 },
	{ "item_id", _INT32 },
	{ "step", _INT32 },
	{ "awards", _CHAIN_TABLE, pw_task_award_serializer },
	{ "", _TYPE_END },
};

static struct serializer pw_task_talk_proc_choice_serializer[] = {
	{ "id", _INT32 },
	{ "text", _WSTRING(64) },
	{ "param", _INT32 },
	{ "", _TYPE_END },
};

static struct serializer pw_task_talk_proc_question_serializer[] = {
	{ "id", _INT32 },
	{ "control", _INT32 },
	{ "text", _CUSTOM, serialize_pascal_wstr_fn, deserialize_pascal_wstr_fn },
	{ "_choices_cnt", _INT32 },
	{ "choices", _CHAIN_TABLE, pw_task_talk_proc_choice_serializer },
	{ "", _TYPE_END },
};

static struct serializer pw_task_talk_proc_serializer[] = {
	{ "id", _INT32 },
	{ "name", _WSTRING(64) },
	{ "_questions_cnt", _INT32 },
	{ "questions", _CHAIN_TABLE, pw_task_talk_proc_question_serializer },
	{ "", _TYPE_END },
};

static struct serializer pw_task_ref_serializer[] = {
	{ "", _INT32 },
	{ "", _TYPE_END },
};


static struct serializer pw_task_serializer[];

static size_t
serialize_start_by_fn(FILE *fp, struct serializer *f, void *data)
{
	struct serializer *slzr = pw_task_serializer;
	void *task = data;
	int val = 0;

	if (fp == g_nullfile) {
		return 0;
	}

	if (*(uint8_t *)serializer_get_field(slzr, "_auto_trigger", task)) val = 1;
	if (*(uint32_t *)serializer_get_field(slzr, "start_npc", task) & ~(1 << 31)) val = 2;
	if (*(uint8_t *)serializer_get_field(slzr, "_start_on_enter", task)) val = 3;
	if (*(uint8_t *)serializer_get_field(slzr, "_trigger_on_death", task)) val = 4;

	fprintf(fp, "\"%s\":%d,", f->name, val);
	return 0;
}

static size_t
deserialize_start_by(struct cjson *f, struct serializer *slzr, void *data)
{
	void *task = data;

	if (f->type == CJSON_TYPE_NONE) {
		return 0;
	}

	*(uint8_t *)serializer_get_field(slzr, "_auto_trigger", task) = 0;
	*(uint32_t *)serializer_get_field(slzr, "start_npc", task) |= (1 << 31);
	*(uint8_t *)serializer_get_field(slzr, "_start_on_enter", task) = 0;
	*(uint8_t *)serializer_get_field(slzr, "_trigger_on_death", task) = 0;
	switch (JSi(f)) {
		case 1:
			*(uint8_t *)serializer_get_field(slzr, "_auto_trigger", task) = 1;
			break;
		case 2:
			*(uint32_t *)serializer_get_field(slzr, "start_npc", task) &= ~(1 << 31);
			break;
		case 3:
			*(uint8_t *)serializer_get_field(slzr, "_start_on_enter", task) = 1;
			break;
		case 4:
			*(uint8_t *)serializer_get_field(slzr, "_trigger_on_death", task) = 1;
			break;
	}

	return 0;
}

static struct serializer pw_task_serializer[] = {
	{ "_start_by", _CUSTOM, serialize_start_by_fn, deserialize_start_by },
	{ "id", _INT32 },
	{ "name", _WSTRING(30) },
	{ "_has_signature", _INT8 }, /* we'll be always setting this to 0 */
	{ "_ptr1", _INT32 },
	{ "type", _INT32 },
	{ "time_limit_sec", _INT32 },
	{ "date_span_is_cyclic", _INT8 },
	{ "_date_spans_cnt", _INT32 },
	{ "date_types", _ARRAY_START(12) },
		{ "", _INT8 },
	{ "", _ARRAY_END },
	{ "_ptr3", _INT32 },
	{ "_ptr4", _INT32 },
	{ "avail_frequency", _INT32 },
	{ "activate_chosen_subquest", _INT8 },
	{ "activate_random_subquest", _INT8 },
	{ "activate_subquests_in_order", _INT8 },
	{ "on_give_up_parent_fail", _INT8 },
	{ "on_success_parent_success", _INT8 },
	{ "can_give_up", _INT8 },
	{ "can_retake", _INT8 },
	{ "can_retake_after_failure", _INT8 },
	{ "on_fail_parent_fail", _INT8 },
	{ "need_record", _INT8 },
	{ "fail_on_death", _INT8 },
	{ "simultaneous_player_limit", _INT32 },
	{ "_start_on_enter", _INT8 },
	{ "start_on_enter_world_id", _INT32 },
	{ "start_on_enter_location", _OBJECT_START, NULL, NULL, pw_task_location_serializer },
	{ "instant_teleport", _INT8 },
	{ "instant_teleport_point", _OBJECT_START, NULL, NULL, pw_task_point_serializer },
	{ "ai_trigger", _INT32 },
	{ "ai_trigger_enable", _INT8 },
	{ "_auto_trigger", _INT8 },
	{ "_trigger_on_death", _INT8 },
	{ "remove_premise_items", _INT8 },
	{ "recommended_level", _INT32 },
	{ "show_quest_title", _INT8 },
	{ "show_as_gold_quest", _INT8 },
	{ "start_npc", _INT32 },
	{ "finish_npc", _INT32 },
	{ "is_craft_skill_quest", _INT8 },
	{ "can_be_found", _INT8 },
	{ "show_direction", _INT8 },
	{ "is_marriage_quest", _INT8 },
	{ "premise_level_min", _INT32 },
	{ "premise_level_max", _INT32 },
	{ "dontshow_under_level_min", _INT8 },
	{ "_premise_items_cnt", _INT32 },
	{ "_ptr5", _INT32 },
	{ "dontshow_without_premise_items", _INT8 },
	{ "_free_given_items_cnt", _INT32 },
	{ "_free_given_common_items_cnt", _INT32 },
	{ "_free_given_task_items_cnt", _INT32 },
	{ "_ptr6", _INT32 },
	{ "premise_coins", _INT32 },
	{ "dontshow_without_premise_coins", _INT8 },
	{ "premise_reputation_min", _INT32 },
	{ "premise_reputation_max", _INT32 },
	{ "dontshow_without_premise_reputation", _INT8 },
	{ "_req_quests_done_cnt", _INT32 },
	{ "premise_quests", _ARRAY_START(5) },
		{ "", _INT32 },
	{ "", _ARRAY_END },
	{ "dontshow_without_premise_quests", _INT8 },
	{ "premise_cultivation", _INT32 },
	{ "dontshow_without_premise_cultivation", _INT8 },
	{ "premise_faction_role", _INT32 },
	{ "dontshow_without_premise_faction_role", _INT8 },
	{ "premise_gender", _INT32 },
	{ "dontshow_premise_gender", _INT8 },
	{ "_premise_class_cnt", _INT32 },
	{ "premise_class", _ARRAY_START(8) },
		{ "", _INT32 },
	{ "", _ARRAY_END },
	{ "dontshow_wrong_class", _INT8 },
	{ "premise_be_married", _INT8 },
	{ "dontshow_without_marriage", _INT8 },
	{ "premise_be_gm", _INT8 },
	{ "_premise_global_quest", _INT32 },
	{ "_premise_global_quest_cond", _INT32 },
	{ "_mutex_quests_cnt", _INT32 },
	{ "mutex_quests", _ARRAY_START(5) },
		{ "", _INT32 },
	{ "", _ARRAY_END },
	{ "premise_blacksmith_level", _INT32 },
	{ "premise_tailor_level", _INT32 },
	{ "premise_craftsman_level", _INT32 },
	{ "premise_apothecary_level", _INT32 },
	{ "_m_DynTaskType", _INT8 },
	{ "special_award_type", _INT32 },
	{ "is_team_task", _INT8 },
	{ "recv_in_team_only", _INT8 },
	{ "m_bSharedTask", _INT8 },
	{ "m_bSharedAchieved", _INT8 },
	{ "m_bCheckTeammate", _INT8 },
	{ "m_fTeammateDist", _FLOAT },
	{ "m_bAllFail", _INT8 },
	{ "m_bCapFail", _INT8 },
	{ "m_bCapSucc", _INT8 },
	{ "m_fSuccDist", _FLOAT },
	{ "m_bDismAsSelfFail", _INT8 },
	{ "m_bRcvChckMem", _INT8 },
	{ "m_fRcvMemDist", _FLOAT },
	{ "m_bCntByMemPos", _INT8 },
	{ "m_fCntMemDist", _FLOAT },
	{ "_premise_squad_cnt", _INT32 },
	{ "_ptr7", _INT32 },
	{ "dontshow_without_premise_squad", _INT8 },
	{ "success_method", _INT32 },
	{ "need_npc_finish", _INT32 }, /* 0 => auto finish once children finished */
	{ "_req_monsters_cnt", _INT32 },
	{ "_ptr8", _INT32 },
	{ "_req_items_cnt", _INT32 },
	{ "_ptr9", _INT32 },
	{ "req_coins", _INT32 },
	{ "m_ulNPCToProtect", _INT32 },
	{ "m_ulProtectTimeLen", _INT32 },
	{ "m_ulNPCMoving", _INT32 },
	{ "m_ulNPCDestSite", _INT32 },
	{ "reach_location", _OBJECT_START, NULL, NULL, pw_task_location_serializer },
	{ "reach_location_world_id", _INT32 },
	{ "req_wait_time", _INT32 },
	{ "m_ulAwardType_S", _INT32 },
	{ "m_ulAwardType_F", _INT32 },
	{ "_ptr10", _ARRAY_START(6) },
		{ "", _INT32 },
	{ "", _ARRAY_END },
	{ "parent_quest", _INT32 },
	{ "previous_quest", _INT32 },
	{ "next_quest", _INT32 },
	{ "sub_quest_first", _INT32 },
/*	{ "signature", _CUSTOM, serialize_signature, deserialize_signature },
 * the signature is always removed */
	{ "date_spans", _CHAIN_TABLE, pw_task_date_span_serializer },
	{ "premise_items", _CHAIN_TABLE, pw_task_item_serializer },
	{ "free_given_items", _CHAIN_TABLE, pw_task_item_serializer },
	{ "premise_squad", _CHAIN_TABLE, pw_task_player_serializer },
	{ "req_monsters", _CHAIN_TABLE, pw_task_mob_serializer },
	{ "req_items", _CHAIN_TABLE, pw_task_item_serializer },
	{ "success_award", _OBJECT_START, NULL, NULL, pw_task_award_serializer },
	{ "failure_award", _OBJECT_START, NULL, NULL, pw_task_award_serializer },
	{ "success_timed_award", _OBJECT_START, NULL, NULL, pw_task_award_timed_serializer },
	{ "failure_timed_award", _OBJECT_START, NULL, NULL, pw_task_award_timed_serializer },
	{ "success_scaled_award", _OBJECT_START, NULL, NULL, pw_task_award_scaled_serializer },
	{ "failure_scaled_award", _OBJECT_START, NULL, NULL, pw_task_award_scaled_serializer },
	{ "briefing", _CUSTOM, serialize_pascal_wstr_fn, deserialize_pascal_wstr_fn },
	{ "unk1_text", _CUSTOM, serialize_pascal_wstr_fn, deserialize_pascal_wstr_fn },
	{ "unk2_text", _CUSTOM, serialize_pascal_wstr_fn, deserialize_pascal_wstr_fn },
	{ "description", _CUSTOM, serialize_pascal_wstr_fn, deserialize_pascal_wstr_fn },
	{ "dialogue", _OBJECT_START },
		{ "initial", _OBJECT_START, NULL, NULL, pw_task_talk_proc_serializer },
		{ "notqualified", _OBJECT_START, NULL, NULL, pw_task_talk_proc_serializer },
		{ "unknown", _OBJECT_START, NULL, NULL, pw_task_talk_proc_serializer },
		{ "unfinished", _OBJECT_START, NULL, NULL, pw_task_talk_proc_serializer },
		{ "ready", _OBJECT_START, NULL, NULL, pw_task_talk_proc_serializer },
	{ "", _OBJECT_END },
	{ "_sub_quests_cnt", _INT32 },
	{ "sub_quests", _CHAIN_TABLE, pw_task_ref_serializer },
	{ "", _TYPE_END },
};

static int
read_award(void **buf_p, FILE *fp, bool is_server)
{
	void *data, *buf, *ptr;
	size_t i, item_groups_count; 
	struct serializer *slzr = pw_task_award_serializer;

	buf = data = *buf_p;

	if (is_server) {
		fread(buf, 75, 1, fp);
		buf += 75;
	} else {
		fread(buf, 67, 1, fp);
		buf += 67;
		fseek(fp, 5, SEEK_CUR);
		fread(buf, 8, 1, fp);
		buf += 8;
	}

	item_groups_count = *(uint32_t *)serializer_get_field(slzr, "_item_groups_cnt", data);
	*(void **)buf = ptr = pw_chain_table_fread(fp, "item_groups", 0, pw_task_item_group_serializer);
	if (!ptr) {
		return -1;
	}
	buf += sizeof(void *);
	*buf_p = buf;

	for (i = 0; i < item_groups_count; i++) {
		buf = pw_chain_table_new_el(ptr);

		void *data_start = buf;
		fread(buf, 5, 1, fp);
		buf += 5;

		size_t item_count = *(uint32_t *)serializer_get_field(pw_task_item_group_serializer, "_items_cnt", data_start);
		void *items = serializer_get_field(pw_task_item_group_serializer, "items", data_start);

		*(void **)items = ptr = pw_chain_table_fread(fp, "items", item_count, pw_task_item_serializer);
		if (!ptr) {
			return -1;
		}
	}

	return 0;
}

#define SAVE_TBL_CNT(name, slzr, data) \
	({ \
		struct pw_chain_table *tbl = *(void **)serializer_get_field(slzr, name, data); \
		struct pw_chain_el *chain = tbl->chain; \
		uint32_t cnt = 0; \
		while (chain) { \
			cnt += chain->count; \
			chain = chain->next; \
		} \
		*(uint32_t *)serializer_get_field(slzr, "_" name "_cnt", data) = cnt; \
	})

static void
write_award(void **buf_p, FILE *fp, bool is_client)
{
	void *data, *buf;
	struct serializer *slzr = pw_task_award_serializer;
	struct pw_chain_table *table;
	void *_el;

	buf = data = *buf_p;
	SAVE_TBL_CNT("item_groups", slzr, data);

	if (is_client) {
		fwrite(buf, 67, 1, fp);
		buf += 67;
		fwrite(g_zeroes, 5, 1, fp);
		/* groups count */
		fwrite(buf, 8, 1, fp);
		buf += 8;
	} else {
		fwrite(buf, 75, 1, fp);
		buf += 75;
	}

	table = *(void **)buf;
	buf += sizeof(void *);

	PW_CHAIN_TABLE_FOREACH(_el, table) {
		struct pw_chain_table *item_table;
		void *el = _el;
		void *el_start = el;
		void *item;

		SAVE_TBL_CNT("items", table->serializer, el_start);

		fwrite(el, 5, 1, fp);
		el += 5;

		item_table = *(void **)serializer_get_field(table->serializer, "items", el_start);
		PW_CHAIN_TABLE_FOREACH(item, item_table) {
			fwrite(item, item_table->el_size, 1, fp);
		}
	}

	*buf_p = buf;
}


#define LOAD_CHAIN_TBL_CNT(fp, name, data_slzr, data_start, tbl_slzr, cnt) \
	*(void **)(buf) = tbl_p = pw_chain_table_fread((fp), #name, cnt, tbl_slzr); \
	if (!(tbl_p)) { \
		PWLOG(LOG_ERROR, "pw_chain_table_fread() failed\n"); \
		return -1; \
	} \
	buf += sizeof(void *)

#define LOAD_CHAIN_TBL(fp, name, data_slzr, data_start, tbl_slzr) \
	count = *(uint32_t *)serializer_get_field((data_slzr), "_" name "_cnt", (data_start)); \
	LOAD_CHAIN_TBL_CNT(fp, name, data_slzr, data_start, tbl_slzr, count)

static int
read_task(struct pw_task_file *taskf, FILE *fp, bool is_server)
{
	struct pw_chain_table *tbl_p;
	uint32_t id;
	size_t count;
	int rc, i;
	struct serializer *slzr = pw_task_serializer;
	void *data = pw_chain_table_new_el(taskf->tasks);
	void *buf = data;

	if (!data) {
		PWLOG(LOG_ERROR, "pw_chain_table_new_el() failed\n");
		return -1;
	}

	/* raw data */
	fread(buf, 534, 1, fp);
	buf += 534;

	id = *(uint32_t *)data;
	xor_bytes(serializer_get_field(slzr, "name", data), 30, id);

	uint8_t *has_signature = (uint8_t *)serializer_get_field(slzr, "_has_signature", data);
	if(*has_signature) {
		fseek(fp, 60, SEEK_CUR);
	}
	/* who needs this? */
	*has_signature = 0;

	LOAD_CHAIN_TBL(fp, "date_spans", slzr, data, pw_task_date_span_serializer);
	LOAD_CHAIN_TBL(fp, "premise_items", slzr, data, pw_task_item_serializer);
	LOAD_CHAIN_TBL(fp, "free_given_items", slzr, data, pw_task_item_serializer);
	LOAD_CHAIN_TBL(fp, "premise_squad", slzr, data, pw_task_player_serializer);
	LOAD_CHAIN_TBL(fp, "req_monsters", slzr, data, pw_task_mob_serializer);
	LOAD_CHAIN_TBL(fp, "req_items", slzr, data, pw_task_item_serializer);

	rc = read_award(&buf, fp, is_server);
	if (rc < 0) {
		return -1;
	}

	rc = read_award(&buf, fp, is_server);
	if (rc < 0) {
		return -1;
	}

	for (int a = 0; a < 2; a++) {
		void *data_start = buf;

		fread(data_start, 24, 1, fp);
		buf += 24;

		LOAD_CHAIN_TBL_CNT(fp, "awards", slzr, data_start, pw_task_award_serializer, 0);
		count = *(uint32_t *)serializer_get_field(pw_task_award_timed_serializer, "_awards_cnt", data_start);

		for (i = 0; i < count; i++) {
			void *el = pw_chain_table_new_el(tbl_p);
			rc = read_award(&el, fp, is_server);
			if (rc) {
				return -1;
			}
		}
	}

	for (int a = 0; a < 2; a++) {
		void *data_start = buf;

		fread(buf, 12, 1, fp);
		buf += 12;

		LOAD_CHAIN_TBL_CNT(fp, "awards", slzr, data_start, pw_task_award_serializer, 0);
		count = *(uint32_t *)serializer_get_field(pw_task_award_scaled_serializer, "_awards_cnt", data_start);

		for (i = 0; i < count; i++) {
			void *el = pw_chain_table_new_el(tbl_p);
			rc = read_award(&el, fp, is_server);
			if (rc) {
				return -1;
			}
		}
	}

	/* always zeroes */
	fseek(fp, 32, SEEK_CUR);

	for (int x = 0; x < 4; x++) {
		uint32_t len;
		fread(&len, 4, 1, fp);
		*(uint32_t *)buf = len;
		buf += 4;

		uint16_t *wstr = *(void **)buf = calloc(1, len * 2 + 2);
		if (!wstr) {
			PWLOG(LOG_ERROR, "calloc() failed\n");
			return -1;
		}
		fread(wstr, len * 2, 1, fp);
		xor_bytes(wstr, len, id);

		buf += sizeof(void *);
	}

	for (int p = 0; p < 5; p++) {
		char *data_start = buf;

		fread(buf, 136, 1, fp);
		buf += 136;

		xor_bytes(serializer_get_field(pw_task_talk_proc_serializer, "name", data_start), 64, id);

		count = *(uint32_t *)serializer_get_field(pw_task_talk_proc_serializer, "_questions_cnt", data_start);
		LOAD_CHAIN_TBL_CNT(fp, "questions", slzr, data_start, pw_task_talk_proc_question_serializer, 0);

		for (int q = 0; q < count; q++) {
			void *el = pw_chain_table_new_el(tbl_p);
			void *buf = el;
			uint32_t wstr_len;

			fread(buf, 8, 1, fp);
			buf += 8;

			fread(&wstr_len, 4, 1, fp);
			*(uint32_t *)buf = wstr_len;
			buf += 4;

			uint16_t *wstr = *(void **)buf = calloc(1, wstr_len * 2 + 2);
			buf += sizeof(void *);
			if (!wstr) {
				PWLOG(LOG_ERROR, "calloc() failed\n");
				return -1;
			}
			fread(wstr, wstr_len * 2, 1, fp);
			xor_bytes(wstr, wstr_len, id);

			fread(buf, 4, 1, fp);
			buf += 4;

			{
				struct pw_chain_table *tbl_p;
				void *c;
				size_t count;

				LOAD_CHAIN_TBL(fp, "choices", pw_task_talk_proc_question_serializer, el, pw_task_talk_proc_choice_serializer);

				PW_CHAIN_TABLE_FOREACH(c, tbl_p) {
					xor_bytes(serializer_get_field(pw_task_talk_proc_choice_serializer, "text", c), 64, id);
				}
			}

		}
	}

	fread(&count, 4, 1, fp);
	*(uint32_t *)buf = count;
	buf += 4;
	LOAD_CHAIN_TBL_CNT(fp, "sub_quests", slzr, data, pw_task_ref_serializer, 0);
	for (int s = 0; s < count; s++) {
		rc = read_task(taskf, fp, is_server);
		if (rc < 0) {
			assert(false);
			continue;
		}

		void *el = pw_chain_table_new_el(tbl_p);
		*(uint32_t *)el = rc;
	}

	pw_idmap_set(taskf->idmap, id, id, 0, data);

	return id;
}

static void
save_chain_tbl(FILE *fp, void **buf)
{
	void **tbl_ptr = *(void ***)buf;
	struct pw_chain_table *tbl = *tbl_ptr;
	void *el;

	PW_CHAIN_TABLE_FOREACH(el, tbl) {
		fwrite(el, tbl->el_size, 1, fp);
	}

	*buf += sizeof(void *);
}

static void
finalize_id_field(const char *fieldname, struct serializer *slzr, void *task)
{
	uint32_t *f = (uint32_t *)serializer_get_field(slzr, fieldname, task);

	if (*f & (1 << 31)) {
		*f = 0;
	} else {
		*f = ~(1 << 31);
	}
}

static void
write_task(struct pw_task_file *taskf, void *data, FILE *fp, bool is_client)
{
	void *buf = data;
	struct pw_chain_table *tbl_p;
	uint32_t id;
	struct serializer *slzr = pw_task_serializer;

	id = *(uint32_t *)data;
	xor_bytes(serializer_get_field(slzr, "name", data), 30, id);

	SAVE_TBL_CNT("date_spans", slzr, data);
	SAVE_TBL_CNT("premise_items", slzr, data);
	SAVE_TBL_CNT("free_given_items", slzr, data);
	SAVE_TBL_CNT("premise_squad", slzr, data);
	SAVE_TBL_CNT("req_monsters", slzr, data);
	SAVE_TBL_CNT("req_items", slzr, data);
	finalize_id_field("start_npc", slzr, data);

	fwrite(buf, 534, 1, fp);
	buf += 534;

	save_chain_tbl(fp, &buf);
	save_chain_tbl(fp, &buf);
	save_chain_tbl(fp, &buf);
	save_chain_tbl(fp, &buf);
	save_chain_tbl(fp, &buf);
	save_chain_tbl(fp, &buf);

	write_award(&buf, fp, is_client);
	write_award(&buf, fp, is_client);

	for (int a = 0; a < 2; a++) {
		void *data_start = buf;
		void *el;

		SAVE_TBL_CNT("awards", pw_task_award_timed_serializer, data_start);

		fwrite(buf, 24, 1, fp);
		buf += 24;

		tbl_p = *(void **)serializer_get_field(pw_task_award_timed_serializer, "awards", data_start);
		buf += sizeof(void *);
		PW_CHAIN_TABLE_FOREACH(el, tbl_p) {
			void *_el = el;
			write_award(&_el, fp, is_client);
		}
	}

	for (int a = 0; a < 2; a++) {
		void *data_start = buf;
		void *el;

		SAVE_TBL_CNT("awards", pw_task_award_scaled_serializer, data_start);

		fwrite(buf, 12, 1, fp);
		buf += 12;

		tbl_p = *(void **)serializer_get_field(pw_task_award_scaled_serializer, "awards", data_start);
		buf += sizeof(void *);
		PW_CHAIN_TABLE_FOREACH(el, tbl_p) {
			void *_el = el;
			write_award(&_el, fp, is_client);
		}
	}

	/* always zeroes */
	fseek(fp, 32, SEEK_CUR);

	for (int x = 0; x < 4; x++) {
		uint32_t len = *(uint32_t *)buf;

		fwrite(buf, sizeof(uint32_t), 1, fp);
		buf += 4;

		uint16_t *wstr = *(void **)buf;
		xor_bytes(wstr, len, id);
		fwrite(wstr, len * 2, 1, fp);

		buf += sizeof(void *);
	}

	for (int p = 0; p < 5; p++) {
		char *data_start = buf;
		void *el;

		xor_bytes(serializer_get_field(pw_task_talk_proc_serializer, "name", data_start), 64, id);
		SAVE_TBL_CNT("questions", pw_task_talk_proc_serializer, data_start);

		fwrite(buf, 136, 1, fp);
		buf += 136;

		tbl_p = *(void **)serializer_get_field(pw_task_talk_proc_serializer, "questions", data_start);
		buf += sizeof(void *);
		PW_CHAIN_TABLE_FOREACH(el, tbl_p) {
			char *data_start = el;
			void *buf = el;

			fwrite(buf, 8, 1, fp);
			buf += 8;

			uint32_t len = *(uint32_t *)buf;
			fwrite(&len, sizeof(uint32_t), 1, fp);
			buf += sizeof(uint32_t);

			uint16_t *wstr = *(void **)buf;
			xor_bytes(wstr, len, id);
			fwrite(wstr, len * 2, 1, fp);

			buf += sizeof(void *);

			SAVE_TBL_CNT("choices", pw_task_talk_proc_question_serializer, data_start);
			fwrite(buf, 4, 1, fp);
			buf += 4;

			{
				struct pw_chain_table *tbl_p = *(void **)buf;
				buf += sizeof(void *);
				void *c;

				PW_CHAIN_TABLE_FOREACH(c, tbl_p) {
					xor_bytes(serializer_get_field(pw_task_talk_proc_choice_serializer, "text", c), 64, id);
					fwrite(c, tbl_p->el_size, 1, fp);
				}
			}
		}
	}

	SAVE_TBL_CNT("sub_quests", pw_task_serializer, data);

	fwrite(buf, 4, 1, fp);
	buf += 4;

	tbl_p = *(void **)serializer_get_field(slzr, "sub_quests", data);
	void *el;

	PW_CHAIN_TABLE_FOREACH(el, tbl_p) {
		uint32_t id = *(uint32_t *)el;

		if (id == 0) {
			continue;
		}

		void *sub_q = pw_idmap_get(taskf->idmap, id, 0); /* FIXME go async */
		write_task(taskf, sub_q, fp, is_client);
	}
}

int
pw_tasks_load(struct pw_task_file *taskf, const char *path)
{
	uint32_t *jmp_offsets;
	FILE* fp;
	uint32_t count;

	memset(taskf, 0, sizeof(*taskf));

	fp = fopen(path, "rb");
	if (!fp) {
		return -errno;
	}

	fread(&taskf->magic, sizeof(taskf->magic), 1, fp);
	if (taskf->magic != TASK_FILE_MAGIC) {
		return -ENOEXEC;
	}

	fread(&taskf->version, sizeof(taskf->version), 1, fp);
	if (taskf->version != 55 && taskf->version != 56) {
		return -ENOEXEC;
	}
	bool is_server = taskf->version == 55;
  
	fread(&count, sizeof(count), 1, fp);
	if (count == 0) {
		return -ENODATA;
	}

	jmp_offsets = malloc(count * sizeof (*jmp_offsets));
	if (!jmp_offsets) {
		fclose(fp);
		return -ENOMEM;
	}

	taskf->idmap = pw_idmap_init("tasks", NULL);
	fread(jmp_offsets, sizeof(*jmp_offsets), count, fp);
	taskf->tasks = pw_chain_table_alloc("quests", pw_task_serializer, serializer_get_size(pw_task_serializer), count);
	if (!taskf->tasks) {
		free(jmp_offsets);
		fclose(fp);
		return -ENOMEM;
	}

	for (uint32_t t = 0; t < count; t++) {
		if (fseek(fp, jmp_offsets[t], SEEK_SET) != 0) {
			/* invalid task, skip it just like the game does */
			continue;
		}

		read_task(taskf, fp, is_server);
		/* dont care about failure, we skip invalid tasks anyway */
	}

	free(jmp_offsets);
	fclose(fp);
	return 0;
}

int
pw_tasks_serialize(struct pw_task_file *taskf, const char *filename)
{
	FILE *fp = fopen(filename, "wb");
	void *el;

	if (fp == NULL) {
		PWLOG(LOG_ERROR, "cant open %s\n", filename);
		return 1;
	}


	fprintf(fp, "[");


	PW_CHAIN_TABLE_FOREACH(el, taskf->tasks) {
		struct serializer *tmp_slzr = pw_task_serializer;
		size_t pos_begin = ftell(fp);
		void *tmp_el = el;

		_serialize(fp, &tmp_slzr, &tmp_el, 1, true, true, true);

		if (ftell(fp) > pos_begin + 2) {
			fprintf(fp, ",\n");
		}
	}

	/* replace ,\n} with }] */
	fseek(fp, -3, SEEK_CUR);
	fprintf(fp, "}]");

	size_t sz = ftell(fp);
	fclose(fp);
	truncate(filename, sz);
	return 0;
}

int
pw_tasks_save(struct pw_task_file *taskf, const char *path, bool is_server)
{
	uint32_t *jmp_offsets;
	FILE* fp;
	uint32_t count;
	void *el;

	fp = fopen(path, "wb");
	if (!fp) {
		return -errno;
	}

	taskf->version = is_server ? 55 : 56;
	fwrite(&taskf->magic, sizeof(taskf->magic), 1, fp);
	fwrite(&taskf->version, sizeof(taskf->version), 1, fp);
  
	count = 0;
	PW_CHAIN_TABLE_FOREACH(el, taskf->tasks) {
		uint32_t parent_id = *(uint32_t *)serializer_get_field(pw_task_serializer, "parent_quest", el);
		if (!parent_id) {
			count++;
		}
	}
	fwrite(&count, sizeof(count), 1, fp);

	jmp_offsets = malloc(count * sizeof (*jmp_offsets));
	if (!jmp_offsets) {
		fclose(fp);
		return -ENOMEM;
	}

	/* skip offset, we'll get back to them */
	size_t jmp_table_pos = ftell(fp);
	fseek(fp, count * sizeof (*jmp_offsets), SEEK_CUR);

	uint32_t i = 0;
	PW_CHAIN_TABLE_FOREACH(el, taskf->tasks) {
		uint32_t parent_id = *(uint32_t *)serializer_get_field(pw_task_serializer, "parent_quest", el);
		if (parent_id) {
			continue;
		}
		jmp_offsets[i++] = ftell(fp);
		write_task(taskf, el, fp, !is_server);
	}

	assert(i == count);
	fseek(fp, jmp_table_pos, SEEK_SET);
	fwrite(jmp_offsets, sizeof(uint32_t) * count, 1, fp);

	free(jmp_offsets);
	fclose(fp);
	return 0;
}

void
pw_tasks_adjust_rates(struct pw_task_file *taskf, struct cjson *rates)
{
	void *task;

	double xp_rate = JSf(rates, "quest", "xp");
	double sp_rate = JSf(rates, "quest", "sp");
	double coins_rate = JSf(rates, "quest", "coin");

	fprintf(stderr, "Adjusting rates:\n");
	fprintf(stderr, "  quest xp:   %8.4f\n", xp_rate);
	fprintf(stderr, "  quest sp:   %8.4f\n", sp_rate);
	fprintf(stderr, "  quest coin: %8.4f\n", coins_rate);

	void adjust_award_rates(struct pw_task_award *award) {
		award->xp *= xp_rate;
		award->sp *= sp_rate;
		award->coins *= coins_rate;
	}

	struct serializer *slzr = taskf->tasks->serializer;
	int success_award_off = serializer_get_offset(slzr, "success_award");
	int failure_award_off = serializer_get_offset(slzr, "failure_award");
	struct serializer *timed_award_slzr;
	int success_timed_awards_tbl_off = serializer_get_offset_slzr(slzr, "success_timed_award", &timed_award_slzr);
	int failure_timed_awards_tbl_off = serializer_get_offset_slzr(slzr, "failure_timed_award", &timed_award_slzr);

	success_timed_awards_tbl_off += serializer_get_offset(timed_award_slzr->ctx, "awards");
	failure_timed_awards_tbl_off += serializer_get_offset(timed_award_slzr->ctx, "awards");

	struct serializer *scaled_award_slzr;
	int success_scaled_awards_tbl_off = serializer_get_offset_slzr(slzr, "success_scaled_award", &scaled_award_slzr);
	int failure_scaled_awards_tbl_off = serializer_get_offset_slzr(slzr, "failure_scaled_award", &scaled_award_slzr);
	success_scaled_awards_tbl_off += serializer_get_offset(scaled_award_slzr->ctx, "awards");
	failure_scaled_awards_tbl_off += serializer_get_offset(scaled_award_slzr->ctx, "awards");

	PW_CHAIN_TABLE_FOREACH(task, taskf->tasks) {
		void *el;
		int i;

		adjust_award_rates(task + success_award_off);
		adjust_award_rates(task + failure_award_off);

		struct pw_chain_table *success_timed = *(void **)(task + success_timed_awards_tbl_off);
		struct pw_chain_table *failure_timed = *(void **)(task + failure_timed_awards_tbl_off);
		struct pw_chain_table *success_scaled = *(void **)(task + success_scaled_awards_tbl_off);
		struct pw_chain_table *failure_scaled = *(void **)(task + failure_scaled_awards_tbl_off);
		struct pw_chain_table *tables[] = { success_timed, failure_timed, success_scaled, failure_scaled };

		for (i = 0; i < sizeof(tables) / sizeof(tables[0]); i++) {
			struct pw_chain_table *table = tables[i];

			PW_CHAIN_TABLE_FOREACH(el, table) {
				adjust_award_rates(el);
			}
		}
	}
}
