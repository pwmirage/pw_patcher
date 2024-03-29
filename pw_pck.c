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
#include <unistd.h>

#include <zlib.h>

/* windows specific */
#include <tchar.h>
#include <windows.h>
#include <winbase.h>

#include "pw_pck.h"
#include "avl.h"
#include "common.h"

/* enable fseek with uint32_t > 2GB */
#define fseek _fseeki64
#define ftell ftello64

static FILE *
wfopen(const char *path, const char *flags)
{
	size_t path_len = strlen(path);
	wchar_t *wpath = calloc(1, sizeof(wchar_t) * (path_len + 1));
	wchar_t wflags[4];
	FILE *fp;

	if (!wpath) {
		return NULL;
	}

	path_len = MultiByteToWideChar(CP_UTF8, 0, path, path_len, wpath, path_len);
	wpath[path_len] = 0;

	for (int i = 0; i < 4; i++) {
		wflags[i] = flags[i];
		if (wflags[i] == 0) {
			break;
		}
	}

	fp = _wfopen(wpath, wflags);

	free(wpath);
	return fp;
}

static void
wmkdir(const char *path)
{
	size_t path_len = strlen(path);
	wchar_t *wpath = calloc(1, sizeof(wchar_t) * (path_len + 1));

	if (!wpath) {
		return;
	}

	path_len = MultiByteToWideChar(CP_UTF8, 0, path, path_len, wpath, path_len);
	wpath[path_len] = 0;
	CreateDirectoryW(wpath, NULL);
}

static void
fp_truncate(FILE *fp, uint32_t size)
{
	/* truncate() doesn't work on >2GB files, use WinApi */
	fseek(fp, size, SEEK_SET);
	fflush(fp);

	HANDLE fh = (HANDLE)_get_osfhandle(_fileno(fp));
	SetEndOfFile(fh);
}

static int
pipe(FILE *dest, FILE *source, size_t remaining_bytes)
{
	size_t read_bytes, buf_size;
	char buf[16384];

	while (remaining_bytes > 0) {
		buf_size = MIN(sizeof(buf), remaining_bytes);
		read_bytes = fread(buf, 1, buf_size, source);
		if (read_bytes == 0 || ferror(source)) {
			return -errno;
		}

		fwrite(buf, 1, read_bytes, dest);
		remaining_bytes -= read_bytes;
	}

	return 0;
}

HANDLE g_stdoutH;

enum {
	COLOR_RED = FOREGROUND_INTENSITY | FOREGROUND_RED,
	COLOR_GREEN = FOREGROUND_INTENSITY | FOREGROUND_GREEN,
	COLOR_YELLOW = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
	COLOR_DEFAULT = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
};

static void
print_colored_utf8(int color, const char *fmt, ...)
{
	char buf[4096];
	va_list args;
	int rc;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (!g_stdoutH || g_stdoutH == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "%s", buf);
		return;
	}

	SetConsoleTextAttribute(g_stdoutH, color);

	wchar_t wbuf[sizeof(buf) + 1];
	rc = MultiByteToWideChar(CP_UTF8, 0, buf, strlen(buf), wbuf, sizeof(buf));
	wbuf[rc] = 0;

	DWORD written = 0;
	WriteConsoleW(g_stdoutH, wbuf, rc, &written, NULL);
	SetConsoleTextAttribute(g_stdoutH, COLOR_DEFAULT);
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

struct pck_alias_tree {
	struct pck_alias_tree *parent;
	struct pw_avl *avl; /**< indexed by org name */
	struct pw_avl *reverse_avl; /**< indexed by alias name */
};

struct pck_alias {
	char *org_name;
	char *new_name;
	struct pck_alias_tree *sub_aliases;
};

static struct pck_alias_tree *
alloc_alias_tree(void)
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

	tree->reverse_avl = pw_avl_init(sizeof(void *));
	if (!tree->reverse_avl) {
		free(tree->avl);
		free(tree);
		return NULL;
	}

	return tree;
}

static int
read_aliases(struct pw_pck *pck, const char *custom_name)
{
	struct pck_alias_tree *root_avl;
	struct pck_alias_tree *cur_avl;
	struct pck_alias *alias = NULL;
	struct pck_alias **reverse_alias;
	unsigned lineno = 0;
	int nest_level = 0;
	int rc;
	char tmp[64];
	size_t alias_buflen;

	if (custom_name) {
		snprintf(tmp, sizeof(tmp), "%s", custom_name);
	} else {
		snprintf(tmp, sizeof(tmp), "%s_aliases.cfg", pck->name);
	}

	rc = readfile(tmp, &pck->alias_buf, &alias_buflen);
	if (rc != 0) {
		PWLOG(LOG_ERROR, "readfile() failed: %d\n", rc);
		return rc;
	}

	root_avl = cur_avl = alloc_alias_tree();
	if (!root_avl) {
		return -1;
	}

	char *c = pck->alias_buf;
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
			alias->sub_aliases = alloc_alias_tree();
			alias->sub_aliases->parent = cur_avl;
			if (!alias->sub_aliases) {
				PWLOG(LOG_ERROR, "alloc_alias_tree() failed\n");
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
		while (*c != '\r' && *c != '\n' && *c != 0) {
			c++;
		}

		/* trim the end of orgname */
		c2 = c - 1;
		while (*c2 == ' ' || *c2 == '\t') {
			*c2 = 0;
			c2--;
		}

		if (*c != 0) {
			*c++ = 0;
		}
		if (*c == '\r') {
			c++;
		}
		if (*c == '\n') {
			c++;
		}

		if (!*orgname) {
			/* it's not really an alias, just a directory for nesting */
			orgname = newname;
		}

		alias = pw_avl_alloc(cur_avl->avl);
		if (!alias) {
			PWLOG(LOG_ERROR, "pw_avl_alloc() failed\n");
			return -1;
		}

		reverse_alias = pw_avl_alloc(cur_avl->reverse_avl);
		if (!reverse_alias) {
			PWLOG(LOG_ERROR, "pw_avl_alloc() failed\n");
			return -1;
		}

		alias->new_name = newname;
		alias->org_name = orgname;
		unsigned hash = djb2(alias->org_name);
		pw_avl_insert(cur_avl->avl, hash, alias);

		*reverse_alias = alias;
		hash = djb2(alias->new_name);
		pw_avl_insert(cur_avl->reverse_avl, hash, reverse_alias);
		lineno++;
	}

	pck->alias_tree = root_avl;
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

static struct pck_alias *
get_name_by_alias(struct pck_alias_tree *aliases, char *filename)
{
	struct pck_alias **alias_p;
	unsigned hash;

	if (!aliases) {
		return NULL;
	}

	hash = djb2(filename);
	alias_p = pw_avl_get(aliases->reverse_avl, hash);
	while (alias_p && strcmp((*alias_p)->new_name, filename) != 0) {
		alias_p = pw_avl_get_next(aliases->reverse_avl, *alias_p);
	}

	return alias_p ? *alias_p : NULL;
}

static int
read_entry_hdr(struct pw_pck *pck, FILE *fp, struct pw_pck_entry *ent)
{
	struct pw_pck_entry_header *hdr = &ent->hdr;
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
		fread(hdr, sizeof(*hdr), 1, fp);
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
		if (rc != Z_OK || (uncompressed_size != sizeof(*hdr) &&
					uncompressed_size != sizeof(*hdr) - 4)) {
			PWLOG(LOG_ERROR, "uncompress() failed, rc=%d, errno=%d, uncompressed_size=%u, expected=%u\n", rc, errno, uncompressed_size, sizeof(*hdr));
			return rc || -ENOSPC;
		}
	}

	return 0;
}

