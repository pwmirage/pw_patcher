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

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif


extern const char g_zeroes[4096];
extern FILE *g_nullfile;

#define PW_VERSION_MAGIC 0xb78e97a1
struct pw_version {
	uint32_t magic;
	uint32_t version;
	uint32_t generation;
	char branch[64];
	char cur_hash[64];
};

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

int pw_version_load(struct pw_version *ver);
int pw_version_save(struct pw_version *ver);

#endif /* PW_COMMON_H */
