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

/* windows specific */
#include <tchar.h>
#include <windows.h>
#include <winbase.h>

#include "pw_pck.h"
#include "avl.h"

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
rmrf(char *path)
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

struct pck_alias_tree {
	struct pck_alias_tree *parent;
	struct pw_avl *avl;
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

	root_avl = cur_avl = alloc_alias_tree();
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

static int
read_entry_hdr(struct pw_pck *pck, struct pw_pck_entry_header *hdr)
{
	FILE *fp = pck->fp;
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

/* each file or directory inside an entry's path */
struct pck_file {
	char *name; /* this particular file/dir basename */
	struct pw_pck_entry *entry; /**< set for files, NULL for directories */
	struct pw_avl *nested; /**< for directories only */
};

static struct pck_file *
get_file(struct pw_avl *files, char *name)
{
	struct pck_file *file;
	unsigned hash;

	if (!files) {
		return NULL;
	}

	hash = djb2(name);
	file = pw_avl_get(files, hash);
	while (file && strcmp(file->name, name) != 0) {
		file = pw_avl_get_next(files, file);
	}

	return file;
}

static struct pck_file *
set_file(struct pw_avl *files, char *name, struct pw_pck_entry *ent)
{
	struct pck_file *file;
	unsigned hash;

	if (!files) {
		return NULL;
	}

	hash = djb2(name);
	file = pw_avl_get(files, hash);
	while (file && strcmp(file->name, name) != 0) {
		file = pw_avl_get_next(files, file);
	}

	if (file) {
		return file;
	}

	file = pw_avl_alloc(files);
	if (!file) {
		PWLOG(LOG_ERROR, "pw_avl_alloc() failed\n");
		return NULL;
	}

	file->name = name;
	if (ent) {
		file->entry = ent;
	} else {
		file->nested = pw_avl_init(sizeof(struct pck_file));
	}

	pw_avl_insert(files, hash, file);
	return file;
}


/** convert the path to utf8, find an alias for it, fill in the entry avl */
static int
process_entry_path(struct pw_pck *pck, struct pw_pck_entry *ent, enum pw_pck_action action)
{
	char utf8_path[396];
	char *c, *word;
	struct pck_alias_tree *aliases = pck->alias_tree;
	struct pw_avl *files = pck->entries_tree;
	struct pck_file *file;
	struct pck_alias *alias;
	size_t aliased_len = 0;

	change_charset("GB2312", "UTF-8", ent->hdr.path, sizeof(ent->hdr.path), utf8_path, sizeof(utf8_path));

	c = word = utf8_path;
	while (1) {
		if (*c == '\\' || *c == '/') {
			*c = 0;
			alias = get_alias(aliases, word);
			/* a dir */

			aliased_len += snprintf(ent->path_aliased_utf8 + aliased_len, sizeof(ent->path_aliased_utf8) - aliased_len, "%s/", alias ? alias->new_name : word);

			if (alias) {
				aliases = alias->sub_aliases;
			}

			if (action == PW_PCK_ACTION_EXTRACT) {
				mkdir(ent->path_aliased_utf8, 0755);
			} else {
				if (alias) {
					file = set_file(files, alias->new_name, NULL);
				} else {
					char *word_d = strdup(word); /* memleak, don't care */
					if (!word_d) {
						PWLOG(LOG_ERROR, "strdup() failed\n");
						return -ENOMEM;
					}

					file = set_file(files, word_d, NULL);
				}

				if (!file) {
					PWLOG(LOG_ERROR, "set_file() failed\n");
					return -1;
				}

				files = file->nested;
			}

			word = c + 1;
		} else if (*c == 0) {
			alias = get_alias(aliases, word);
			/* a file (because there can't be empty dirs in the pck) */

			aliased_len += snprintf(ent->path_aliased_utf8 + aliased_len, sizeof(ent->path_aliased_utf8) - aliased_len, "%s", alias ? alias->new_name : word);

			if (action != PW_PCK_ACTION_EXTRACT) {
				if (alias) {
					file = set_file(files, alias->new_name, ent);
				} else {
					char *word_d = strdup(word); /* memleak, don't care */
					if (!word_d) {
						PWLOG(LOG_ERROR, "strdup() failed\n");
						return -ENOMEM;
					}

					file = set_file(files, word_d, ent);
				}

				if (!file) {
					PWLOG(LOG_ERROR, "set_file() failed\n");
					return -1;
				}
			}

			break;
		}

		c++;
	}

	return 0;
}

static int extract_entry(struct pw_pck *pck, struct pw_pck_entry *ent);

static int
read_pck(struct pw_pck *pck, enum pw_pck_action action)
{
	FILE *fp = pck->fp;
	char tmp[296];
	char *alias_buf = NULL;
	size_t alias_buflen = 0;
	size_t fsize;
	int rc;

	/* read the file and check magic numbers at the very beginning */

	fread(&pck->hdr, sizeof(pck->hdr), 1, fp);
	if (pck->hdr.magic0 != PCK_HEADER_MAGIC0 || pck->hdr.magic1 != PCK_HEADER_MAGIC1) {
		PWLOG(LOG_ERROR, "not a pck file\n");
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

	rc = sscanf(pck->ftr.description, " pwmirage : %u : %[^:]", &pck->mg_version, tmp);
	if (rc != 2) {
		pck->mg_version = 0;
	}

	pck->ftr.entry_list_off ^= PW_PCK_XOR1;

	pck->entries = calloc(1, sizeof(*pck->entries) * pck->entry_cnt);
	if (!pck->entries) {
		PWLOG(LOG_ERROR, "calloc() failed\n");
		goto err_close;
	}

	fseek(pck->fp, pck->ftr.entry_list_off, SEEK_SET);

	/* Also extract the aliases -> we'll need them early */
	if (pck->mg_version > 0 && action == PW_PCK_ACTION_EXTRACT) {
		struct pw_pck_entry *ent = &pck->entries[PW_PCK_ENTRY_ALIASES];

		snprintf(tmp, sizeof(tmp), "%s_aliases.cfg", pck->name);
		if (strcmp(ent->hdr.path, tmp) != 0) {
			PWLOG(LOG_ERROR, "Alias filename mismatch, got=\"%s\", expected=\"%s\"\n",
					ent->hdr.path, tmp);
			rc = -EIO;
			goto err_cleanup;
		}

		rc = extract_entry(pck, ent);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "extract_entry(%d) failed: %d\n", 0, rc);
			goto err_cleanup;
		}

		readfile(ent->path_aliased_utf8, &alias_buf, &alias_buflen);
		read_aliases(pck, alias_buf);
	}

	if (pck->mg_version == 0) {
		/* setup a few defaults */
		pck->entry_max_cnt = pck->entry_cnt;
	}

	/* for anything but extract we'll need a file db to compare to */
	if (action != PW_PCK_ACTION_EXTRACT) {
		pck->entries_tree = pw_avl_init(sizeof(struct pw_pck_entry));
		if (!pck->entries_tree) {
			PWLOG(LOG_ERROR, "pw_avl_init() failed\n");
			return -1;
		}
	}

	fseek(pck->fp, pck->ftr.entry_list_off, SEEK_SET);
	for (int i = PW_PCK_ENTRY_ALIASES; i < pck->entry_cnt; i++) {
		struct pw_pck_entry *ent = &pck->entries[i];

		rc = read_entry_hdr(pck, &ent->hdr);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "read_entry_hdr() failed: %d\n", rc);
			goto err_cleanup;
		}