static int
undo_path_translation(struct pw_pck *pck, char *org_buf, size_t orgsize, char *aliased)
{
	char *c, *word;
	char bak;
	struct pck_alias_tree *aliases = pck->alias_tree;
	struct pck_alias *alias;
	size_t org_buf_pos = 0;

	c = word = aliased;
	while (1) {
		if (*c == '\\' || *c == '/') {
			bak = *c;
			*c = 0;

			alias = get_name_by_alias(aliases, word);
			if (alias) {
				aliases = alias->sub_aliases;
			}

			org_buf_pos += snprintf(org_buf + org_buf_pos, orgsize - org_buf_pos,
					"%s/", alias ? alias->org_name : word);
			*c = bak;
			word = c + 1;
		} else if (*c == 0) {

			alias = get_name_by_alias(aliases, word);
			if (alias) {
				aliases = alias->sub_aliases;
			}

			org_buf_pos += snprintf(org_buf + org_buf_pos, orgsize - org_buf_pos,
					"%s", alias ? alias->org_name : word);
			break;
		}

		c++;
	}

	return 0;
}

static int
write_entry_hdr(struct pw_pck *pck, FILE *fp, struct pw_pck_entry *ent)
{
	struct pw_pck_entry_header *hdr = &ent->hdr;
	uint32_t compressed_size;
	char *buf;
	uLongf bufsize = compressBound(sizeof(*hdr));
	int rc;

	if (hdr->path[0] == 0) {
		char buf[396];
		undo_path_translation(pck, buf, sizeof(buf), ent->path_aliased_utf8);

		change_charset("UTF-8", "GBK", buf, strlen(buf) + 1,
				hdr->path, sizeof(hdr->path));
	}

	buf = calloc(1, bufsize);
	if (!buf) {
		PWLOG(LOG_ERROR, "calloc() failed: %u\n", bufsize);
		return -ENOMEM;
	}

	rc = compress((Bytef *)buf, &bufsize, (const Bytef *)hdr, sizeof(*hdr));
	if (rc != Z_OK) {
		return rc;
	}

	compressed_size = ((uint32_t)bufsize) ^ PW_PCK_XOR1;
	fwrite(&compressed_size, 4, 1, fp);

	compressed_size = ((uint32_t)bufsize) ^ PW_PCK_XOR2;
	fwrite(&compressed_size, 4, 1, fp);

	if (bufsize >= sizeof(*hdr)) {
		fwrite(hdr, sizeof(*hdr), 1, fp);
	} else {
		fwrite(buf, bufsize, 1, fp);
	}

	free(buf);
	return 0;
}

/* each file or directory inside an entry's path */
struct pck_path_node {
	char *name; /* this particular file/dir basename */
	struct pw_pck_entry *entry; /**< set for files, NULL for directories */
	struct pw_avl *nested; /**< for directories only */
};

static struct pck_path_node *
get_path_node(struct pw_avl *parent, char *name)
{
	struct pck_path_node *file;
	unsigned hash;

	if (!parent) {
		return NULL;
	}

	hash = djb2(name);
	file = pw_avl_get(parent, hash);
	while (file && strcmp(file->name, name) != 0) {
		file = pw_avl_get_next(parent, file);
	}

	return file;
}

static struct pck_path_node *
set_path_node(struct pw_avl *parent, char *name, struct pw_pck_entry *ent)
{
	struct pck_path_node *file;
	unsigned hash;

	if (!parent) {
		return NULL;
	}

	hash = djb2(name);
	file = pw_avl_get(parent, hash);
	while (file && strcmp(file->name, name) != 0) {
		file = pw_avl_get_next(parent, file);
	}

	if (file) {
		return file;
	}

	file = pw_avl_alloc(parent);
	if (!file) {
		PWLOG(LOG_ERROR, "pw_avl_alloc() failed\n");
		return NULL;
	}

	file->name = name;
	if (ent) {
		file->entry = ent;
	} else {
		file->nested = pw_avl_init(sizeof(struct pck_path_node));
	}

	pw_avl_insert(parent, hash, file);
	return file;
}

/** convert the path to utf8, find an alias for it, fill in the entry avl */
static struct pw_pck_entry *
set_path_nodes(struct pw_pck *pck, struct pw_pck_entry *ent, bool create_dirs)
{
	char utf8_path[396];
	char *c, *word;
	struct pck_alias_tree *aliases = pck->alias_tree;
	struct pw_avl *parent = pck->path_node_tree;
	struct pck_path_node *file;
	struct pck_alias *alias;
	size_t aliased_len = 0;

	change_charset("GBK", "UTF-8", ent->hdr.path, sizeof(ent->hdr.path), utf8_path, sizeof(utf8_path));

	c = word = utf8_path;
	while (1) {
		if (*c == '\\' || *c == '/') {
			/* a dir */
			*c = 0;
			alias = get_alias(aliases, word);
			aliased_len += snprintf(ent->path_aliased_utf8 + aliased_len,
					sizeof(ent->path_aliased_utf8) - aliased_len, "%s/",
					alias ? alias->new_name : word);

			if (create_dirs) {
				wmkdir(ent->path_aliased_utf8);
			}

			if (alias) {
				aliases = alias->sub_aliases;
				file = set_path_node(parent, alias->new_name, NULL);
			} else {
				char *word_d = strdup(word); /* memleak, don't care */
				if (!word_d) {
					PWLOG(LOG_ERROR, "strdup() failed\n");
					return NULL;
				}

				file = set_path_node(parent, word_d, NULL);
			}

			if (!file) {
				PWLOG(LOG_ERROR, "set_path_node() failed\n");
				return NULL;
			}

			parent = file->nested;

			word = c + 1;
		} else if (*c == 0) {
			/* a file (because there can't be empty dirs in the pck) */
			alias = get_alias(aliases, word);
			aliased_len += snprintf(ent->path_aliased_utf8 + aliased_len,
					sizeof(ent->path_aliased_utf8) - aliased_len, "%s",
					alias ? alias->new_name : word);

			if (alias) {
				file = set_path_node(parent, alias->new_name, ent);
			} else {
				char *word_d = strdup(word); /* memleak, don't care */
				if (!word_d) {
					PWLOG(LOG_ERROR, "strdup() failed\n");
					return NULL;
				}

				file = set_path_node(parent, word_d, ent);
			}

			if (!file) {
				PWLOG(LOG_ERROR, "set_path_node() failed\n");
				return NULL;
			}

			return file->entry;
			break;
		}

		c++;
	}

	return NULL;
}

static void
append_entry(struct pw_pck *pck, struct pw_pck_entry *ent)
{
	*pck->entries_tail = ent;
	pck->entries_tail = &ent->next;
}

static int
read_pck_entry_list(struct pw_pck *pck, bool create_dirs)
{
	int rc;

	pck->path_node_tree = pw_avl_init(sizeof(struct pw_pck_entry));
	if (!pck->path_node_tree) {
		PWLOG(LOG_ERROR, "pw_avl_init() failed\n");
		return -1;
	}

	fseek(pck->fp, pck->ftr.entry_list_off, SEEK_SET);

	if (pck->mg_version == 0) {
		/* if it's not a mirage package we need a few extra entry slots at the beginning */
		struct pw_pck_entry *ent;

		ent = pck->entry_free_blocks;
		snprintf(ent->hdr.path, sizeof(ent->hdr.path),
				"%s_fragm.dat", pck->name);
		set_path_nodes(pck, ent, false);
		append_entry(pck, pck->entry_free_blocks);


		ent = pck->entry_aliases;
		snprintf(ent->hdr.path, sizeof(ent->hdr.path),
				"%s_aliases.cfg", pck->name);
		set_path_nodes(pck, ent, false);
		append_entry(pck, pck->entry_aliases);
	}

	struct pw_pck_entry *entry_arr = calloc(1, sizeof(*entry_arr) * pck->ftr.entry_cnt);
	if (!entry_arr) {
		PWLOG(LOG_ERROR, "calloc(%d) failed\n", sizeof(*entry_arr) * pck->ftr.entry_cnt);
		goto err_cleanup;
	}

	for (int i = 0; i < pck->ftr.entry_cnt; i++) {
		struct pw_pck_entry *ent = &entry_arr[i];

		if (pck->mg_version > 0) {
			switch (i) {
				case 0:
					ent = pck->entry_free_blocks;
					break;
				case 1:
					ent = pck->entry_aliases;
					break;
				default:
					break;
			}
		}

		rc = read_entry_hdr(pck, pck->fp, ent);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "read_entry_hdr() failed: %d\n", rc);
			goto err_cleanup;
		}

		ent->hdr.to_be_removed = 0;

		set_path_nodes(pck, ent, create_dirs);
		append_entry(pck, ent);

		PWLOG(LOG_DEBUG_5, "Read entry %u: \"%s\"\n", i, ent->path_aliased_utf8);
	}

	/* it won't be in the extracted files but don't remove it */
	pck->entry_free_blocks->is_present = true;

	pck->file_append_offset = pck->ftr.entry_list_off;
	pck->ftr.entry_list_off = 0;

	return 0;
