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
	struct pw_pck_entry *next; /**< for putting in temporary lists */
};

/* special entries */
enum {
	PW_PCK_ENTRY_FREE_BLOCKS = 0,
	PW_PCK_ENTRY_ALIASES = 1,
	PW_PCK_ENTRY_MAX,
};

#define PW_PCK_XOR1 0xa8937462
#define PW_PCK_XOR2 0xf1a43653
struct pw_pck {
	char name[64];
	unsigned mg_version;
	FILE *fp;
	FILE *fp_log;
	FILE *fp_hdr;
	struct pw_pck_header hdr;
	uint32_t ver;
	uint32_t entry_cnt; /**< count of entries in pck->entries */
	uint32_t needs_update;
	struct pw_pck_footer ftr;
	struct pck_alias_tree *alias_tree;
	struct pw_pck_entry *entries;
	/** avl indexed by the alias name (or the org name if there's no alias) */
	struct pw_avl *entries_tree;
	struct pw_avl *free_blocks_tree;
	uint32_t new_entry_cnt; /**< count of entries to be written back into the pck */
	struct pw_pck_entry *new_entries; /**< entries to be written back into the pck */
	uint32_t file_append_offset;
};

enum pw_pck_action {
	PW_PCK_ACTION_EXTRACT,
	PW_PCK_ACTION_UPDATE,
	PW_PCK_ACTION_GEN_PATCH,
	PW_PCK_ACTION_APPLY_PATCH,
};

int pw_pck_open(struct pw_pck *pck, const char *path, enum pw_pck_action action);

#endif /* PW_PCK_H */
