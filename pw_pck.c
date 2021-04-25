/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <zlib.h>

#include "pw_pck.h"
#include "avl.h"

struct pck_alias_tree {
	struct pck_alias_tree *parent;
	struct pw_avl *avl;
};

struct pck_alias {
	char *org_name;
	char *new_name;
	struct pck_alias_tree *sub_aliases;
};

static void rmrf(char *path)
{
	SHFILEOPSTRUCT file_op = {
		NULL, FO_DELETE, path, "",
		FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
		false, 0, ""
	};
	SHFileOperation(&file_op);
}

static uint32_t
djb2(const char *str) {
	uint32_t hash = 5381;
	unsigned char c;

	while ((c = (unsigned char)*str++)) {
	    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

static struct pck_alias_tree *
alloc_tree(void)
{
	struct pck_alias_tree *tree;

	tree = calloc(1, sizeof(*tree));
	if (!tree) {
		return NULL;
	}

	tree->avl = pw_avl_init(sizeof(struct pck_alias));
	if (!tree->avl) {
		free(tree);
		return NULL;
	}

	return tree;
}

static int
read_aliases(struct pw_pck *pck, char *buf)
{
	struct pck_alias_tree *root_avl;
	struct pck_alias_tree *cur_avl;
	struct pck_alias *alias;
	unsigned lineno = 0;
	int nest_level = 0;

	root_avl = cur_avl = alloc_tree();
	if (!root_avl) {
		return -1;
	}

	char *c = buf;
	/* for each line */
	while (*c) {
		char *newname = NULL;
		char *orgname = NULL;
		int new_nest_level = 0;

		if (*c == '#') {
			/* skip line */
			while (*c != '\n' && *c != 0) {
				c++;
			}
			if (*c == '\n') {
				c++;
			}
			lineno++;
			continue;
		}

		while (*c == '\t') {
			new_nest_level++;
			c++;
		}

		if (*c == '\r' || *c == '\n') {
			/* empty line, skip it */
			if (*c == '\r') {
				c++;
			}
			if (*c == '\n') {
				c++;
			}
			lineno++;
			continue;
		}

		int nest_diff = new_nest_level - nest_level;
		if (nest_diff > 1) {
			PWLOG(LOG_ERROR, "found nested entry without a parent at line %d (too much indent)\n", lineno);
			return -1;

		} else if (nest_diff == 1) {
			if (!alias) {
				PWLOG(LOG_ERROR, "found nested entry without a parent at line %d\n", lineno);
				return -1;
			}

			/* set previous entry as root */
			assert(!alias->sub_aliases);
			alias->sub_aliases = alloc_tree();
			alias->sub_aliases->parent = cur_avl;
			if (!alias->sub_aliases) {
				PWLOG(LOG_ERROR, "alloc_tree() failed\n");
				return -1;
			}
			cur_avl = alias->sub_aliases;
		} else if (nest_diff < 0) {
			while (nest_diff < 0) {
				cur_avl = cur_avl->parent;
				assert(cur_avl);
				nest_diff++;
			}
		}
		nest_level = new_nest_level;

		newname = c;
		while (*c != '=' && *c != '\t' && *c != '\r' && *c != '\n' && *c != 0) {
			c++;
		}
		*c++ = 0;

		/* trim the newname */
		char *c2 = c - 2;
		while (*c2 == ' ' || *c2 == '\t') {
			*c2 = 0;
			c2--;
		}

		if (!newname) {
			PWLOG(LOG_ERROR, "empty alias at line %u\n", lineno);
			return -1;
		}

		/* trim the start of orgname */
		while (*c == ' ' || *c == '\t') {
			c++;
		}

		orgname = c;
		while (*c != '\r' && *c != '\n') {
			c++;
		}
		*c++ = 0;

		/* trim the end of orgname */
		c2 = c - 2;
		while (*c2 == ' ' || *c2 == '\t') {
			*c2 = 0;
			c2--;
		}

		if (*c == '\r') {
			c++;
		}
		if (*c == '\n') {
			c++;
		}

		if (!orgname) {
			PWLOG(LOG_ERROR, "empty filename at line %u\n", lineno);
			return -1;
		}

		alias = pw_avl_alloc(cur_avl->avl);
		if (!alias) {
			PWLOG(LOG_ERROR, "pw_avl_alloc() failed\n");
			return -1;
		}

		alias->new_name = newname;
		alias->org_name = orgname;
		unsigned hash = djb2(alias->org_name);
		pw_avl_insert(cur_avl->avl, hash, alias);
		lineno++;
	}

	pck->aliases = root_avl;
	return 0;
}

static struct pck_alias *
get_alias(struct pck_alias_tree *aliases, char *filename)
{
	struct pck_alias *alias;
	unsigned hash;

	if (!aliases) {
		return NULL;
	}

	hash = djb2(filename);
	alias = pw_avl_get(aliases->avl, hash);
	while (alias && strcmp(alias->org_name, filename) != 0) {
		alias = pw_avl_get_next(aliases->avl, alias);
	}

	return alias;
}

static int
read_entry_hdr(struct pw_pck_entry_header *hdr, FILE *fp)
{
	uint32_t compressed_size;
	uint32_t compressed_size2;
	int rc;

	fread(&compressed_size, 4, 1, fp);
	compressed_size ^= PW_PCK_XOR1;

	fread(&compressed_size2, 4, 1, fp);
	compressed_size2 ^= PW_PCK_XOR2;

	if (compressed_size != compressed_size2) {
		PWLOG(LOG_ERROR, "entry size mismatch: size1=%u, size2=%u\n",
				compressed_size, compressed_size2);
		return -EIO;
	}

	if (compressed_size == sizeof(*hdr)) {
		fread(hdr, compressed_size, 1, fp);
	} else {
		char *buf = calloc(1, compressed_size);

		if (!buf) {
			PWLOG(LOG_ERROR, "calloc() failed\n");
			return -ENOMEM;
		}

		fread(buf, compressed_size, 1, fp);

		uLongf uncompressed_size = sizeof(*hdr);
		rc = uncompress((Bytef *)hdr, &uncompressed_size, (const Bytef *)buf, compressed_size);
		free(buf);
		if (rc != Z_OK || uncompressed_size != sizeof(*hdr)) {
			return rc;
		}
	}

	return 0;
}

static void
alias_entry_path(struct pw_pck *pck, struct pw_pck_entry *ent, bool create_dirs)
{
	char utf8_path[396];
	char *c, *word;
	struct pck_alias_tree *aliases = pck->aliases;
	struct pck_alias *alias;
	size_t aliased_len = 0;

	change_charset("GB2312", "UTF-8", ent->hdr.path, sizeof(ent->hdr.path), utf8_path, sizeof(utf8_path));

	c = word = utf8_path;
	while (1) {
		if (*c == '\\' || *c == '/') {
			*c = 0;
			alias = get_alias(aliases, word);
			/* translate a dir */

			if (alias) {
				aliased_len += snprintf(ent->path_aliased_utf8 + aliased_len, sizeof(ent->path_aliased_utf8) - aliased_len, "%s/", alias->new_name);
				aliases = alias->sub_aliases;
			} else {
				aliased_len += snprintf(ent->path_aliased_utf8 + aliased_len, sizeof(ent->path_aliased_utf8) - aliased_len, "%s/", word);
			}

			if (create_dirs) {
				mkdir(ent->path_aliased_utf8, 0755);
			}

			word = c + 1;
		} else if (*c == 0) {
			alias = get_alias(aliases, word);
			/* translate the basename */

			if (alias) {
				aliased_len += snprintf(ent->path_aliased_utf8 + aliased_len, sizeof(ent->path_aliased_utf8) - aliased_len, "%s", alias->new_name);
				aliases = alias->sub_aliases;
			} else {
				aliased_len += snprintf(ent->path_aliased_utf8 + aliased_len, sizeof(ent->path_aliased_utf8) - aliased_len, "%s", word);
			}
			break;
		}

		c++;
	}
}

static int
extract_entry(struct pw_pck *pck, struct pw_pck_entry *ent, FILE *fp)
{
	char buf[16384];
	size_t remaining_bytes = ent->hdr.length;
	size_t read_bytes, buf_size;
	size_t cur_pos = ftell(fp);
	int rc;

	fseek(fp, ent->hdr.offset, SEEK_SET);

	FILE *fp_out = fopen(ent->path_aliased_utf8, "wb");

	if (ent->hdr.compressed_length >= ent->hdr.length) {
		/* not compressed */

		do {
			buf_size = MIN(sizeof(buf), remaining_bytes);
			read_bytes = fread(buf, 1, buf_size, fp);
			if (read_bytes != buf_size) {
				PWLOG(LOG_ERROR, "read error on %s:%s\n", pck->name, ent->path_aliased_utf8);
				fclose(fp_out);
				return -1;
			}

			fwrite(buf, 1, read_bytes, fp_out);
			remaining_bytes -= read_bytes;
		} while (remaining_bytes > 0);
	} else {
		rc = zpipe_uncompress(fp_out, fp, ent->hdr.compressed_length);
		if (rc != Z_OK) {
			PWLOG(LOG_ERROR, "uncompress error (%d) on %s:%s\n", rc, pck->name, ent->path_aliased_utf8);
			return -rc;
		}
	}

	fclose(fp_out);
	fseek(fp, cur_pos, SEEK_SET);

	return 0;
}

static uint64_t
get_cur_time(void)
{
	SYSTEMTIME st;
	FILETIME ft;

	GetLocalTime(&st);
	SystemTimeToFileTime(&st, &ft);

	return (((uint64_t)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

int
pw_pck_open(struct pw_pck *pck, const char *path, enum pw_pck_action action)
{
	FILE *fp;
	size_t fsize;
	int rc;
	char *alias_buf = NULL;
	size_t alias_buflen = 0;
	const char *c;
	char tmp[296];

	/* set pck->name to basename, without .pck extension */
	c = path + strlen(path) - 1;
	while (c != path && *c != '\\' && *c != '/') {
		c--;
	}
	
	if (*c == 0) {
		PWLOG(LOG_ERROR, "invalid file path: \"%s\"\n", path);
		return -EINVAL;
	}

	c++;
	snprintf(pck->name, sizeof(pck->name), "%.*s", strlen(c) - strlen(".pck"), c);

	/* read the file and check magic numbers at the very beginning */
	fp = fopen(path, "rb");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "fopen(\"%s\") failed: %d\n", path, errno);
		return -errno;
	}

	fread(&pck->hdr, sizeof(pck->hdr), 1, fp);
	if (pck->hdr.magic0 != PCK_HEADER_MAGIC0 || pck->hdr.magic1 != PCK_HEADER_MAGIC1) {
		PWLOG(LOG_ERROR, "invalid pck header: %s\n", path);
		goto err_close;
	}

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);

	if (fsize < 4) {
		goto err_close;
	}

	/* now check similar magic numbers at the end of file */
	fseek(fp, fsize - 4, SEEK_SET);
	fread(&pck->ver, 4, 1, fp);

	if (pck->ver != 0x20002 && pck->ver != 0x20001) {
		PWLOG(LOG_ERROR, "invalid pck version: 0x%x\n", pck->ver);
		goto err_close;
	}

	fseek(fp, fsize - 8, SEEK_SET);
	fread(&pck->entry_cnt, 4, 1, fp);

	fseek(fp, fsize - 8 - sizeof(struct pw_pck_footer), SEEK_SET);
	fread(&pck->ftr, sizeof(pck->ftr), 1, fp);

	if (pck->ftr.magic0 != PCK_FOOTER_MAGIC0 || pck->ftr.magic1 != PCK_FOOTER_MAGIC1) {
		PWLOG(LOG_ERROR, "invalid footer magic\n");
		goto err_close;
	}

	if(strstr(pck->ftr.description, "lica File Package") == NULL) {
		PWLOG(LOG_ERROR, "invalid pck description: \"%s\"\n", pck->ftr.description);
		goto err_close;
	}

	/* begin extracting */
	rc = GetCurrentDirectory(sizeof(tmp), tmp);
	snprintf(tmp + rc, sizeof(tmp) - rc, "\\%s.pck.files\x0\x0", pck->name);
	rc = access(tmp, F_OK);
	if (action == PW_PCK_ACTION_EXTRACT) {
		if (rc == 0) {
			/* TODO implement -f */
			//fprintf(stderr, "The pck was already extracted.\nPlease add \"-f\" flag if you want to override it");
			rmrf(tmp);
		}

		mkdir(tmp, 0755);
	} else {
		if (rc != 0) {
			fprintf(stderr, "Can't find \"%s\"\n", tmp);
			goto err_close;
		}
	}

	SetCurrentDirectory(tmp);

	uint64_t cur_time;
	cur_time = get_cur_time();

	snprintf(tmp, sizeof(tmp), "%s_mgpck.log", pck->name);
	rc = access(tmp, F_OK);

	if (rc != 0 && action != PW_PCK_ACTION_EXTRACT) {
		fprintf(stderr, "Can't find the log file (\"%s\")\n", tmp);
		goto err_close;
	}

	pck->fp_log = fopen(tmp, "a+");
	size_t log_endpos;

	if (rc != 0) {
		/* first write */
		fprintf(pck->fp_log, "#PW Mirage PCK Log: 1.0\r\n");
		fprintf(pck->fp_log, "#Keeps track of when each file was modified.\r\n");
		fprintf(pck->fp_log, "#This file is both written and read by mgpck.\r\n");
		fprintf(pck->fp_log, "\r\n");
	}

	fseek(pck->fp_log, 0, SEEK_END);
	log_endpos = ftell(fp);

	const char *action_str;
	switch (action) {
		case PW_PCK_ACTION_EXTRACT:
			action_str = "EXTRACT";
			break;
		case PW_PCK_ACTION_UPDATE:
			action_str = "UPDATE";
			break;
		case PW_PCK_ACTION_GEN_PATCH:
			action_str = "GEN_PATCH";
			break;
		case PW_PCK_ACTION_APPLY_PATCH:
			action_str = "APPLY_PATCH";
			break;
		default:
			assert(false);
			return -1;
	}

	fprintf(pck->fp_log, ":%s:%"PRIu64"\r\n", action_str, cur_time);

	pck->ftr.entry_list_off ^= PW_PCK_XOR1;
	fseek(fp, pck->ftr.entry_list_off, SEEK_SET);

	char desc[sizeof(pck->ftr.description)];
	unsigned mirage_version;
	int entry_idx = 0;

	rc = sscanf(pck->ftr.description, " pwmirage : %u : %[^:]", &mirage_version, desc);
	if (rc != 2) {
		/* not pwmirage package, make no assumptions to the content */
		mirage_version = 0;
		desc[0] = 0;

		if (action == PW_PCK_ACTION_EXTRACT) {
			snprintf(tmp, sizeof(tmp), "%s_aliases.cfg", pck->name);

			FILE *fp_tmp = fopen(tmp, "w");
			fprintf(fp_tmp, "#PW Mirage Filename Aliases: 1.0\r\n");
			fprintf(fp_tmp, "#Enables renaming chinese filenames to english. The filenames are translated\r\n");
			fprintf(fp_tmp, "#once when the pck is unpacked, then they are translated back when pck is updated.\r\n");
			fprintf(fp_tmp, "#\r\n");
			fprintf(fp_tmp, "#somedir = 布香\r\n");
			fprintf(fp_tmp, "#\tsomefile.sdr = 葬心林晶体.sdr\r\n");
			fprintf(fp_tmp, "#\tsomefile2.sdr = 焚香谷瀑布.sdr\r\n");
			fclose(fp_tmp);

			fprintf(pck->fp_log, "%s\n", tmp);
		}
	} else {
		struct pw_pck_entry ent;
		char tmp[100];
		/* a mirage package! */

		/* read the aliases first, to know how to extract the rest */
		rc = read_entry_hdr(&ent.hdr, fp);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "read_entry_hdr() failed: %d\n", rc);
			return rc;
		}

		snprintf(tmp, sizeof(tmp), "%s_aliases.cfg", pck->name);
		if (strcmp(ent.hdr.path, tmp) != 0) {
			PWLOG(LOG_ERROR, "Alias filename mismatch, got=\"%s\", expected=\"%s\"\n",
					ent.hdr.path, tmp);
			return -1;
		}

		rc = extract_entry(pck, &ent, fp);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "extract_entry(%d) failed: %d\n", entry_idx, rc);
			goto err_cleanup;
		}

		readfile(ent.path_aliased_utf8, &alias_buf, &alias_buflen);
		read_aliases(pck, alias_buf);

		if (action == PW_PCK_ACTION_EXTRACT) {
			fprintf(pck->fp_log, "%s\n", ent.path_aliased_utf8);
		}
		entry_idx++;
	}

	for (; entry_idx < pck->entry_cnt; entry_idx++) {
		struct pw_pck_entry ent;

		rc = read_entry_hdr(&ent.hdr, fp);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "read_entry_hdr() failed: %d\n", rc);
			return rc;
		}

		alias_entry_path(pck, &ent, action == PW_PCK_ACTION_EXTRACT);
		PWLOG(LOG_INFO, "entry: %s\n", ent.path_aliased_utf8);

		if (action == PW_PCK_ACTION_EXTRACT) {
			rc = extract_entry(pck, &ent, fp);
			if (rc != 0) {
				PWLOG(LOG_ERROR, "extract_entry(%d) failed: %d\n", entry_idx, rc);
				goto err_cleanup;
			}

			fprintf(pck->fp_log, "%s\n", ent.path_aliased_utf8);
		}
	}

	if (action == PW_PCK_ACTION_EXTRACT) {
	}

	/* go out of *.pck.files */
	SetCurrentDirectory("..");
	fclose(pck->fp_log);
	fclose(fp);
	return 0;

err_cleanup:
err_close:
	fclose(fp);
	return -1;
}