err_cleanup:
	return -1;
}

static int
extract_entry(struct pw_pck *pck, struct pw_pck_entry *ent)
{
	FILE *fp = pck->fp;
	int rc;

	fseek(fp, ent->hdr.offset, SEEK_SET);

	FILE *fp_out = wfopen(ent->path_aliased_utf8, "wb");
	if (fp_out == NULL) {
		PWLOG(LOG_ERROR, "can't open %s\n", ent->path_aliased_utf8);
		return -1;
	}

	if (ent->hdr.compressed_length >= ent->hdr.length) {
		rc = pipe(fp_out, fp, ent->hdr.length);
		if (rc) {
			PWLOG(LOG_ERROR, "read error on %s:%s\n", pck->name, ent->path_aliased_utf8);
			fclose(fp_out);
			return rc;
		}
	} else {
		rc = zpipe_uncompress(fp_out, fp, ent->hdr.compressed_length);
		if (rc != Z_OK) {
			PWLOG(LOG_ERROR, "uncompress error (%d - %d) on %s:%s\n", rc, errno, pck->name, ent->path_aliased_utf8);
			fclose(fp_out);
			return rc;
		}
	}

	fflush(fp_out);
	fclose(fp_out);

	return 0;
}

static uint64_t
get_cur_time(char *buf, size_t buflen)
{
	SYSTEMTIME st;
	FILETIME ft;
	size_t date_len;

	/* get utc time */
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);

	/* pretty print time in local timezone */
	GetLocalTime(&st);
	date_len = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buf, buflen);

	if (date_len >= buflen) {
		/* no space for time, just return */
		return (((uint64_t)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
	}

	buf[date_len - 1] = ' ';
	buf += date_len;
	buflen -= date_len;

	GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, buf, buflen);

	return (((uint64_t)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

static int
read_log(struct pw_pck *pck)
{
	char tmp[128];
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int rc;
	uint64_t cur_modtime = 0;
	unsigned lineno = 0;
	struct pw_avl *files;
	struct pck_path_node *file;
	bool save_patch_pos = false;

	snprintf(tmp, sizeof(tmp), "%s_mgpck.log", pck->name);
	fp = pck->fp_log = fopen(tmp, "r+");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "Cant open the log file: %d (\"%s\")\n", errno, tmp);
		return -errno;
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		bool is_empty = true;
		char *c, *word;
		uint64_t tmp_modtime = cur_modtime;

		assert(line[0] != 0);
		/* comment */
		if (line[0] == '#' || line[0] == '\r' || line[0] == '\n') {
			lineno++;
			continue;
		}

		c = line;
		while (*c) {
			if(*c != '\n' && *c != '\t' && *c != '\r' && *c != ' ') {
				is_empty = false;
				break;
			}

			c++;
		}

		if (is_empty) {
			continue;
		}

		/* header */
		if (line[0] == ':') {
			rc = sscanf(line, " : %127[^:] : %"SCNu64" : %*s", tmp, &cur_modtime);
			if (rc != 2) {
				PWLOG(LOG_ERROR, "Incomplete header at line %u\n\t\"%s\"\n", lineno, line);
				return -EIO;
			}

			/* save some metadata for possible gen_patch */
			if (save_patch_pos) {
				pck->log_last_patch_pos = ftell(pck->fp_log) - strlen(line) - 1;
				pck->log_last_patch_line = lineno;
				save_patch_pos = false;
			}
			if (strcmp(tmp, "EXTRACT") == 0 || strcmp(tmp, "GEN_PATCH") == 0) {
				save_patch_pos = true;
			}
			lineno++;
			continue;
		}

		/* filename */
		files = pck->path_node_tree;
		c = word = line;
		if (line[0] == '?') {
			tmp_modtime = 0;
			c++;
			word++;
		}
		while (1) {
			if (*c == '\\' || *c == '/') {
				*c = 0;
				file = get_path_node(files, word);
				if (!file) {
					/* we got a log entry for a file that doesn't exist in the pck.
					 * it could have been removed or aliased - no problem.
					 */
					break;
				}

				files = file->nested;
				word = c + 1;
			} else if (*c == '\n' || *c == 0) {
				*c = 0;

				file = get_path_node(files, word);
				if (!file) {
					/* same as above */
					break;
				}

				if (!file->entry) {
					/* we got a log entry for a directory? weird, ignore it */
					break;
				}

				file->entry->mod_time = tmp_modtime;
				break;
			}

			c++;
		}

		lineno++;
	}

	fseek(pck->fp_log, 0, SEEK_END);

	if (save_patch_pos) {
		pck->log_last_patch_pos = 0;
	}

	return 0;
}

static int
get_updated_files_by_log(struct pw_pck *pck, struct pw_pck_entry **list)
{
	char tmp[128];
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int rc;
	uint64_t cur_modtime = 0;
	unsigned lineno = pck->log_last_patch_line;
	struct pw_avl *files;
	struct pck_path_node *file = NULL;
	int cur_section = -1;

	if (!pck->log_last_patch_pos) {
		/* nothing new in the log file, return nothing */
		return 0;
	}

	/* clear EOF for getline */
	rewind(pck->fp_log);
	fseek(pck->fp_log, pck->log_last_patch_pos, SEEK_SET);

	while ((read = getline(&line, &len, pck->fp_log)) != -1) {
		bool is_empty = true;
		char *c, *word;
		uint64_t tmp_modtime = cur_modtime;

		assert(line[0] != 0);
		/* comment */
		if (line[0] == '#' || line[0] == '\r' || line[0] == '\n') {
			lineno++;
			continue;
		}

		c = line;
		while (*c) {
			if(*c != '\n' && *c != '\t' && *c != '\r' && *c != ' ') {
				is_empty = false;
				break;
			}

			c++;
		}

		if (is_empty) {
			continue;
		}

		/* header */
		if (line[0] == ':') {
			rc = sscanf(line, " : %127[^:] : %"SCNu64" : %*s", tmp, &cur_modtime);
			if (rc != 2) {
				PWLOG(LOG_ERROR, "Incomplete header at line %u\n\t\"%s\"\n", lineno, line);
				return -EIO;
			}

			if (strcmp(tmp, "APPLY_PATCH") == 0) {
				cur_section = PW_PCK_ACTION_APPLY_PATCH;
			} else if (strcmp(tmp, "GEN_PATCH") == 0) {
				cur_section = PW_PCK_ACTION_GEN_PATCH;
			} else if (strcmp(tmp, "EXTRACT") == 0) {
				cur_section = PW_PCK_ACTION_EXTRACT;
			} else {
				cur_section = PW_PCK_ACTION_UPDATE;
			}

			lineno++;
			continue;
		}

		if (cur_section == -1) {
			PWLOG(LOG_ERROR, "Entry with no preceeding header at line %u\n\t\"%s\"\n", lineno, line);
			return -EIO;
		}

		/* filename */
		if (line[0] == '?') {
			tmp_modtime = 0;
			line++;
		}
		files = pck->path_node_tree;
		c = word = line;
		while (1) {
			if (*c == '\\' || *c == '/') {
				*c = 0;
				file = get_path_node(files, word);
				if (!file) {
					/* we got a log entry for a file that doesn't exist in the pck.
					 * it could have been removed or aliased - no problem.
					 */
					break;
				}

				files = file->nested;
				word = c + 1;
			} else if (*c == '\n' || *c == 0) {
				*c = 0;

				file = get_path_node(files, word);
				if (!file) {
					/* same as above */
					break;
				}

				if (!file->entry) {
					/* we got a log entry for a directory? weird, ignore it */
					break;
				}

				file->entry->mod_time = tmp_modtime;
				break;
			}

			c++;
		}

		if (file && file->entry) {
			if (cur_section == PW_PCK_ACTION_APPLY_PATCH) {
				/* if it was queued before unqueue it now */
				if (file->entry->gen_prev_next_p) {
					*file->entry->gen_prev_next_p = file->entry->gen_next;
					file->entry->gen_prev_next_p = NULL;
					file->entry->gen_next = NULL;
				}
			} else if (!file->entry->gen_prev_next_p) {
				file->entry->gen_next = *list;
				file->entry->gen_prev_next_p = list;

				if (*list) {
					(*list)->gen_prev_next_p = &file->entry->gen_next;
				}

				*list = file->entry;
			}
		}

		lineno++;
	}

	fseek(pck->fp_log, 0, SEEK_END);
	return 0;
}

#ifdef __MINGW32__
#include <tchar.h>
#include <windows.h>
#include <winbase.h>
#include <errno.h>

#define FindExInfoBasic 1

static int
_find_modified_files(struct pw_pck *pck, wchar_t *path, struct pw_avl *files, bool do_log)
{
	struct pw_pck_entry *entry;
	struct pck_path_node *file;
	WIN32_FIND_DATAW fdata;
	HANDLE handle;
	DWORD rc;

	handle = FindFirstFileExW(path, FindExInfoBasic, &fdata, FindExSearchNameMatch, NULL, 0);
	if (handle == INVALID_HANDLE_VALUE) {
		return -1;
	}

	do {
		uint64_t write_time = (((uint64_t)fdata.ftLastWriteTime.dwHighDateTime) << 32) |
				fdata.ftLastWriteTime.dwLowDateTime;
		uint32_t fsize = fdata.nFileSizeLow; /**< dont expect files > 4gb */

		char utf8_name[1024];
		WideCharToMultiByte(CP_UTF8, 0, fdata.cFileName, -1, utf8_name, sizeof(utf8_name), NULL, NULL);

		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (strcmp(utf8_name, ".") == 0 || strcmp(utf8_name, "..") == 0) {
				continue;
			}

			wchar_t buf[512];
			if (path[0] == '.' && path[1] == '\\') {
				_snwprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%s\\*", fdata.cFileName);
			} else {
				_snwprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%.*s\\%s\\*", wcslen(path) - 2, path, fdata.cFileName);
			}

			file = get_path_node(files, utf8_name);
			_find_modified_files(pck, buf, file ? file->nested : NULL, do_log);
		} else {

			if (strstr(utf8_name, "_mgpck.log") != NULL || strstr(utf8_name, "_fragm.dat") != NULL) {
				continue;
			}

			file = get_path_node(files, utf8_name);
			if (file) {
				assert(file->entry);
				entry = file->entry;
				entry->is_present = true;
				if (write_time <= entry->mod_time && fsize == entry->hdr.length) {
					/* nothing new */
					continue;
				}


				entry->is_modified = true;
				entry->hdr.length = fsize;

				pck->needs_update = true;
				print_colored_utf8(COLOR_YELLOW, "\t+%s\n", entry->path_aliased_utf8);
			} else {
				/* a brand new file */
				entry = calloc(1, sizeof(*entry));
				if (!entry) {
					PWLOG(LOG_ERROR, "calloc() failed\n");
					return -ENOMEM;
				}

				entry->is_present = true;
				entry->is_modified = true;
				entry->hdr.length = fsize;

				if (path[0] == '.' && path[1] == '\\') {
					snprintf(entry->path_aliased_utf8, sizeof(entry->path_aliased_utf8),
							"%s", utf8_name);
				} else {
					char *c = entry->path_aliased_utf8;

					rc = WideCharToMultiByte(CP_UTF8, 0, path, wcslen(path) - 2, entry->path_aliased_utf8, sizeof(entry->path_aliased_utf8), NULL, NULL);

					snprintf(entry->path_aliased_utf8 + rc,
							sizeof(entry->path_aliased_utf8) - rc,
							"\\%s", utf8_name);

					while (*c) {
						if (*c == '\\') {
							*c = '/';
						}
						c++;
					}
				}

				append_entry(pck, entry);
				pck->needs_update = true;
				print_colored_utf8(COLOR_GREEN, "\t+%s\n", entry->path_aliased_utf8);
			}
			if (do_log) {
				fprintf(pck->fp_log, "%s\n", entry->path_aliased_utf8);
			}

		}
	} while (FindNextFileW(handle, &fdata) != 0);

	rc = GetLastError();
	FindClose(handle);

	if (rc != ERROR_NO_MORE_FILES) {
		return -rc;
	}

	return 0;
}
#endif

