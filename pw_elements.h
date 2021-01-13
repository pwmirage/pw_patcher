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
int pw_elements_save(struct pw_elements *el, const char *filename, bool is_server);
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
	struct pw_elements_chain *next;
	size_t capacity;
	size_t count;
	char data[0];
};

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

	struct pw_elements_table *tables[256];
	size_t tables_count;
};

#endif /* PW_ELEMENTS_H */
