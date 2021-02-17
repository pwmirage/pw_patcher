/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include "common.h"
#include "cjson.h"

#include <iconv.h>

int g_pwlog_level = 99;
FILE *g_nullfile;
const char g_zeroes[4096];

int
pw_chain_table_init(struct pw_chain_table *table, const char *name, struct serializer *serializer, size_t el_size, size_t count)
{
	table->name = name;
	table->serializer = serializer;
	table->el_size = el_size;

	table->chain = table->chain_last = calloc(1, sizeof(*table->chain) + el_size * count);
	if (!table->chain) {
		return -ENOMEM;
	}

	table->chain->capacity = count;
	return 0;
}

struct pw_chain_table *
pw_chain_table_alloc(const char *name, struct serializer *serializer, size_t el_size, size_t count)
{
	struct pw_chain_table *table;
	int rc;

	table = calloc(1, sizeof(*table));
	if (!table) {
		return NULL;
	}

	rc = pw_chain_table_init(table, name, serializer, el_size, count);
	if (rc) {
		free(table);
		return NULL;
	}

	return table;
}

void *
pw_chain_table_new_el(struct pw_chain_table *table)
{
	struct pw_chain_el *chain = table->chain_last;

	if (chain->count < chain->capacity) {
		return &chain->data[chain->count++ * table->el_size];
	}

	size_t table_count = 16;
	chain->next = table->chain_last = calloc(1, sizeof(struct pw_chain_el) + table_count * table->el_size);
	if (!chain->next) {
		return NULL;
	}

	chain = table->chain_last;
	chain->capacity = table_count;
	chain->count = 1;

	return chain->data;
}

struct pw_chain_table *
pw_chain_table_fread(FILE *fp, const char *name, size_t el_count, struct serializer *el_serializer)
{
	struct pw_chain_table *tbl;
	uint32_t el_size = serializer_get_size(el_serializer);
	size_t i;

	uint32_t el_cap = MAX(8, el_count);
	tbl = pw_chain_table_alloc(name, el_serializer, el_size, el_cap);
	if (!tbl) {
		PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed\n");
		return NULL;
	}

	for (i = 0; i < el_count; i++) {
		void *el = pw_chain_table_new_el(tbl);
		fread(el, el_size, 1, fp);
	}

	return tbl;
}

#ifdef __MINGW32__
#include <wininet.h>

static int
download_wininet(const char *url, const char *filename)
{
	HINTERNET hInternetSession;
	HINTERNET hURL;
	BOOL success;
	DWORD num_bytes = 1;
	FILE *fp;

	if (strstr(url, "version.json") != NULL) {
		DeleteUrlCacheEntry(url);
	}

	fp = fopen(filename, "wb");
	if (!fp) {
		return -errno;
	}

	hInternetSession = InternetOpen("Mirage Patcher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!hInternetSession) {
		fclose(fp);
		return -1;
	}

	hURL = InternetOpenUrl(hInternetSession, url,	NULL, 0, 0, 0);
	if (!hURL) {
		InternetCloseHandle(hInternetSession);
		fclose(fp);
		return -1;
	}

	char buf[1024];

	while (num_bytes > 0) {
		success = InternetReadFile(hURL, buf, (DWORD)sizeof(buf), &num_bytes);
		if (!success) {
			break;
		}
		fwrite(buf, 1, (size_t)num_bytes, fp);
	}

	// Close down connections.
	InternetCloseHandle(hURL);
	InternetCloseHandle(hInternetSession);

	fclose(fp);
	return success ? 0 : -1;
}

#else

static int
download_wget(const char *url, const char *filename)
{
	char buf[2048];
	int rc;

	snprintf(buf, sizeof(buf), "wget \"%s\" -O \"%s\"", url, filename);
	rc = system(buf);
	if (rc != 0) {
		return rc;
	}

	return 0;
}
#endif

int
download(const char *url, const char *filename)
{
	PWLOG(LOG_DEBUG_1, "Fetching \"%s\" ...\n", url);
#ifdef __MINGW32__
	return download_wininet(url, filename);
#else
	return download_wget(url, filename);
#endif
}

