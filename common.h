/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_COMMON_H
#define PW_COMMON_H

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>

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

#define _TYPE_END 0
#define _INT16 1
#define _INT32 2
#define _FLOAT 3
#define _ARRAY_END 4
#define _CUSTOM 5
#define _WSTRING(n) (0x1000 + (n))
#define _STRING(n) (0x2000 + (n))
#define _ARRAY_START(n) (0x3000 + (n))
#define _CONST_INT(n) (0x4001 + (n))

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
struct pw_idmap *pw_idmap_init(void);
void *pw_idmap_get(struct pw_idmap *map, long long id, void *type);
void pw_idmap_set(struct pw_idmap *map, long long id, void *type, void *data);

#endif /* PW_COMMON_H */