static int add_free_block(struct pw_pck *pck, uint32_t offset, uint32_t size);

static int
find_modified_files(struct pw_pck *pck, bool do_log)
{
#ifdef __MINGW32__
	struct pw_pck_entry **ep;
	int rc;

	rc = _find_modified_files(pck, L".\\*", pck->path_node_tree, do_log);
	if (rc) {
		return rc;
	}

	/* remove deleted files from pck->entries and free up the space
	 * after them */
	ep = &pck->entries;
	while (*ep) {
		struct pw_pck_entry *entry = *ep;

		if (entry->is_present) {
			ep = &(*ep)->next;
			continue;
		}

		add_free_block(pck, entry->hdr.offset,
				MIN(entry->hdr.compressed_length, entry->hdr.length));
		if (do_log) {
			fprintf(pck->fp_log, "?%s\n", entry->path_aliased_utf8);
		}
		print_colored_utf8(COLOR_RED, "\t-%s\n", entry->path_aliased_utf8);
		pck->needs_update = true;

		/* remove from the list */
		*ep = (*ep)->next;
	}

	return 0;
#else
	return -ENOSYS;
#endif
}

/** unused space inside the pck */
struct pck_free_block {
	uint32_t offset;
	uint32_t size;
};

struct pck_free_blocks_meta {
	uint32_t block_cnt;
};

static void
try_read_free_blocks(struct pw_pck *pck)
{
	struct pck_free_blocks_meta meta;

	if (pck->is_free_blocks_read || pck->mg_version == 0 ||
			pck->entry_free_blocks->hdr.length == 0) {
		return;
	}

	fseek(pck->fp, pck->entry_free_blocks->hdr.offset, SEEK_SET);
	fread(&meta, sizeof(meta), 1, pck->fp);

	for (int i = 0; i < meta.block_cnt; i++) {
		struct pck_free_block *block = pw_avl_alloc(pck->free_blocks_tree);

		fread(block, sizeof(*block), 1, pck->fp);
		pw_avl_insert(pck->free_blocks_tree, block->size, block);
	}

	pck->is_free_blocks_read = true;
}

