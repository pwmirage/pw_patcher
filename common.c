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
static FILE *g_nullfile;
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
		*write_b = *read_b;
		read_b++;
		write_b++;

		if (*read_b == '\\' && *(read_b + 1) == '\\') {
			/* skip second \ in strings */
			read_b++;
		}
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

long
_serialize(FILE *fp, struct serializer **slzr_table_p, void **data_p,
		unsigned data_cnt, bool skip_empty_objs, bool newlines, bool force_object)
{
	unsigned data_idx;
	struct serializer *slzr;
	void *data = *data_p;
	bool nonzero, obj_printed;
	long sz, arr_sz;

	if (!force_object) {
		fprintf(fp, "[");
	}
	/* in case the arr is full of empty objects or just 0-fields -> print just [] */
	arr_sz = ftell(fp);
	for (data_idx = 0; data_idx < data_cnt; data_idx++) {
		slzr = *slzr_table_p;
		obj_printed = false;
		if (slzr->name[0] != 0) {
			obj_printed = true;
			fprintf(fp, "{");
		}
		/* when obj contains only 0-fields -> print {} */
		sz = ftell(fp);
		nonzero = false;

		while (true) {
			if (slzr->type == _INT8) {
				if (!obj_printed || (*(uint16_t *)data != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%u,", *(uint8_t *)data);
					nonzero = nonzero || *(uint8_t *)data != 0;
				}
				data += 1;
			} else if (slzr->type == _INT16) {
				if (!obj_printed || (*(uint16_t *)data != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%u,", *(uint16_t *)data);
					nonzero = nonzero || *(uint16_t *)data != 0;
				}
				data += 2;
			} else if (slzr->type == _INT32) {
				if (!obj_printed || (*(uint32_t *)data != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%u,", *(uint32_t *)data);
					nonzero = nonzero || *(uint32_t *)data != 0;
				}
				data += 4;
			} else if (slzr->type > _CONST_INT(0) && slzr->type <= _CONST_INT(0x1000)) {
				unsigned num = slzr->type - _CONST_INT(0);

				if (!obj_printed || (num != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%u,", num);
					nonzero = nonzero || num != 0;
				}
			} else if (slzr->type == _FLOAT) {
				if (!obj_printed || (*(float *)data != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%.8f,", *(float *)data);
					nonzero = nonzero || *(float *)data != 0;
				}
				data += 4;
			} else if (slzr->type > _WSTRING(0) && slzr->type <= _WSTRING(0x1000)) {
				unsigned len = slzr->type - _WSTRING(0);

				if (slzr->name[0] != '_') {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "\"");
					fwsprint(fp, (const uint16_t *)data, len);
					fprintf(fp, "\",");
					nonzero = nonzero || *(uint16_t *)data != 0;
				}
				data += len * 2;
			} else if (slzr->type > _STRING(0) && slzr->type <= _STRING(0x1000)) {
				unsigned len = slzr->type - _STRING(0);

				if (slzr->name[0] != '_') {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "\"");
					fsprint(fp, (const char *)data, len);
					fprintf(fp, "\",");
					nonzero = nonzero || *(char *)data != 0;
				}
				data += len;
			} else if (slzr->type > _ARRAY_START(0) && slzr->type <= _ARRAY_START(0x1000)) {
				unsigned cnt = slzr->type - _ARRAY_START(0);
				size_t pre_name_pos = ftell(fp);
				const char *slzr_name = slzr->name;

				if (slzr->name[0] != 0) {
					fprintf(fp, "\"%s\":", slzr->name);
				}

				size_t pre_pos = ftell(fp);
				slzr++;
				_serialize(fp, &slzr, &data, cnt, true, false, false);

				if (ftell(fp) == pre_pos || slzr_name[0] == '_') {
					fseek(fp, pre_name_pos, SEEK_SET);
				} else {
					fprintf(fp, ",");
					nonzero = true;
				}
			} else if (slzr->type == _OBJECT_START) {
				/* FIXME need to advance slzr for inline objs even if its _prefixed */
				if (slzr->name[0] != '_') {
					size_t pre_name_pos = ftell(fp);

					struct serializer *nested_slzr = slzr->ctx;
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}


					size_t pre_pos = ftell(fp);
					if (nested_slzr) {
						_serialize(fp, &nested_slzr, &data, 1, true, false, true);
					} else {
						slzr++;
						_serialize(fp, &slzr, &data, 1, true, false, true);
					}
					if (ftell(fp) == pre_pos) {
						/* nothing printed in the object, skip its name */
						fseek(fp, pre_name_pos, SEEK_SET);
					} else {
						fprintf(fp, ",");
						nonzero = true;
					}
				}
			} else if (slzr->type == _CUSTOM) {
				size_t pre_pos = ftell(fp);
				data += slzr->fn(fp, slzr, data);
				nonzero = nonzero || ftell(fp) != pre_pos;
			} else if (slzr->type == _ARRAY_END) {
				break;
			} else if (slzr->type == _OBJECT_END) {
				break;
			} else if (slzr->type == _TYPE_END) {
				break;
			}
			slzr++;
		}

		if (skip_empty_objs && !nonzero) {
			if (obj_printed) {
				/* go back to { */
				fseek(fp, sz, SEEK_SET);
			} else {
				/* overwrite previous comma */
				fseek(fp, -1, SEEK_CUR);
			}
		} else {
			/* overwrite previous comma */
			fseek(fp, -1, SEEK_CUR);
			/* save } of the last non-empty object since we may need to strip
			 * all subsequent objects from the array */
			arr_sz = ftell(fp) + (obj_printed ? 1 : 0);
		}

		if (obj_printed) {
			fprintf(fp, "}");
		}
		fprintf(fp, ",");
		if (newlines) {
			fprintf(fp, "\n");
		}
	}

	/* overwrite previous comma and strip empty objects from the array */
	fseek(fp, arr_sz, SEEK_SET);

	if (!force_object) {
		fprintf(fp, "]");
	}

	*slzr_table_p = slzr;
	*data_p = data;
	return arr_sz + !force_object;
}

long
serialize(FILE *fp, struct serializer *slzr_table, void *data, unsigned data_cnt)
{
	return _serialize(fp, &slzr_table, &data, data_cnt, false, true, false);
}

void
deserialize_log(struct cjson *json_f, void *data)
{
	char buf[2048] = {0};
	char buf2[2048];
	struct cjson *json = json_f;

	while (json && json->key) {
		memcpy(buf2, buf, sizeof(buf2));
		snprintf(buf, sizeof(buf), "%s->%s", json->key, buf2);
		json = json->parent;
	}
	buf[strlen(buf) - 2] = 0;

	switch (json_f->type) {
		case CJSON_TYPE_INTEGER:
			PWLOG(LOG_INFO, "patching \"%s\" (prev:%d, new:%d)\n", buf, *(uint32_t *)data, json_f->i);
			break;
		case CJSON_TYPE_FLOAT:
			PWLOG(LOG_INFO, "patching \"%s\" (prev:%.8f, new:%.8f)\n", buf, *(float *)data, json_f->d);
			break;
		case CJSON_TYPE_BOOLEAN:
			PWLOG(LOG_INFO, "patching \"%s\" (prev:%u, new:%u)\n", buf, *(char *)data, json_f->i);
			break;
		case CJSON_TYPE_STRING:
			PWLOG(LOG_INFO, "patching \"%s\" (new:%s)\n", buf, json_f->s);
			break;
	}
}

int
serializer_get_size(struct serializer *slzr_table)
{
	return serializer_get_offset(slzr_table, NULL);
}

int
serializer_get_offset(struct serializer *slzr, const char *name)
{
	int offset = 0;
	int rc;

	while (true) {
		if (name && strcmp(slzr->name, name) == 0) {
			return offset;
		}

		if (slzr->type == _INT8) {
			offset += 1;
		} else if (slzr->type == _INT16) {
			offset += 2;
		} else if (slzr->type == _INT32) {
			offset += 4;
		} else if (slzr->type == _FLOAT) {
			offset += 4;
		} else if (slzr->type > _WSTRING(0) && slzr->type <= _WSTRING(0x1000)) {
			unsigned len = slzr->type - _WSTRING(0);

			offset += len * 2;
		} else if (slzr->type > _STRING(0) && slzr->type <= _STRING(0x1000)) {
			unsigned len = slzr->type - _STRING(0);

			offset += len;
		} else if (slzr->type > _ARRAY_START(0) && slzr->type <= _ARRAY_START(0x1000)) {
			unsigned cnt = slzr->type - _ARRAY_START(0);

			slzr++;
			rc = serializer_get_offset(slzr, NULL);
			offset += rc * cnt;
			while (slzr->type != _ARRAY_END) {
				slzr++;
			}
		} else if (slzr->type == _OBJECT_START) {
			struct serializer *nested_slzr = slzr->ctx;

			if (nested_slzr) {
				rc = serializer_get_offset(nested_slzr, NULL);
			} else {
				slzr++;
				rc = serializer_get_offset(slzr, NULL);
				while (slzr->type != _OBJECT_END) {
					slzr++;
				}
			}

			offset += rc;
		} else if (slzr->type == _CUSTOM) {
			offset += slzr->fn(g_nullfile, slzr, (void *)g_zeroes);
		} else if (slzr->type == _ARRAY_END) {
			break;
		} else if (slzr->type == _OBJECT_END) {
			break;
		} else if (slzr->type == _TYPE_END) {
			break;
		}
		slzr++;
	}

	if (name) {
		return -1;
	} else {
		return offset;
	}
}

void *
serializer_get_field(struct serializer *slzr, const char *name, void *data)
{
	return data + serializer_get_offset(slzr, name);
}

static void
_deserialize(struct cjson *obj, struct serializer **slzr_table_p, void **data_p, bool is_root_obj)
{
	struct serializer *slzr = *slzr_table_p;
	void *data = *data_p;

	while (true) {
		struct cjson *json_f;

		if (slzr->type == _ARRAY_END) {
			break;
		} else if (slzr->type == _OBJECT_END) {
			break;
		} else if (slzr->type == _TYPE_END) {
			break;
		}

		/* TODO make id fields _CUSTOM in all serializers */
		if (is_root_obj && strcmp(slzr->name, "id") == 0) {
			data += 4;
			slzr++;
			continue;
		}

		if (strlen(slzr->name) == 0) {
			assert(obj->type != CJSON_TYPE_OBJECT);
			json_f = obj;
		} else {
			json_f = cjson_obj(obj, slzr->name);
		}

		if (slzr->type == _INT8) {
			if (json_f->type != CJSON_TYPE_NONE) {
				uint32_t _data = *(uint8_t *)data;

				deserialize_log(json_f, &_data);
				*(uint8_t *)data = json_f->i;
			}
			data += 1;
		} else if (slzr->type == _INT16) {
			if (json_f->type != CJSON_TYPE_NONE) {
				uint32_t _data = *(uint8_t *)data;

				deserialize_log(json_f, &_data);
				*(uint16_t *)data = json_f->i;
			}
			data += 2;
		} else if (slzr->type == _INT32) {
			if (json_f->type != CJSON_TYPE_NONE) {
				deserialize_log(json_f, data);
				*(uint32_t *)data = json_f->i;
			}
			data += 4;
		} else if (slzr->type > _CONST_INT(0) && slzr->type <= _CONST_INT(0x1000)) {
			continue;
		} else if (slzr->type == _FLOAT) {
			if (json_f->type != CJSON_TYPE_NONE) {
				deserialize_log(json_f, data);
				*(float *)data = json_f->d;
			}
			data += 4;
		} else if (slzr->type > _WSTRING(0) && slzr->type <= _WSTRING(0x1000)) {
			unsigned len = slzr->type - _WSTRING(0);

			char buf[2048] = {0};
			struct cjson *json = json_f;
			while (json && json->key) {
				char buf2[2048];
				memcpy(buf2, buf, sizeof(buf2));
				snprintf(buf, sizeof(buf), "%s->%s", json->key, buf2);
				json = json->parent;
			}
			if (strlen(buf) > 2) {
				buf[strlen(buf) - 2] = 0;
			}

			if (json_f->type != CJSON_TYPE_NONE) {
				normalize_json_string(json_f->s);
				deserialize_log(json_f, data);
				memset(data, 0, len * 2);
				change_charset("UTF-8", "UTF-16LE", json_f->s, strlen(json_f->s), (char *)data, len * 2);
			}
			data += len * 2;
		} else if (slzr->type > _STRING(0) && slzr->type <= _STRING(0x1000)) {
			unsigned len = slzr->type - _STRING(0);

			char buf[2048] = {0};
			struct cjson *json = json_f;
			while (json && json->key) {
				char buf2[2048];
				memcpy(buf2, buf, sizeof(buf2));
				snprintf(buf, sizeof(buf), "%s->%s", json->key, buf2);
				json = json->parent;
			}
			if (strlen(buf) > 2) {
				buf[strlen(buf) - 2] = 0;
			}

			if (json_f->type != CJSON_TYPE_NONE) {
				normalize_json_string(json_f->s);
				deserialize_log(json_f, data);
				memset(data, 0, len * 2);
				change_charset("UTF-8", "GB2312", json_f->s, strlen(json_f->s), (char *)data, len);
			}
			data += len;
		} else if (slzr->type > _ARRAY_START(0) && slzr->type <= _ARRAY_START(0x1000)) {
			unsigned cnt = slzr->type - _ARRAY_START(0);
			void *arr_data_end = data;
			struct serializer *tmp_slzr = ++slzr;
			size_t arr_el_size = 0;
			struct cjson *json_el;

			/* serialize to /dev/null to get arr element's size */
			_serialize(g_nullfile, &tmp_slzr, &arr_data_end, 1, true, false, false);
			arr_el_size = (size_t)((uintptr_t)arr_data_end - (uintptr_t)data);

			json_el = json_f->a;
			while (json_el) {
				char *end;
				unsigned idx = strtod(json_el->key, &end);
				if (end == json_el->key || errno == ERANGE) {
					/* non-numeric key in array */
					json_el = json_el->next;
					assert(false);
					continue;
				}

				void *arr_data_el = data + arr_el_size * idx;
				tmp_slzr = slzr;

				_deserialize(json_el, &tmp_slzr, &arr_data_el, false);
				json_el = json_el->next;
			}

			data += arr_el_size * cnt;
			slzr = tmp_slzr;
		} else if (slzr->type == _OBJECT_START) {
			struct serializer *nested_slzr = slzr->ctx;
			if (nested_slzr) {
				_deserialize(json_f, &nested_slzr, &data, false);
			} else {
				slzr++;
				_deserialize(json_f, &slzr, &data, false);
			}
		} else if (slzr->type == _CUSTOM) {
			if (slzr->des_fn) {
				data += slzr->des_fn(json_f, data, slzr->ctx);
			}
		}

		slzr++;
	}

	*slzr_table_p = slzr;
	*data_p = data;
}


void
deserialize(struct cjson *obj, struct serializer *slzr_table, void *data)
{
	_deserialize(obj, &slzr_table, &data, true);
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
