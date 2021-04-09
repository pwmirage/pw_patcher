/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_ELEMENTS_H
#define PW_ELEMENTS_H

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

#ifdef __MINGW32__
#define char16_t uint16_t
#else
#include <uchar.h>
#endif

struct pw_elements;
struct cjson;
struct pw_idmap;

extern uint32_t g_elements_last_id;
extern struct pw_idmap *g_elements_map;

extern char g_icon_names[][64];
extern char g_item_colors[];
extern char *g_item_descs[];

int pw_elements_load(struct pw_elements *el, const char *filename, const char *idmap_filename);
int pw_elements_save(struct pw_elements *el, const char *filename, bool is_server);
int pw_elements_idmap_save(struct pw_elements *el, const char *filename);
void pw_elements_serialize(struct pw_elements *elements);
int pw_elements_patch_obj(struct pw_elements *elements, struct cjson *obj);
void pw_elements_adjust_rates(struct pw_elements *elements, struct cjson *rates);
void pw_elements_prepare(struct pw_elements *elements);

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

	void *talk_proc;
	uint32_t talk_proc_cnt;

	struct pw_chain_table *tables[256];
	size_t tables_count;
};

#endif /* PW_ELEMENTS_H */