int
readfile(const char *path, char **buf, size_t *len)
{
	FILE *fp;

	fp = fopen(path, "rb");
	if (!fp) {
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	*len = ftell(fp);

	fseek(fp, 0, SEEK_SET);
	*buf = malloc(*len + 1);
	if (!*buf) {
		return -1;
	}

	fread(*buf, 1, *len, fp);
	(*buf)[*len] = 0;
	fclose(fp);

	return 0;
}

int
download_mem(const char *url, char **buf, size_t *len)
{
	int rc;
	char *tmp_filename = "patcher/tmp";

	rc = download((char *)url, tmp_filename);
	if (rc) {
		return rc;
	}

	rc = readfile(tmp_filename, buf, len);
	unlink(tmp_filename);
	return rc;
}

void
normalize_json_string(char *str)
{
	char *read_b = str;
	char *write_b = str;

	while (*read_b) { /* write_b is slacking behind */
		char c = *read_b;

		if (*read_b == '\\' && *(read_b + 1) == 'r') {
			/* skip \r entirely */
			read_b++;
			read_b++;
			continue;
		}

		if (*read_b == '\\' && *(read_b + 1) == 'n') {
			/* replace first \\ with newline, skip second \\ */
			c = '\n';
			read_b++;
		}

		if (*read_b == '\\' && *(read_b + 1) == '"') {
			/* skip first \\ */
			read_b++;
			c = *read_b;
		}

		if (*read_b == '\\' && *(read_b + 1) == '\\') {
			/* skip second \ in strings */
			read_b++;
		}

		*write_b = c;
		read_b++;
		write_b++;

	}

	*write_b = 0;
}

int
change_charset(char *src_charset, char *dst_charset, char *src, long srclen, char *dst, long dstlen)
{
	iconv_t cd;
	int rc;

	cd = iconv_open(dst_charset, src_charset);
	if (cd == 0) {
		return -1;
	}

	rc = iconv(cd, &src, (size_t *) &srclen, &dst, (size_t *) &dstlen);
	iconv_close(cd);
	return rc;
}

void
fwsprint(FILE *fp, const uint16_t *buf, int maxlen)
{
	char out[1024] = {};
	char *b = out;

	change_charset("UTF-16LE", "UTF-8", (char *)buf, maxlen * 2, out, sizeof(out));
	while (*b && maxlen--) {
		if (*b == '\\') {
			fputs("\\\\", fp);
		} else if (*b == '"') {
			fputs("\\\"", fp);
		} else if (*b == '\r') {
			/* do nothing */
		} else if (*b == '\n') {
			fputs("\\n", fp);
		} else {
			fputc(*b, fp);
		}
		b++;
	}
}

void
sprint(char *dst, size_t dstsize, const char *src, int srcsize)
{
	change_charset("GB2312", "UTF-8", (char *)src, srcsize, dst, dstsize);
}

void
fsprint(FILE *fp, const char *buf, int maxlen)
{
	char out[1024] = {};
	char *b = out;

	sprint(out, sizeof(out), buf, maxlen);
	while (*b && maxlen--) {
		if (*b == '\\') {
			fputs("\\\\", fp);
		} else {
			fputc(*b, fp);
		}
		b++;
	}
}


void
fwsprintf(FILE *fp, const char *fmt, const uint16_t *buf, int maxlen)
{
	char c;

	while ((c = *fmt++)) {
		if (c == '%' && *fmt == 's') {
			fwsprint(fp, buf, maxlen);
			fmt++;
		} else {
			putc(c, fp);
		}
	}
}

void
wsnprintf(uint16_t *dst, size_t dstsize, const char *src) {
	char c;

	memset(dst, 0, dstsize);
	while (dstsize > 0 && (c = *src++)) {
		if (c == '\n' && dstsize >= 2) {
			*dst++ = '\r';
			*dst++ = '\n';
			dstsize -= 2;
		} else {
			*dst++ = c;
			dstsize--;
		}
	}

	if (dstsize == 0) {
		*(dst - 1) = 0;
	} else {
		*dst = 0;
	}
}

uint32_t
js_strlen(const char *str)
{
	uint32_t len = 0;
	char c;

	while ((c = *str++)) {
		if (c == '\n') {
			len++;
		}
		len++;
	}
	return len;
}

size_t
serialize_chunked_table_fn(FILE *fp, struct serializer *f, void *data)
{
	struct pw_chain_table *table = *(void **)data;
	struct serializer *slzr;
	struct pw_chain_el *chain;

	if (!table) {
		return sizeof(void *);
	}
	slzr = table->serializer;
	chain = table->chain;

	size_t ftell_begin = ftell(fp);
	fprintf(fp, "\"%s\":[", f->name);
	bool obj_printed = false;
	while (chain) {
		size_t i;

		for (i = 0; i < chain->count; i++) {
			void *el = chain->data + i * table->el_size;
			struct serializer *tmp_slzr = slzr;
			size_t pos_begin = ftell(fp);

			_serialize(fp, &tmp_slzr, &el, 1, true, false, true);
			if (ftell(fp) > pos_begin) {
				fprintf(fp, ",");
				obj_printed = true;
			}
		}

		chain = chain->next;
	}

	if (!obj_printed) {
		fseek(fp, ftell_begin, SEEK_SET);
	} else {
		/* override last comma */
		fseek(fp, -1, SEEK_CUR);
		fprintf(fp, "],");
	}

	return sizeof(void *);
}

size_t
deserialize_chunked_table_fn(struct cjson *f, struct serializer *_slzr, void *data)
{
	struct pw_chain_table *table = *(void **)data;
	struct serializer *slzr = _slzr->ctx;

	if (f->type == CJSON_TYPE_NONE) {
		return 8;
	}

	if (f->type != CJSON_TYPE_OBJECT) {
		PWLOG(LOG_ERROR, "found json group field that is not an object (type: %d)\n", f->type);
		return 8;
	}

	if (!table) {
		size_t el_size = serializer_get_size(slzr);
		table = *(void **)data = pw_chain_table_alloc("", slzr, el_size, 8);
		if (!table) {
			PWLOG(LOG_ERROR, "pw_chain_table_alloc() failed\n");
			return 8;
		}
	}

	struct cjson *json_el = f->a;
	while (json_el) {
		char *end;
		unsigned idx = strtod(json_el->key, &end);
		if (end == json_el->key || errno == ERANGE) {
			/* non-numeric key in array */
			json_el = json_el->next;
			assert(false);
			continue;
		}

		void *el = NULL;
		struct pw_chain_el *chain = table->chain;
		unsigned remaining_idx = idx;

		while (chain) {
			if (remaining_idx < chain->count) {
				/* the obj exists */
				el = chain->data + remaining_idx * table->el_size;
				break;
			}

			if (remaining_idx < chain->capacity) {
				remaining_idx -= chain->count;
				/* the obj doesn't exist, but memory for it is already allocated */
				break;
			}
			assert(remaining_idx >= chain->count); /* technically possible if chain->cap == 0 */
			remaining_idx -= chain->count;
			chain = chain->next;
		}

		if (!el) {
			if (remaining_idx > 8) {
				/* sane limit - the hole is too big */
				continue;
			}

			while (true) {
				el = pw_chain_table_new_el(table);
				if (!el) {
					PWLOG(LOG_ERROR, "pw_chain_table_new_el() failed\n");
					return 8;
				}

				if (remaining_idx == 0) {
					break;
				}
				remaining_idx--;
			}
		}

		deserialize(json_el, slzr, &el);
		json_el = json_el->next;
	}

	return sizeof(void *);
}

int
pw_version_load(struct pw_version *ver)
{
	FILE *fp = fopen("patcher/version", "rb");

	if (!fp) {
		memset(ver, 0, sizeof(*ver));
		ver->magic = PW_VERSION_MAGIC;
		return errno;
	}

	fread(ver, 1, sizeof(*ver), fp);
	ver->branch[sizeof(ver->branch) - 1] = 0;
	ver->cur_hash[sizeof(ver->cur_hash) - 1] = 0;
	fclose(fp);

	if (ver->magic != PW_VERSION_MAGIC) {
		memset(ver, 0, sizeof(*ver));
		ver->magic = PW_VERSION_MAGIC;
		return 1;
	}

	return 0;
}

int
pw_version_save(struct pw_version *ver)
{
	FILE *fp = fopen("patcher/version", "wb");

	if (!fp) {
		return -errno;
	}

	fwrite(ver, 1, sizeof(*ver), fp);
	ver->branch[sizeof(ver->branch) - 1] = 0;
	ver->cur_hash[sizeof(ver->cur_hash) - 1] = 0;
	fclose(fp);

	return 0;
}

void
pwlog(int type, const char *filename, unsigned lineno, const char *fnname, const char *fmt, ...)
{
	va_list args;
	const char *type_str;

	if (type > g_pwlog_level) {
		return;
	}

	switch (type) {
		case LOG_ERROR:
			type_str = "ERROR";
			break;
		case LOG_INFO:
			type_str = "INFO";
			break;
		default:
			return;
	}
	
	fprintf(stderr, "%s:%u %s(): %s: ", filename, lineno, fnname, type_str);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fflush(stderr);
}

static void __attribute__((constructor))
common_init()
{
	g_nullfile = fopen("/dev/null", "w");
	if (g_nullfile == NULL) {
		g_nullfile = fopen("NUL", "w");
	}
}