static int
add_free_block(struct pw_pck *pck, uint32_t offset, uint32_t size)
{
	struct pck_free_block *block;

	block = pw_avl_alloc(pck->free_blocks_tree);
	if (!block) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		return -ENOMEM;
	}

	block->offset = offset;
	block->size = size;
	pw_avl_insert(pck->free_blocks_tree, block->size, block);
	return 0;
}

static uint32_t
get_free_block(struct pw_pck *pck, uint32_t min_size)
{
	uint32_t ret;
	struct pw_avl_node *node = pck->free_blocks_tree->root;
	struct pw_avl_node *prev_node = NULL;

	while (node) {
		if (min_size > node->key) {
			/* node not big enough */
			node = node->right;
		} else {
			/* store the last big enough node, then try a smaller one */
			prev_node = node;
			node = node->left;
		}
	}

	if (prev_node) {
		struct pck_free_block *block = (void *)prev_node->data;

		pw_avl_remove(pck->free_blocks_tree, block);
		ret = block->offset;

		/* if the remainder is smaller than 2*sizeof(pck_free_block) then there's
		 * no point storing it ... */
		if (block->size - min_size > 16) {
			block->offset += min_size;
			block->size -= min_size;
			pw_avl_insert(pck->free_blocks_tree, block->size, block);
		} else {
			pw_avl_free(pck->free_blocks_tree, block);
		}

		return ret;
	}

	ret = pck->file_append_offset;
	pck->file_append_offset += min_size;

	return ret;
}

static void
_write_free_block(FILE *fp, struct pw_avl_node *node)
{
	struct pck_free_block *block;

	if (!node) {
		return;
	}

	block = (void *)node->data;
	fwrite(block, sizeof(*block), 1, fp);

	_write_free_block(fp, node->next);
	_write_free_block(fp, node->left);
	_write_free_block(fp, node->right);
}

static int
write_free_blocks(struct pw_pck *pck)
{
	struct pw_pck_entry *ent = pck->entry_free_blocks;
	struct pck_free_blocks_meta meta;
	size_t fsize;

	/* re-write free blocks */
	if (ent->hdr.offset) {
		add_free_block(pck, ent->hdr.offset, ent->hdr.length);
	}

	fsize = sizeof(struct pck_free_blocks_meta) + pck->free_blocks_tree->el_count *
			sizeof(struct pck_free_block);
	ent->hdr.offset = get_free_block(pck, fsize);

	fseek(pck->fp, ent->hdr.offset, SEEK_SET);
	/* we want this file uncompressed, so fake the compress length */
	ent->hdr.compressed_length = fsize + 1;

	meta.block_cnt = pck->free_blocks_tree->el_count;
	fwrite(&meta, sizeof(meta), 1, pck->fp);

	_write_free_block(pck->fp, pck->free_blocks_tree->root);
	return 0;
}

static int
write_entry(struct pw_pck *pck, struct pw_pck_entry *e)
{
	char *buf_compressed, *buf_uncompressed;
	uLongf buf_compsize;
	uint32_t prev_len = 0;
	uint32_t new_len;
	int rc;

	FILE *fp_in = wfopen(e->path_aliased_utf8, "rb");
	if (fp_in == NULL) {
		PWLOG(LOG_ERROR, "can't open \"%s\": %d\n", e->path_aliased_utf8, errno);
		return -errno;
	}

	buf_uncompressed = calloc(1, e->hdr.length);
	if (!buf_uncompressed) {
		PWLOG(LOG_ERROR, "calloc(%u) failed\n", buf_compsize);
		return -ENOMEM;
	}

	buf_compsize = compressBound(e->hdr.length);
	buf_compressed = calloc(1, buf_compsize);
	if (!buf_compressed) {
		PWLOG(LOG_ERROR, "calloc(%u) failed\n", buf_compsize);
		free(buf_uncompressed);
		return -ENOMEM;
	}

	fread(buf_uncompressed, e->hdr.length, 1, fp_in);
	fclose(fp_in);

	rc = compress((Bytef *)buf_compressed, &buf_compsize, (const Bytef *)buf_uncompressed, e->hdr.length);
	if (rc != Z_OK) {
		free(buf_uncompressed);
		free(buf_compressed);
		return rc;
	}

	prev_len = MIN(e->hdr.compressed_length, e->hdr.length);
	e->hdr.compressed_length = buf_compsize;
	new_len = MIN(e->hdr.compressed_length, e->hdr.length);
	/* prev_len will be always 0 for newly added files */
	if (new_len != prev_len) {
		if (prev_len) {
			rc = add_free_block(pck, e->hdr.offset, prev_len);
			if (rc) {
				free(buf_compressed);
				free(buf_uncompressed);
				return rc;
			}
		}

		e->hdr.offset = get_free_block(pck, new_len);
	}

	fseek(pck->fp, e->hdr.offset, SEEK_SET);
	if (e->hdr.length <= e->hdr.compressed_length) {
		fwrite(buf_uncompressed, e->hdr.length, 1, pck->fp);
	} else {
		fwrite(buf_compressed, e->hdr.compressed_length, 1, pck->fp);
	}

	free(buf_compressed);
	free(buf_uncompressed);

	return 0;
}

static int
write_modified_pck_files(struct pw_pck *pck)
{
	int rc;

	struct pw_pck_entry *e = pck->entries;
	while (e) {
		if (!e->is_modified) {
			e = e->next;
			continue;
		}

		rc = write_entry(pck, e);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "write_entry(\"%s\") failed: %d\n",
					e->path_aliased_utf8, rc);
			return rc;
		}

		e->is_modified = false;
		e = e->next;
	}

	return 0;
}

static int
rewrite_entry_hdr_list(struct pw_pck *pck)
{
	struct pw_pck_entry **ep;
	int rc;

	fseek(pck->fp, pck->file_append_offset, SEEK_SET);
	pck->ftr.entry_list_off = pck->file_append_offset;
	pck->ftr.entry_cnt = 0;

	ep = &pck->entries;
	while (*ep) {
		if ((*ep)->hdr.to_be_removed) {
			*ep = (*ep)->next;
			PWLOG(LOG_DEBUG_5, "Skipping entry %u: \"%s\"\n", pck->ftr.entry_cnt, (*ep)->path_aliased_utf8);
			continue;
		}

		rc = write_entry_hdr(pck, pck->fp, *ep);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "write_entry_hdr(\"%s\") failed: %d\n",
					(*ep)->path_aliased_utf8, rc);
			return rc;
		}

		PWLOG(LOG_DEBUG_5, "Saving entry %u: \"%s\"\n", pck->ftr.entry_cnt, (*ep)->path_aliased_utf8);
		pck->ftr.entry_cnt++;
		ep = &(*ep)->next;
	}

	pck->file_append_offset = ftell(pck->fp);
	return 0;
}

static int
write_ftr(struct pw_pck *pck)
{
	fseek(pck->fp, pck->file_append_offset, SEEK_SET);

	PWLOG(LOG_DEBUG_1, "Wrote %u pck entries\n", pck->ftr.entry_cnt);

	pck->ftr.entry_list_off ^= PW_PCK_XOR1;
	snprintf(pck->ftr.description, sizeof(pck->ftr.description), "pwmirage :1 :Angelica File Package");

	fwrite(&pck->ftr, sizeof(pck->ftr), 1, pck->fp);
	pck->file_append_offset += sizeof(pck->ftr);

	pck->hdr.file_size = ftell(pck->fp);
	fseek(pck->fp, 0, SEEK_SET);
	fwrite(&pck->hdr, sizeof(pck->hdr), 1, pck->fp);

	fp_truncate(pck->fp, pck->file_append_offset);
	return 0;
}

