/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

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
read_entry(struct pw_pck *pck, FILE *fp, size_t compressed_size)
{
	struct pw_pck_entry_header ent_hdr;
	int rc;

	if (compressed_size == sizeof(ent_hdr)) {
		fread(&ent_hdr, compressed_size, 1, fp);
	} else {
		char *buf = calloc(1, compressed_size);

		if (!buf) {
			PWLOG(LOG_ERROR, "calloc() failed\n");
			return -ENOMEM;
		}

		fread(buf, compressed_size, 1, fp);

		uLongf uncompressed_size = sizeof(ent_hdr);
		rc = uncompress((Bytef *)&ent_hdr, &uncompressed_size, (const Bytef *)buf, compressed_size);
		free(buf);
		if (rc != Z_OK || uncompressed_size != sizeof(ent_hdr)) {
			return rc;
		}
	}

	char utf8_name[396];
	char utf8_aliased_name[396];
	size_t aliased_len = 0;
	char *c;
	char *word;
	struct pck_alias_tree *aliases = pck->aliases;
	struct pck_alias *alias;

	change_charset("GB2312", "UTF-8", ent_hdr.path, sizeof(ent_hdr.path), utf8_name, sizeof(utf8_name));

	c = word = utf8_name;
	while (1) {
		if (*c == '\\' || *c == '/') {
			*c = 0;
			alias = get_alias(aliases, word);
			/* translate a dir */

			if (alias) {
				aliased_len += snprintf(utf8_aliased_name + aliased_len, sizeof(utf8_aliased_name) - aliased_len, "%s\\", alias->new_name);
				aliases = alias->sub_aliases;
			} else {
				aliased_len += snprintf(utf8_aliased_name + aliased_len, sizeof(utf8_aliased_name) - aliased_len, "%s\\", word);
			}

			word = c + 1;
		} else if (*c == 0) {
			alias = get_alias(aliases, word);
			/* translate the basename */

			if (alias) {
				aliased_len += snprintf(utf8_aliased_name + aliased_len, sizeof(utf8_aliased_name) - aliased_len, "%s", alias->new_name);
				aliases = alias->sub_aliases;
			} else {
				aliased_len += snprintf(utf8_aliased_name + aliased_len, sizeof(utf8_aliased_name) - aliased_len, "%s", word);
			}
			break;
		}

		c++;
	}


	PWLOG(LOG_INFO, "entry: %s\n", utf8_aliased_name);
	return 0;
}

int
pw_pck_read(struct pw_pck *pck, const char *path)
{
	FILE *fp;
	size_t fsize;
	int rc;
	char *buf;
	size_t buflen;

	readfile("patcher/shaders_alias.txt", &buf, &buflen);
	read_aliases(pck, buf);

	fp = fopen(path, "rb");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "fopen() failed: %d\n", errno);
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

	pck->ftr.entry_list_off ^= PW_PCK_XOR1;
	fseek(fp, pck->ftr.entry_list_off, SEEK_SET);

	for (int i = 0; i < pck->entry_cnt; i++) {
		uint32_t compressed_size;
		uint32_t compressed_size2;

		fread(&compressed_size, 4, 1, fp);
		compressed_size ^= PW_PCK_XOR1;

		fread(&compressed_size2, 4, 1, fp);
		compressed_size2 ^= PW_PCK_XOR2;

		if (compressed_size != compressed_size2) {
			PWLOG(LOG_ERROR, "Entry size mismatch: idx=%d, size1=%u, size2=%u\n",
					i, compressed_size, compressed_size2);
			goto err_cleanup;
		}

		rc = read_entry(pck, fp, compressed_size);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "read_entry() failed: %d\n", rc);
			goto err_cleanup;
		}
	}

	fclose(fp);
	return 0;

err_cleanup:
err_close:
	fclose(fp);
	return -1;
}
