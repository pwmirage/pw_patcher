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

#include "serializer.h"

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

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define _CHAIN_TABLE _CUSTOM, serialize_chunked_table_fn, deserialize_chunked_table_fn

extern const char g_zeroes[4096];
extern FILE *g_nullfile;

struct cjson;
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

struct pw_task_file {
	uint32_t magic;
	uint32_t version;
	struct pw_chain_table *tasks;
	unsigned max_arr_idx;
	unsigned max_dialogue_id;
};

#define PW_CHAIN_TABLE_FOREACH(_var, _chain, _table) \
	for ((_chain) = (_table)->chain; (_chain); (_chain) = (_chain)->next) \
	for ((_var) = (void *)(_chain)->data; (_var) != (void *)(_chain)->data + (_chain)->count * (_table)->el_size; (_var) += (_table)->el_size)

int pw_chain_table_init(struct pw_chain_table *table, const char *name, struct serializer *serializer, size_t el_size, size_t count);
struct pw_chain_table *pw_chain_table_alloc(const char *name, struct serializer *serializer, size_t el_size, size_t count);
struct pw_chain_table *pw_chain_table_fread(FILE *fp, const char *name, size_t el_count, struct serializer *el_serializer);
void *pw_chain_table_new_el(struct pw_chain_table *table);

size_t serialize_chunked_table_fn(FILE *fp, struct serializer *f, void *data);
size_t deserialize_chunked_table_fn(struct cjson *f, struct serializer *_slzr, void *data);

int download(const char *url, const char *filename);
int readfile(const char *path, char **buf, size_t *len);
int download_mem(const char *url, char **buf, size_t *len);

void sprint(char *dst, size_t dstsize, const char *src, int srcsize);
void fsprint(FILE *fp, const char *buf, int maxlen);
void fwsprint(FILE *fp, const uint16_t *buf, int maxlen);
void fwsprintf(FILE *fp, const char *fmt, const uint16_t *buf, int maxlen);
void wsnprintf(uint16_t *dst, size_t dstsize, const char *src);

int change_charset(char *src_charset, char *dst_charset, char *src, long srclen, char *dst, long dstlen);
void normalize_json_string(char *str);

#define LOG_ERROR 0
#define LOG_INFO 1
#define LOG_DEBUG 100
#define LOG_DEBUG_1 101
#define LOG_DEBUG_5 105
extern int g_pwlog_level;
void pwlog(int type, const char *filename, unsigned lineno, const char *fnname, const char *fmt, ...);
#define PWLOG(type, ...) pwlog((type), __FILE__, __LINE__, __func__, __VA_ARGS__)

struct pw_idmap;
typedef void (*pw_idmap_async_fn)(void *el, void *ctx);

struct pw_idmap *pw_idmap_init(const char *name, bool clean_load);
long pw_idmap_register_type(struct pw_idmap *map);
void *pw_idmap_get(struct pw_idmap *map, long long lid, long type);
int pw_idmap_get_async(struct pw_idmap *map, long long lid, long type, pw_idmap_async_fn fn, void *fn_ctx);
void pw_idmap_set(struct pw_idmap *map, long long lid, long id, long type, void *data);
void pw_idmap_end_type_load(struct pw_idmap *map, long type, uint32_t max_id);
int pw_idmap_save(struct pw_idmap *map);

int pw_version_load(struct pw_version *ver);
int pw_version_save(struct pw_version *ver);

int pw_tasks_load(struct pw_task_file *taskf, const char *path);
int pw_tasks_serialize(struct pw_task_file *taskf, const char *filename);
int pw_tasks_save(struct pw_task_file *taskf, const char *path, bool is_server);
void pw_tasks_adjust_rates(struct pw_task_file *taskf, struct cjson *rates);

#endif /* PW_COMMON_H */