int
pw_pck_open(struct pw_pck *pck, const char *path) {

	char tmp[296];
	const char *c, *last_dot = NULL;
	uint32_t fsize;
	int rc;

	fprintf(stderr, "Reading pck ...\n");

	pck->entries_tail = &pck->entries;

	c = path + strlen(path) - 1;
	while (c != path && *c != '\\' && *c != '/') {
		if (*c == '.' && !last_dot) {
			last_dot = c;
		}
		c--;
	}

	if (*c == 0) {
		PWLOG(LOG_ERROR, "invalid file path: \"%s\"\n", path);
		return -EINVAL;
	}

	if (c != path) {
		c++;
	}
	snprintf(pck->fullname, sizeof(pck->fullname), "%s", c);
	snprintf(pck->name, sizeof(pck->name), "%.*s", last_dot - c, c);

	pck->fp = fopen(path, "r+b");
	if (pck->fp == NULL) {
		PWLOG(LOG_ERROR, "fopen(\"%s\") failed: %d\n", path, errno);
		return -errno;
	}

	/* read the file and check magic numbers at the very beginning */
	fread(&pck->hdr, sizeof(pck->hdr), 1, pck->fp);
	if (pck->hdr.magic0 != PCK_HEADER_MAGIC0 || pck->hdr.magic1 != PCK_HEADER_MAGIC1) {
		PWLOG(LOG_ERROR, "not a pck file\n");
		rc = -EINVAL;
		goto err_close;
	}

	fsize = pck->hdr.file_size;

	if (fsize < sizeof(pck->hdr) + sizeof(pck->ftr)) {
		PWLOG(LOG_ERROR, "file way too small\n");
		rc = -EINVAL;
		goto err_close;
	}

	/* now check similar magic numbers at the end of file */
	fseek(pck->fp, fsize - sizeof(struct pw_pck_footer), SEEK_SET);
	fread(&pck->ftr, sizeof(pck->ftr), 1, pck->fp);

	PWLOG(LOG_DEBUG_1, "fsize: 0x%x\n", fsize);
	PWLOG(LOG_DEBUG_1, "ver: 0x%x\n", pck->ftr.ver);
	PWLOG(LOG_DEBUG_1, "flags: 0x%x\n", pck->ftr.flags);

	if (pck->ftr.ver != 0x20002 && pck->ftr.ver != 0x20001 && pck->ftr.ver != 0x30000) {
		PWLOG(LOG_ERROR, "invalid pck version: 0x%x\n", pck->ftr.ver);
		rc = -EINVAL;
		goto err_close;
	}

	if (pck->ftr.magic0 != PCK_FOOTER_MAGIC0 || pck->ftr.magic1 != PCK_FOOTER_MAGIC1) {
		PWLOG(LOG_ERROR, "invalid footer magic\n");
		rc = -EINVAL;
		goto err_close;
	}

	rc = sscanf(pck->ftr.description, " pwmirage : %u : %[^:]", &pck->mg_version, tmp);
	if (rc != 2) {
		pck->mg_version = 0;
	}

	pck->ftr.entry_list_off ^= PW_PCK_XOR1;

	pck->entry_free_blocks = calloc(1, sizeof(*pck->entry_free_blocks));
	pck->entry_aliases = calloc(1, sizeof(*pck->entry_aliases));
	if (!pck->entry_free_blocks || !pck->entry_aliases) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		free(pck->entry_free_blocks);
		free(pck->entry_aliases);
		goto err_close;
	}

	pck->free_blocks_tree = pw_avl_init(sizeof(struct pck_free_block));
	if (!pck->free_blocks_tree) {
		PWLOG(LOG_ERROR, "pw_avl_init() failed\n");
		free(pck->entry_free_blocks);
		free(pck->entry_aliases);
		rc = -ENOMEM;
		goto err_close;
	}

	PWLOG(LOG_DEBUG_1, "Read %u pck entries\n", pck->ftr.entry_cnt);

	return 0;
err_close:
	fclose(pck->fp);
	pck->fp = NULL;
	return rc;
}

int
pw_pck_extract(struct pw_pck *pck, bool do_force)
{
	char tmp[296];
	struct pw_pck_entry *ent;
	int rc;

	fprintf(stderr, "Extracting ...\n");

	snprintf(tmp, sizeof(tmp), "%s.files", pck->fullname);
	rc = access(tmp, F_OK);
	if (rc == 0 && !do_force) {
		print_colored_utf8(COLOR_RED, "Error: ");
		fprintf(stderr, "%s.files already exists. Run mgpck with the `--force` flag to override files inside, or please delete that directory manually.\n", pck->fullname);
		return 0;
	}

	wmkdir(tmp);

	SetCurrentDirectory(tmp);

	/* Extract the aliases first -> we'll need them early */
	if (pck->mg_version > 0) {
		struct pw_pck_entry *ent = pck->entry_aliases;

		fseek(pck->fp, pck->ftr.entry_list_off, SEEK_SET);

		/* skip the first entry (free blocks) */
		rc = read_entry_hdr(pck, pck->fp, ent);
		rc = rc || read_entry_hdr(pck, pck->fp, ent);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "read_entry_hdr() failed: %d\n", rc);
			goto out;
		}

		if (strstr(ent->hdr.path, "_aliases.cfg") == NULL) {
			PWLOG(LOG_ERROR, "Alias filename mismatch, got=\"%s\"\n",
					ent->hdr.path);
			rc = -EIO;
			goto out;
		}

		snprintf(ent->path_aliased_utf8, sizeof(ent->path_aliased_utf8),
				"%s", ent->hdr.path);

		rc = extract_entry(pck, ent);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "extract_entry(%d) failed: %d\n", 0, rc);
			goto out;
		}

		rc = read_aliases(pck, ent->hdr.path);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "read_aliases(%d) failed: %d\n", 0, rc);
			goto out;
		}
	}

	rc = read_pck_entry_list(pck, true);
	if (rc != 0) {
		goto out;
	}

	/* begin extracting */
	snprintf(tmp, sizeof(tmp), "%s_mgpck.log", pck->name);
	pck->fp_log = fopen(tmp, "w");

	fprintf(pck->fp_log, "#PW Mirage PCK Log: 1.0\n");
	fprintf(pck->fp_log, "#Keeps track of when each file was modified.\n");
	fprintf(pck->fp_log, "#This file is both written and read by mgpck.\n");
	fprintf(pck->fp_log, "\n");

	/* write the header just to allocate bytes in the file, this will be overritten soon */
	uint64_t cur_time;
	size_t log_hdr_pos;
	cur_time = get_cur_time(tmp, sizeof(tmp));

	log_hdr_pos = ftell(pck->fp_log);
	fprintf(pck->fp_log, ":EXTRACT:%020"PRIu64":%28s\n", cur_time, tmp);

	ent = pck->entries;

	if (pck->mg_version == 0) {
		/* generate files which should've been inside the pck */
		snprintf(tmp, sizeof(tmp), "%s_aliases.cfg", pck->name);

		FILE *fp_tmp = fopen(tmp, "w");
		fprintf(fp_tmp, "#PW Mirage Filename Aliases: 1.0\n");
		fprintf(fp_tmp, "#Enables renaming chinese filenames to english. The filenames are translated\n");
		fprintf(fp_tmp, "#once when the pck is unpacked, then they are translated back when pck is updated.\n");
		fprintf(fp_tmp, "#\n");
		fprintf(fp_tmp, "#somedir = 布香\n");
		fprintf(fp_tmp, "#\tsomefile.sdr = 葬心林晶体.sdr\n");
		fprintf(fp_tmp, "#\tsomefile2.sdr = 焚香谷瀑布.sdr\n");
		fclose(fp_tmp);
	}

	/* the first entry is always the list of free blocks, don't extract
	 * it -> it should live inside the pck only */
	ent = ent->next;

	/* the second entry is the log and it must have been extracted earlier, skip it now */
	ent = ent->next;
	fprintf(pck->fp_log, "%s\n", pck->entry_aliases->path_aliased_utf8);

	while (ent) {
		rc = extract_entry(pck, ent);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "extract_entry() failed: %d\n", rc);
			goto out;
		}

		fprintf(pck->fp_log, "%s\n", ent->path_aliased_utf8);
		ent = ent->next;
	}

	/* newline for prettiness */
	fprintf(pck->fp_log, "\n");
	/* override the header to be greater than modification date of any extracted file */
	fseek(pck->fp_log, log_hdr_pos, SEEK_SET);
	cur_time = get_cur_time(tmp, sizeof(tmp));
	/* for some reason the modification date of some files can be still greater, so add
	 * an arbitrary 2 seconds here */
	cur_time += 20 * 1000 * 1000;
	fprintf(pck->fp_log, ":EXTRACT:%020"PRIu64":%28s\n", cur_time, tmp);
	fclose(pck->fp_log);

	fprintf(stderr, "\nDone!\n");
	rc = 0;
