/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2021 Darek Stojaczyk for pwmirage.com
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

#include "serializer.h"
#include "common.h"
#include "cjson.h"

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
				if (!obj_printed || (*(uint8_t *)data != 0 && slzr->name[0] != '_')) {
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

				if (ftell(fp) <= pre_pos + 2 || slzr_name[0] == '_') {
					fseek(fp, pre_name_pos, SEEK_SET);
				} else {
					fprintf(fp, ",");
					nonzero = true;
				}
			} else if (slzr->type == _OBJECT_START) {
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
					if (ftell(fp) <= pre_pos + 2) {
						/* nothing printed in the object, skip its name */
						fseek(fp, pre_name_pos, SEEK_SET);
					} else {
						fprintf(fp, ",");
						nonzero = true;
					}
				} else {
					/* just advance the slzr and data */
					slzr++;
					_serialize(g_nullfile, &slzr, &data, 1, true, false, true);
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

static int
_serializer_get_offset(struct serializer **slzr_p, const char *name)
{
	struct serializer *slzr = *slzr_p;
	int offset = 0;
	int rc;

	while (true) {
		if (name && strcmp(slzr->name, name) == 0) {
			*slzr_p = slzr;
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

	*slzr_p = slzr;
	if (name) {
		return -1;
	} else {
		return offset;
	}
}

int
serializer_get_offset(struct serializer *slzr, const char *name)
{
	return _serializer_get_offset(&slzr, name);
}

int
serializer_get_size(struct serializer *slzr_table)
{
	return serializer_get_offset(slzr_table, NULL);
}

int
serializer_get_offset_slzr(struct serializer *slzr_table, const char *name, struct serializer **slzr)
{
	int rc = _serializer_get_offset(&slzr_table, name);

	*slzr = slzr_table;
	return rc;
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
		if (is_root_obj && slzr->type == _INT32 && strcmp(slzr->name, "id") == 0 && slzr->ctx == NULL) {
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
			/* nothing */
		} else if (slzr->type == _FLOAT) {
			if (json_f->type != CJSON_TYPE_NONE) {
				deserialize_log(json_f, data);
				if (json_f->type == CJSON_TYPE_FLOAT) {
					*(float *)data = json_f->d;
				} else {
					*(float *)data = json_f->i;
				}
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
				normalize_json_string(json_f->s, true);
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
				normalize_json_string(json_f->s, true);
				deserialize_log(json_f, data);
				memset(data, 0, len);
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
				data += slzr->des_fn(json_f, slzr, data);
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