		process_entry_path(pck, ent, action);
	}

	return 0;
err_cleanup:
err_close:
	fclose(fp);
	return -1;
}

static int
extract_entry(struct pw_pck *pck, struct pw_pck_entry *ent)
{
	char buf[16384];
	size_t remaining_bytes = ent->hdr.length;
	size_t read_bytes, buf_size;
	FILE *fp = pck->fp;
	int rc;

	fseek(fp, ent->hdr.offset, SEEK_SET);

	FILE *fp_out = wfopen(ent->path_aliased_utf8, "wb");
	if (fp_out == NULL) {
		PWLOG(LOG_ERROR, "can't open %s\n", ent->path_aliased_utf8);
		return -1;
	}

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
			fclose(fp_out);
			return -rc;
		}
	}

	fclose(fp_out);

	return 0;
}

static uint64_t
get_cur_time(char *buf, size_t buflen)
{
	SYSTEMTIME st;
	FILETIME ft;
	size_t date_len;

	GetLocalTime(&st);
	SystemTimeToFileTime(&st, &ft);

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
extract_pck(struct pw_pck *pck)
{
	char tmp[296];
	int rc;
	unsigned entry_idx = 0;

	fseek(pck->fp, pck->ftr.entry_list_off, SEEK_SET);

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

		fprintf(pck->fp_log, "%s\n", tmp);
	} else {
		/* the first entry is always the list of free blocks, don't extract
		 * it -> it should live inside the pck only */
		entry_idx++;

		/* the second entry is the log and it must have been extracted earlier, skip it now */
		entry_idx++;
	}

	for (; entry_idx < pck->entry_cnt; entry_idx++) {
		struct pw_pck_entry *ent = &pck->entries[entry_idx];

		rc = extract_entry(pck, ent);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "extract_entry(%d) failed: %d\n", entry_idx, rc);
			goto out;
		}

		fprintf(pck->fp_log, "%s\n", ent->path_aliased_utf8);
	}

	/* newline for prettiness */
	fprintf(pck->fp_log, "\n");
	/* override the header to be greater than modification date of any extracted file */
	fseek(pck->fp_log, log_hdr_pos, SEEK_SET);
	cur_time = get_cur_time(tmp, sizeof(tmp));
	fprintf(pck->fp_log, ":EXTRACT:%020"PRIu64":%28s\n", cur_time, tmp);
	rc = 0;
