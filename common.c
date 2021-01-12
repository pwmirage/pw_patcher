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

static FILE *g_nullfile;

#ifdef __MINGW32__
#include <wininet.h>

static void
download_wininet(const char *url, char *filename)
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
	hURL = InternetOpenUrl(hInternetSession, url,	NULL, 0, 0, 0);

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
	if (rc == -1) {
		return rc;
	}

	return 0;
}
#endif

int
download(const char *url, const char *filename)
{
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

static int
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
    while (*b) {
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
    while (*b) {
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

static long
_serialize(FILE *fp, struct serializer **slzr_table_p, void **data_p,
		unsigned data_cnt, bool skip_empty_objs, bool newlines)
{
	unsigned data_idx;
	struct serializer *slzr;
	void *data = *data_p;
	bool nonzero, obj_printed;
	long sz, arr_sz;

	fprintf(fp, "[");
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
			if (slzr->type == INT16) {
				if (!obj_printed || (*(uint16_t *)data != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%u,", *(uint16_t *)data);
					nonzero = *(uint16_t *)data != 0;
				}
				data += 2;
			} else if (slzr->type == INT32) {
				if (!obj_printed || (*(uint32_t *)data != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%u,", *(uint32_t *)data);
					nonzero = *(uint32_t *)data != 0;
				}
				data += 4;
			} else if (slzr->type > CONST_INT(0) && slzr->type <= CONST_INT(0x1000)) {
				unsigned num = slzr->type - CONST_INT(0);

				if (!obj_printed || (num != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%u,", num);
					nonzero = num != 0;
				}
			} else if (slzr->type == FLOAT) {
				if (!obj_printed || (*(float *)data != 0 && slzr->name[0] != '_')) {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "%.8f,", *(float *)data);
					nonzero = *(float *)data != 0;
				}
				data += 4;
			} else if (slzr->type > WSTRING(0) && slzr->type <= WSTRING(0x1000)) {
				unsigned len = slzr->type - WSTRING(0);

				if (slzr->name[0] != '_') {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "\"");
					fwsprint(fp, (const uint16_t *)data, len);
					fprintf(fp, "\",");
					nonzero = *(uint16_t *)data != 0;
				}
				data += len * 2;
			} else if (slzr->type > STRING(0) && slzr->type <= STRING(0x1000)) {
				unsigned len = slzr->type - STRING(0);

				if (slzr->name[0] != '_') {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					fprintf(fp, "\"");
					fsprint(fp, (const char *)data, len);
					fprintf(fp, "\",");
					nonzero = *(char *)data != 0;
				}
				data += len;
			} else if (slzr->type > ARRAY_START(0) && slzr->type <= ARRAY_START(0x1000)) {
				unsigned cnt = slzr->type - ARRAY_START(0);

				if (slzr->name[0] != '_') {
					if (slzr->name[0] != 0) {
						fprintf(fp, "\"%s\":", slzr->name);
					}
					slzr++;
					_serialize(fp, &slzr, &data, cnt, true, false);
					fprintf(fp, ",");
				}
			} else if (slzr->type == CUSTOM) {
				data += slzr->fn(fp, data);
                nonzero = true;
			} else if (slzr->type == ARRAY_END) {
				break;
			} else if (slzr->type == TYPE_END) {
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

	fprintf(fp, "]");

	*slzr_table_p = slzr;
	*data_p = data;
	return arr_sz + 1;
}

long
serialize(FILE *fp, struct serializer *slzr_table, void *data, unsigned data_cnt)
{
	return _serialize(fp, &slzr_table, &data, data_cnt, false, true);
}

static void
_deserialize(struct cjson *obj, struct serializer **slzr_table_p, void **data_p)
{
	struct serializer *slzr = *slzr_table_p;
	void *data = *data_p;

	while (true) {
		struct cjson *json_f;

		if (slzr->type == ARRAY_END) {
			break;
		} else if (slzr->type == TYPE_END) {
			break;
		}

		if (strcmp(slzr->name, "id") == 0) {
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

		if (slzr->type == INT16) {
			if (json_f->type != CJSON_TYPE_NONE) {
				*(uint16_t *)data = json_f->i;
			}
			data += 2;
		} else if (slzr->type == INT32) {
			if (json_f->type != CJSON_TYPE_NONE && strcmp(slzr->name, "id") != 0) {
				char buf[2048] = {0};
				struct cjson *json = json_f;
				while (json && json->key) {
					char buf2[2048];
					memcpy(buf2, buf, sizeof(buf2));
					snprintf(buf, sizeof(buf), "%s->%s", json->key, buf2);
					json = json->parent;
				}
				buf[strlen(buf) - 2] = 0;
				pwlog(LOG_INFO, "patching \"%s\" (prev:%d, new:%d)\n", buf, *(uint32_t *)data, json_f->i);

				/* don't override IDs */
				*(uint32_t *)data = json_f->i;
			}
			data += 4;
		} else if (slzr->type > CONST_INT(0) && slzr->type <= CONST_INT(0x1000)) {
			continue;
		} else if (slzr->type == FLOAT) {
			if (json_f->type != CJSON_TYPE_NONE) {
				*(float *)data = json_f->d;
			}
			data += 4;
		} else if (slzr->type > WSTRING(0) && slzr->type <= WSTRING(0x1000)) {
			unsigned len = slzr->type - WSTRING(0);

			char buf[2048] = {0};
			struct cjson *json = json_f;
			while (json && json->key) {
				char buf2[2048];
				memcpy(buf2, buf, sizeof(buf2));
				snprintf(buf, sizeof(buf), "%s->%s", json->key, buf2);
				json = json->parent;
			}
			buf[strlen(buf) - 2] = 0;

			if (json_f->type != CJSON_TYPE_NONE) {
				memset(data, 0, len * 2);
				change_charset("UTF-8", "UTF-16LE", json_f->s, strlen(json_f->s), (char *)data, len * 2);
			}
			data += len * 2;
		} else if (slzr->type > STRING(0) && slzr->type <= STRING(0x1000)) {
			unsigned len = slzr->type - STRING(0);

			char buf[2048] = {0};
			struct cjson *json = json_f;
			while (json && json->key) {
				char buf2[2048];
				memcpy(buf2, buf, sizeof(buf2));
				snprintf(buf, sizeof(buf), "%s->%s", json->key, buf2);
				json = json->parent;
			}
			buf[strlen(buf) - 2] = 0;

			if (json_f->type != CJSON_TYPE_NONE) {
				memset(data, 0, len * 2);
				change_charset("UTF-8", "GB2312", json_f->s, strlen(json_f->s), (char *)data, len);
			}
			data += len;
		} else if (slzr->type > ARRAY_START(0) && slzr->type <= ARRAY_START(0x1000)) {
			unsigned cnt = slzr->type - ARRAY_START(0);
			void *arr_data_end = data;
			struct serializer *tmp_slzr = ++slzr;
			size_t arr_el_size = 0;
			struct cjson *json_el;

			/* serialize to /dev/null to get arr element's size */
			_serialize(g_nullfile, &tmp_slzr, &arr_data_end, 1, true, false);
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

				_deserialize(json_el, &tmp_slzr, &arr_data_el);
				json_el = json_el->next;
			}

			data += arr_el_size * cnt;
			slzr = tmp_slzr;
		} else if (slzr->type == CUSTOM) {
			if (slzr->des_fn) {
				data += slzr->des_fn(obj, data);
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
	_deserialize(obj, &slzr_table, &data);
}

void
pwlog(int type, const char *fmt, ...)
{
	va_list args;
	const char *type_str;

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
	

	va_start(args, fmt);
	fprintf(stderr, "%s: ", type_str);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

static void __attribute__((constructor))
common_init()
{
	g_nullfile = fopen("/dev/null", "w");
	if (g_nullfile == NULL) {
		g_nullfile = fopen("NULL", "w");
	}
}
