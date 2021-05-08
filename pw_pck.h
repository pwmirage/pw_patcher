/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_PCK_H
#define PW_PCK_H

#include <stdint.h>
#include <inttypes.h>

#include "common.h"

extern HANDLE g_stdoutH;

#define PCK_HEADER_MAGIC0 0x4dca23ef
#define PCK_HEADER_MAGIC1 0x56a089b7
struct pw_pck_header {
	uint32_t magic0;
	uint32_t file_size;
	uint32_t magic1;
};

#define PCK_FOOTER_MAGIC0 0xfdfdfeee
#define PCK_FOOTER_MAGIC1 0xf00dbeef
struct pw_pck_footer {
	uint32_t magic0;
	uint32_t version;
	uint32_t entry_list_off;
	uint32_t flags;
	char description[252]; /* ASCI */
	uint32_t magic1;
	uint32_t entry_cnt;
	uint32_t ver;
};

struct pw_pck_entry_header {
	char path[260]; /* GBK. always 260 bytes = MAX_PATH */
	uint32_t offset;
	uint32_t length;
	uint32_t compressed_length;
	uint32_t unknown;
};

struct pw_pck_entry {
	struct pw_pck_entry_header hdr;
	char path_aliased_utf8[396];
	uint64_t mod_time;
	bool is_present;
	bool is_modified;
	/** for putting in pck->entries */
	struct pw_pck_entry *next;

	/** for gen-patch */
	bool in_gen_list;
	struct pw_pck_entry *gen_next;
};

#define PW_PCK_XOR1 0xa8937462
#define PW_PCK_XOR2 0xf1a43653
struct pw_pck {
	/** pck file basename without the extension */
	char name[64];
	/** 0 for non-mirage file */
	unsigned mg_version;
	/** headers taken straight from the pck */
	struct pw_pck_header hdr;
	struct pw_pck_footer ftr;

	/** pck file handle */
	FILE *fp;
	/** mgpck.log handle */
	FILE *fp_log;
	unsigned log_last_patch_pos;
	unsigned log_last_patch_line;

	/** linked list of entries parsed and/or to be written back into the pck */
	struct pw_pck_entry *entries;
	struct pw_pck_entry **entries_tail;
	/** special entries pointers for easy access */
	struct pw_pck_entry *entry_free_blocks;
	struct pw_pck_entry *entry_aliases;

	/** avl of pck_alias structs, indexed by both org and alias names */
	struct pck_alias_tree *alias_tree;
	char *alias_buf;

	/** top level pck_path_node-s */
	struct pw_avl *path_node_tree;
	/** tracked empty space inside the pck file */
	struct pw_avl *free_blocks_tree;

	/** if any file in .pck.files was changed */
	uint32_t needs_update;

	/** the new EOF counted from the beginning */
	uint32_t file_append_offset;
};

enum pw_pck_action {
	PW_PCK_ACTION_EXTRACT,
	PW_PCK_ACTION_UPDATE,
	PW_PCK_ACTION_GEN_PATCH,
	PW_PCK_ACTION_APPLY_PATCH,
};

int pw_pck_open(struct pw_pck *pck, const char *path);
int pw_pck_extract(struct pw_pck *pck, bool do_force);
int pw_pck_update(struct pw_pck *pck);
int pw_pck_gen_patch(struct pw_pck *pck, const char *patch_path, bool do_force);

#endif /* PW_PCK_H */