out:
	SetCurrentDirectory("..");
	return rc;
}

int
pw_pck_update(struct pw_pck *pck)
{
	char tmp[296];
	uint64_t cur_time;
	size_t log_hdr_pos;
	int rc;

	fprintf(stderr, "Updating:\n\n");

	snprintf(tmp, sizeof(tmp), "%s.files", pck->fullname);
	rc = access(tmp, F_OK);
	if (rc != 0) {
		PWLOG(LOG_ERROR, "Can't find \"%s\"\n", tmp);
		return -rc;
	}
	SetCurrentDirectory(tmp);

	rc = read_aliases(pck, NULL);
	if (rc) {
		PWLOG(LOG_ERROR, "Can't read the alias file (rc=%d). Please extract the .pck file again using mgpck\n", rc);
		goto out;
	}

	rc = read_pck_entry_list(pck, false);
	rc = rc || read_log(pck);
	if (rc) {
		goto out;
	}

	log_hdr_pos = ftell(pck->fp_log);
	cur_time = get_cur_time(tmp, sizeof(tmp));
	fprintf(pck->fp_log, ":UPDATE:%020"PRIu64":%28s\n", cur_time, tmp);

	rc = find_modified_files(pck, true);
	if (rc) {
		return rc;
	}

	if (!pck->needs_update) {
		fprintf(stderr, "\t<everything up to date>\n");
		/* cut the UPDATE hdr */
		fp_truncate(pck->fp_log, log_hdr_pos);
		rc = 0;
		goto out;
	}

	try_read_free_blocks(pck);

	rc = write_modified_pck_files(pck);
	rc = rc || write_free_blocks(pck);
	rc = rc || rewrite_entry_hdr_list(pck);
	rc = rc || write_ftr(pck);
	if (rc) {
		goto out;
	}

	/* newline in the log for prettiness */
	fprintf(pck->fp_log, "\n");

	fprintf(stderr, "\nDone!\n");
	rc = 0;
out:
	SetCurrentDirectory("..");
	return rc;
}

int
pw_pck_gen_patch(struct pw_pck *pck, const char *patch_path, bool do_force)
{
	struct pw_pck_entry *patch_files = NULL;
	FILE *fp_patch;
	char tmp[296];
	int rc;

	snprintf(tmp, sizeof(tmp), "%s.files", pck->fullname);
	rc = access(tmp, F_OK);
	if (rc != 0) {
		PWLOG(LOG_ERROR, "Can't find \"%s\"\n", tmp);
		return -rc;
	}

	if (access(patch_path, F_OK) == 0) {
		print_colored_utf8(COLOR_RED, "Error: ");
		fprintf(stderr, "%s already exists\n", patch_path);
		return -EEXIST;
	}
	fp_patch = fopen(patch_path, "wb");
	if (!fp_patch) {
		PWLOG(LOG_ERROR, "fopen() failed: %d\n", errno);
		return -errno;
	}

	SetCurrentDirectory(tmp);

	rc = read_aliases(pck, NULL);
	if (rc) {
		PWLOG(LOG_ERROR, "Can't read the alias file (rc=%d). Please extract the .pck file again using mgpck\n", rc);
		goto out;
	}

	rc = read_pck_entry_list(pck, false);
	rc = rc || read_log(pck);
	if (rc) {
		goto out;
	}

	fprintf(stderr, "Searching for updated files in %s.files ...\n\n", pck->fullname);
	rc = find_modified_files(pck, false);
	if (rc) {
		goto out;
	}

	if (pck->needs_update) {
		if (!do_force) {
			fprintf(stderr, "\nThe above files were updated in the .files directory and won't be included in the patch. Please update the pck first to include them, or run --gen-patch with the --force flag to ignore this check. Aborting...\n");
			rc = -EBUSY;
			goto out;
		} else {
			fprintf(stderr, "\nThe above files were updated in the .files directory and won't be included in the patch. Update the pck to include them. Continuing...\n");
		}
	} else {
		fprintf(stderr, "\t<everything up to date>\n");
	}

	fprintf(stderr, "\nGenerating the patch file ...\n\n");

	rc = get_updated_files_by_log(pck, &patch_files);
	if (rc) {
		return rc;
	}

	if (!patch_files) {
		fprintf(stderr, "\t<no files changed>\n");
		fprintf(stderr, "\nDone!\n");
		rc = 0;
		goto out;
	}

	uint64_t cur_time = get_cur_time(tmp, sizeof(tmp));
	fprintf(pck->fp_log, ":GEN_PATCH:%020"PRIu64":%28s\n", cur_time, tmp);

	struct pw_pck_header hdr = {};
	struct pw_pck_footer ftr = {};
	bool aliases_found = false;
	/* dummy header -> we'll get back here */
	fwrite(&hdr, sizeof(hdr), 1, fp_patch);

	struct pw_pck_entry **ep = &patch_files;
	while (*ep) {
		print_colored_utf8(COLOR_YELLOW, "\t+%s\n", (*ep)->path_aliased_utf8);

		/* move data from pck to the patch file, also modify the org entry to our needs */
		if ((*ep)->mod_time) {
			fseek(pck->fp, (*ep)->hdr.offset, SEEK_SET);
			(*ep)->hdr.offset = ftell(fp_patch);
			pipe(fp_patch, pck->fp, (*ep)->hdr.compressed_length);
			fprintf(pck->fp_log, "%s\n", (*ep)->path_aliased_utf8);
		} else {
			(*ep)->hdr.to_be_removed = 1;
			fprintf(pck->fp_log, "?%s\n", (*ep)->path_aliased_utf8);
		}

		ftr.entry_cnt++;

		if (strstr((*ep)->hdr.path, "_aliases.cfg") != NULL) {
			aliases_found = true;

			if (ep != &patch_files) {
				struct pw_pck_entry *aliases = *ep;

				/* remove aliases from the list, then put them at the very beginning */
				*ep = aliases->gen_next;
				aliases->gen_next = patch_files;
				patch_files = aliases;
				continue;
			}
		}

		ep = &(*ep)->gen_next;
	}

	ftr.entry_list_off = ftell(fp_patch) ^ PW_PCK_XOR1;

	/* prepend an empty free blocks entry */
	struct pw_pck_entry free_block_ent = {};
	snprintf(free_block_ent.hdr.path, sizeof(free_block_ent.hdr.path),
			"%s_fragm.dat", pck->name);
	write_entry_hdr(pck, fp_patch, &free_block_ent);
	ftr.entry_cnt++;

	if (!aliases_found) {
		/* prepend an empty aliases entry */
		struct pw_pck_entry aliases_ent = {};
		snprintf(aliases_ent.hdr.path, sizeof(aliases_ent.hdr.path),
				"%s_aliases.cfg", pck->name);
		write_entry_hdr(pck, fp_patch, &aliases_ent);
		ftr.entry_cnt++;
	}

	struct pw_pck_entry *p = patch_files;
	while (p) {
		write_entry_hdr(pck, fp_patch, p);
		p = p->gen_next;
	}

	ftr.magic0 = PCK_FOOTER_MAGIC0;
	snprintf(ftr.description, sizeof(ftr.description), "pwmirage :1 :Angelica Patch File");
	ftr.ver = 0x30000;
	ftr.magic1 = PCK_FOOTER_MAGIC1;

	fwrite(&ftr, sizeof(ftr), 1, fp_patch);

	hdr.magic0 = PCK_HEADER_MAGIC0;
	hdr.file_size = ftell(fp_patch);
	hdr.magic1 = PCK_HEADER_MAGIC1;

	fseek(fp_patch, 0, SEEK_SET);
	fwrite(&hdr, sizeof(hdr), 1, fp_patch);

	/* newline in the log for prettiness */
	fprintf(pck->fp_log, "\n");

	fprintf(stderr, "\nDone!\n");
	rc = 0;
out:
	SetCurrentDirectory("..");
	fclose(fp_patch);
	if (rc || !patch_files) {
		remove(patch_path);
	}
	return rc;
}

