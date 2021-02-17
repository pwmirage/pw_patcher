/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2021 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_SERIALIZER_H
#define PW_SERIALIZER_H

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#define _TYPE_END 0
#define _INT8 1
#define _INT16 1
#define _INT32 2
#define _FLOAT 3
#define _ARRAY_END 4
#define _CUSTOM 5
#define _OBJECT_END 6
#define _WSTRING(n) (0x1000 + (n))
#define _STRING(n) (0x2000 + (n))
#define _ARRAY_START(n) (0x3000 + (n))
#define _OBJECT_START 0x4001
#define _CONST_INT(n) (0x4002 + (n))

struct cjson;
struct serializer {
	const char *name;
	unsigned type;
	/* custom parser. returns number of bytes processed */
	size_t (*fn)(FILE *fp, struct serializer *slzr, void *data);
	/* custom deserializer. returns number of bytes processed */
	size_t (*des_fn)(struct cjson *f, struct serializer *slzr, void *data);
	/* user context */
	void *ctx;
};

long serialize(FILE *fp, struct serializer *slzr_table, void *data, unsigned data_cnt);
long _serialize(FILE *fp, struct serializer **slzr_table_p, void **data_p,
		unsigned data_cnt, bool skip_empty_objs, bool newlines, bool force_object);
int serializer_get_size(struct serializer *slzr_table);
int serializer_get_offset(struct serializer *slzr_table, const char *name);
void *serializer_get_field(struct serializer *slzr_table, const char *name, void *data);
int serializer_get_offset_slzr(struct serializer *slzr_table, const char *name, struct serializer **slzr);

void deserialize(struct cjson *obj, struct serializer *slzr_table, void *data);
void deserialize_log(struct cjson *json_f, void *data);

#endif /* PW_SERIALIZER_H */