out:
	fclose(pck->fp_log);
	return rc;
}

static int
read_log(struct pw_pck *pck, bool ignore_updates)
{
	char tmp[128];
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int rc;
	uint64_t cur_modtime = 0;
	bool skip_cur_section = false;
	unsigned lineno = 0;
	struct pw_avl *files;
	struct pck_file *file;

	snprintf(tmp, sizeof(tmp), "%s_mgpck.log", pck->name);
	fp = pck->fp_log = fopen(tmp, "r+");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "Cant open the log file: %d (\"%s\")\n", errno, tmp);
		return -errno;
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		bool is_empty = true;
		char *c, *word;

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
			rc = sscanf(line, " : %[^:] : %"PRIu64" : %*s", tmp, &cur_modtime);
			if (rc != 2) {
				PWLOG(LOG_ERROR, "Incomplete header at line %u\n\t\"%s\"\n", lineno, line);
				return -EIO;
			}
			lineno++;
			continue;
		}

		/* filename */
		if (skip_cur_section) {
			lineno++;
			continue;
		}

		files = pck->entries_tree;
		c = word = line;
		while (1) {
			if (*c == '\\' || *c == '/') {
				*c = 0;
				file = get_file(files, word);
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

				file = get_file(files, word);
				if (!file) {
					/* same as above */
					break;
				}

				if (!file->entry) {
					/* we got a log entry for a directory? weird, ignore it */
					break;
				}

				file->entry->mod_time = cur_modtime;
				break;
			}

			c++;
		}

		lineno++;
	}

	return 0;
}

#ifdef __MINGW32__
#include <tchar.h>
#include <windows.h>
#include <winbase.h>
#include <errno.h>

#define FindExInfoBasic 1

static int
find_modified_files(struct pw_pck *pck, wchar_t *path, struct pw_avl *files, struct pw_pck_entry **list)
{
	struct pck_file *file;
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

			file = get_file(files, utf8_name);
			find_modified_files(pck, buf, file ? file->nested : NULL, list);
		} else {
			struct pw_pck_entry *entry;

			if (strstr(utf8_name, "_mgpck.log") != NULL) {
				continue;
			}

			file = get_file(files, utf8_name);
			if (file) {
				entry = file->entry;
				if (write_time <= entry->mod_time && fsize == entry->hdr.length) {
					/* nothing new */
					continue;
				}
			} else {
				/* a brand new file */
				entry = calloc(1, sizeof(*entry));
				if (!entry) {
					PWLOG(LOG_ERROR, "calloc() failed\n");
					return -ENOMEM;
				}

				if (path[0] == '.' && path[1] == '\\') {
					snprintf(entry->path_aliased_utf8, sizeof(entry->path_aliased_utf8),
							"%.*S\\%s", wcslen(path) - 2, path, utf8_name);
				} else {
					snprintf(entry->path_aliased_utf8, sizeof(entry->path_aliased_utf8),
							"%s", utf8_name);
				}
			}

			entry->next = *list;
			entry->is_present = true;
			*list = entry;

			//fprintf(stderr, "file=%s, wtime=%"PRIu64" last_modtime=%"PRIu64"\r\n", entry->path_aliased_utf8, write_time, entry->mod_time);
		}
	} while (FindNextFileW(handle, &fdata) != 0);

	rc = GetLastError();
	FindClose(handle);

	if (rc != ERROR_NO_MORE_FILES) {
		return -rc;
	}

	return 0;
}

#else

static int
find_modified_files(struct pw_pck *pck, wchar_t *path, struct pw_avl *files, struct pw_pck_entry **list)
{
	return -ENOSYS;
}

#endif

/** unused space inside the pck */
struct pck_free_block {
	uint32_t offset;
	uint32_t size;
};

struct pck_free_blocks_meta {
	uint32_t block_cnt;
	uint32_t max_entry_cnt;
};