int
pw_pck_apply_patch(struct pw_pck *pck, const char *patch_path)
{
	FILE *fp_patch;
	char tmp[296];
	uint64_t cur_time;
	uint32_t fsize;
	uint32_t patch_version;
	int rc;

	fp_patch = fopen(patch_path, "rb");
	if (!fp_patch) {
		print_colored_utf8(COLOR_RED, "Error: ");
		fprintf(stderr, "Can't open the patch file \"%s\"\n", patch_path);
		return -errno;
	}

	fprintf(stderr, "Applying patch ...\n\n");

	if (!pck->path_node_tree) {
		rc = read_pck_entry_list(pck, false);
		if (rc) {
			goto out;
		}

		try_read_free_blocks(pck);

		snprintf(tmp, sizeof(tmp), "%s.files", pck->fullname);
		SetCurrentDirectory(tmp);
		/* it might fail because .pck.files is not present -> don't care */
		read_log(pck);
		SetCurrentDirectory("..");
	}

	if (pck->fp_log) {
		cur_time = get_cur_time(tmp, sizeof(tmp));
		fprintf(pck->fp_log, ":APPLY_PATCH:%020"PRIu64":%28s\n", cur_time, tmp);
	}

	struct pw_pck_header hdr = {};
	struct pw_pck_footer ftr = {};

	/* read the file and check magic numbers at the very beginning */
	fread(&hdr, sizeof(hdr), 1, fp_patch);
	if (hdr.magic0 != PCK_HEADER_MAGIC0 || hdr.magic1 != PCK_HEADER_MAGIC1) {
		PWLOG(LOG_ERROR, "not a patch file\n");
		rc = -EINVAL;
		goto out;
	}

	fsize = hdr.file_size;

	if (fsize < sizeof(hdr) + sizeof(ftr)) {
		PWLOG(LOG_ERROR, "patch file way too small\n");
		rc = -EINVAL;
		goto out;
	}

	/* now check similar magic numbers at the end of file */
	fseek(fp_patch, fsize - sizeof(ftr), SEEK_SET);
	fread(&ftr, sizeof(ftr), 1, fp_patch);

	PWLOG(LOG_DEBUG_1, "fsize: 0x%x\n", fsize);
	PWLOG(LOG_DEBUG_1, "ver: 0x%x\n", ftr.ver);
	PWLOG(LOG_DEBUG_1, "entries: 0x%x\n", ftr.entry_cnt);
	PWLOG(LOG_DEBUG_1, "flags: 0x%x\n", ftr.flags);
	PWLOG(LOG_DEBUG_1, "desc: \"%s\"\n", ftr.description);

	if (ftr.magic0 != PCK_FOOTER_MAGIC0 || ftr.magic1 != PCK_FOOTER_MAGIC1) {
		PWLOG(LOG_ERROR, "invalid footer magic: m0=0x%x, m1=0x%x\n", ftr.magic0, ftr.magic1);
		rc = -EINVAL;
		goto out;
	}

	if (ftr.ver != 0x30000) {
		PWLOG(LOG_ERROR, "invalid patch file version: 0x%x\n", ftr.ver);
		rc = -EINVAL;
		goto out;
	}

	rc = sscanf(ftr.description, " pwmirage : %u : %[^:]", &patch_version, tmp);
	if (rc != 2) {
		patch_version = 0;
	}

	struct pw_pck_entry *entry_arr = calloc(1, sizeof(*entry_arr) * ftr.entry_cnt);
	if (!entry_arr) {
		PWLOG(LOG_ERROR, "calloc(%d) failed\n", sizeof(*entry_arr) * ftr.entry_cnt);
		rc = -ENOMEM;
		goto out;
	}

	ftr.entry_list_off ^= PW_PCK_XOR1;
	fseek(fp_patch, ftr.entry_list_off, SEEK_SET);

	for (int i = 0; i < ftr.entry_cnt; i++) {
		struct pw_pck_entry *ent = &entry_arr[i];

		rc = read_entry_hdr(pck, fp_patch, ent);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "read_entry_hdr() failed: %d\n", rc);
			goto out;
		}
	}

	for (int i = 0; i < ftr.entry_cnt; i++) {
		struct pw_pck_entry *ent = &entry_arr[i];
		struct pw_pck_entry *pck_ent;
		uint32_t ent_off = ent->hdr.offset;
		uint32_t ent_size = MIN(ent->hdr.compressed_length, ent->hdr.length);

		pck_ent = set_path_nodes(pck, ent, false);
		if (!pck_ent) {
			rc = -EFAULT;
			goto out;
		}

		if (pck_ent != ent) {
			/* the entry is present in the pck, so patch it */
			add_free_block(pck, pck_ent->hdr.offset,
					MIN(pck_ent->hdr.compressed_length, pck_ent->hdr.length));
			print_colored_utf8(COLOR_YELLOW, "\t+%s\n", pck_ent->path_aliased_utf8);
			if (pck->fp_log) {
				fprintf(pck->fp_log, "%s\n", pck_ent->path_aliased_utf8);
			}
		} else if (!pck_ent->hdr.to_be_removed) {
			/* the entry is new, so just add it to pck->entries */
			append_entry(pck, pck_ent);
			print_colored_utf8(COLOR_GREEN, "\t+%s\n", pck_ent->path_aliased_utf8);
			if (pck->fp_log) {
				fprintf(pck->fp_log, "%s\n", pck_ent->path_aliased_utf8);
			}
		} else {
			print_colored_utf8(COLOR_RED, "\t-%s\n", pck_ent->path_aliased_utf8);
			if (pck->fp_log) {
				fprintf(pck->fp_log, "?%s\n", pck_ent->path_aliased_utf8);
			}
		}

		pck_ent->hdr.length = ent->hdr.length;
		pck_ent->hdr.compressed_length = ent->hdr.compressed_length;
		pck_ent->hdr.to_be_removed = ent->hdr.to_be_removed;
		if (!pck_ent->hdr.to_be_removed) {
			pck_ent->hdr.offset = get_free_block(pck, ent_size);
			fseek(fp_patch, ent_off, SEEK_SET);
			fseek(pck->fp, pck_ent->hdr.offset, SEEK_SET);
			pipe(pck->fp, fp_patch, ent_size);
		}
	}

	rc = write_free_blocks(pck);
	rc = rc || rewrite_entry_hdr_list(pck);
	rc = rc || write_ftr(pck);
	if (rc) {
		goto out;
	}

	if (pck->fp_log) {
		/* newline in the log for prettiness */
		fprintf(pck->fp_log, "\n");
	}

	fprintf(stderr, "\nDone!\n");
	rc = 0;
out:
	fclose(fp_patch);
	return rc;
}
