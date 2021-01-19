/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_COMMON_H
#define PW_COMMON_H

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#ifdef __MINGW32__
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* XXX MinGW bug? */
#define truncate(filepath, sz) \
	({ int fd = open(filepath, O_WRONLY); \
	   int rc = ftruncate(fd, sz); \
	   close(fd); \
	   rc; })
#endif

struct cjson;
struct serializer {
	const char *name;
	unsigned type;
	/* custom parser. returns number of bytes processed */
	size_t (*fn)(FILE *fp, void *data);
	/* custom deserializer. returns number of bytes processed */
	size_t (*des_fn)(struct cjson *f, void *data);
};

struct pw_chain_el;
struct pw_chain_table {
	const char *name;
	struct serializer *serializer;
	long idmap_type;
	size_t el_size;
	struct pw_chain_el *chain;
	struct pw_chain_el *chain_last;
};

struct pw_chain_el {
	struct pw_chain_el *next;
	size_t capacity;
	size_t count;
	char data[0];
};

#define PW_VERSION_MAGIC 0xb78e97a0
struct pw_version {
	uint32_t magic;
	uint32_t version;
	char branch[64];
	char cur_hash[64];
};

#define PW_CHAIN_TABLE_FOREACH(_var, _chain, _table) \
	for ((_chain) = (_table)->chain; (_chain); (_chain) = (_chain)->next) \
	for ((_var) = (void *)(_chain)->data; (_var) != (void *)(_chain)->data + (_chain)->count * (_table)->el_size; (_var) += (_table)->el_size)

int pw_chain_table_init(struct pw_chain_table *table, const char *name, struct serializer *serializer, size_t el_size, size_t count);
struct pw_chain_table *pw_chain_table_alloc(const char *name, struct serializer *serializer, size_t el_size, size_t count);
void *pw_chain_table_new_el(struct pw_chain_table *table);

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

int download(const char *url, const char *filename);
int readfile(const char *path, char **buf, size_t *len);
int download_mem(const char *url, char **buf, size_t *len);

void sprint(char *dst, size_t dstsize, const char *src, int srcsize);
void fsprint(FILE *fp, const char *buf, int maxlen);
void fwsprint(FILE *fp, const uint16_t *buf, int maxlen);
void fwsprintf(FILE *fp, const char *fmt, const uint16_t *buf, int maxlen);
void wsnprintf(uint16_t *dst, size_t dstsize, const char *src);

long serialize(FILE *fp, struct serializer *slzr_table,
		void *data, unsigned data_cnt);

void deserialize(struct cjson *obj, struct serializer *slzr_table, void *data);

#define LOG_ERROR 0
#define LOG_INFO 1
void pwlog(int type, const char *filename, unsigned lineno, const char *fnname, const char *fmt, ...);
#define PWLOG(type, ...) pwlog((type), __FILE__, __LINE__, __func__, __VA_ARGS__)

struct pw_idmap;
struct pw_idmap *pw_idmap_init(const char *name, bool clean_load);
long pw_idmap_register_type(struct pw_idmap *map);
void *pw_idmap_get(struct pw_idmap *map, long long lid, long type);
void pw_idmap_set(struct pw_idmap *map, long long lid, long id, long type, void *data);
void pw_idmap_end_type_load(struct pw_idmap *map, long type, uint32_t max_id);
int pw_idmap_save(struct pw_idmap *map);

int pw_version_load(struct pw_version *ver);
int pw_version_save(struct pw_version *ver);

#endif /* PW_COMMON_H */