static int
read_free_blocks(struct pw_pck *pck)
{
	struct pw_avl *avl;
	struct pw_pck_entry *entry;
	struct pck_free_blocks_meta meta;
	struct pck_free_block *blocks;

	assert(pck->mg_version > 0);

	avl = pck->free_blocks_tree = pw_avl_init(sizeof(struct pck_free_block));
	if (!avl) {
		PWLOG(LOG_ERROR, "pw_avl_init() failed\n");
		return -1;
	}

	entry = &pck->entries[PW_PCK_ENTRY_FREE_BLOCKS];

	fseek(pck->fp, entry->hdr.offset, SEEK_SET);
	fread(&meta, sizeof(meta), 1, pck->fp);

	pck->entry_max_cnt = meta.max_entry_cnt;

	assert(meta.block_cnt > 0);
	for (int i = 0; i < meta.block_cnt; i++) {
		struct pck_free_block *block = pw_avl_alloc(avl);

		fread(block, sizeof(*block), 1, pck->fp);
		pw_avl_insert(avl, block->size, block);
	}

	return 0;
}

static void
write_free_block(FILE *fp, struct pw_avl_node *node)
{
	struct pck_free_block *block;

	if (!node) {
		return;
	}

	block = (void *)node->data;
	fwrite(block, sizeof(*block), 1, fp);

	write_free_block(fp, node->next);
	write_free_block(fp, node->left);
	write_free_block(fp, node->right);
}

static int
write_free_blocks(struct pw_pck *pck, struct pw_pck_entry_header *ent_hdr)
{
	struct pck_free_blocks_meta meta;
	struct pck_free_block *blocks;
	size_t off = ftell(pck->fp);
	size_t off_end;

	snprintf(entry_hdr->path, sizeof(entry_hdr->path), "%s_fragm.dat", pck->name);
	entry_hdr->offset = off;

	meta.block_cnt = 0; /* dummy, will be overritten later */
	meta.max_entry_cnt = pck->entry_max_cnt;

	fwrite(&meta, sizeof(meta), 1, pck->fp);

	pck->entry_max_cnt = meta.max_entry_cnt;

	write_free_block(pck->fp, pck->free_blocks_tree->root);
	off_end = ftell(pck->fp);

	/* write the header again */
	fseek(pck->fp, off, SEEK_SET);
	meta.block_cnt = (off_end - off - sizeof(meta)) / sizeof(struct pck_free_block);
	fwrite(&meta, sizeof(meta), 1, pck->fp);

	fseek(pck->fp, off_end, SEEK_SET);

	return 0;
}

int
pw_pck_open(struct pw_pck *pck, const char *path, enum pw_pck_action action)
{
	int rc;
	const char *c;
	char *alias_buf;
	size_t alias_buflen;
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

	if (c != path) {
		c++;
	}
	snprintf(pck->name, sizeof(pck->name), "%.*s", strlen(c) - strlen(".pck"), c);

	pck->fp = fopen(path, "rb");
	if (pck->fp == NULL) {
		PWLOG(LOG_ERROR, "fopen(\"%s\") failed: %d\n", path, errno);
		return -errno;
	}

	rc = GetCurrentDirectory(sizeof(tmp), tmp);
	snprintf(tmp + rc, sizeof(tmp) - rc, "\\%s.pck.files%c", pck->name, 0);
	/* ^ rmrf needs this to be double NULL-terminated */

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
			PWLOG(LOG_ERROR, "Can't find \"%s\"\n", tmp);
			return -rc;
		}
	}

	SetCurrentDirectory(tmp);

	rc = 0;
	if (action == PW_PCK_ACTION_EXTRACT) {
		rc = read_pck(pck, action);
		rc = rc || extract_pck(pck);
		return rc;
	}

	struct pw_pck_entry *modified = NULL;

	if (action == PW_PCK_ACTION_UPDATE) {
		snprintf(tmp, sizeof(tmp), "%s_aliases.cfg", pck->name);
		readfile(tmp, &alias_buf, &alias_buflen);

		rc = read_aliases(pck, alias_buf);
		rc = rc || read_pck(pck, action);
		rc = rc || read_log(pck, false);
		rc = rc || find_modified_files(pck, L".\\*", pck->entries_tree, &modified);

		if (modified == NULL) {
			return 0;
		}

		if (pck->mg_version == 0) {
			/* TODO: repack everything and return */
			return 0;
		}

		read_free_blocks(pck);

		/* find entries with is_present == false
		 *  -> add them to the free blocks list
		 * iterate through the modified/newly added entries
		 *  -> compress to a file with prefixed "dot", get size
		 *  -> try to find a smallest fitting free block first, if ok -> write into the pck->fp, remove the dot file, remove from the list, also update the free-block list
		 *  if the list empty -> return
		 *  fseek into the footer pos
		 *  iterate throught the rest of modified/added entries:
		 *    * pipe the dot file into fp, append an entry
		 *  if entry overflow flag set, rewrite the entry table entirely
		 */
	}

	return rc;
}
