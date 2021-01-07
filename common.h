/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_COMMON_H
#define PW_COMMON_H

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>

struct serializer {
	const char *name;
	unsigned type;
	/* custom parser. returns number of bytes processed */
	size_t (*fn)(FILE *fp, void *data);
};

#define TYPE_END 0
#define INT16 1
#define INT32 2
#define FLOAT 3
#define ARRAY_END 4
#define CUSTOM 5
#define WSTRING(n) (0x1000 + (n))
#define STRING(n) (0x2000 + (n))
#define ARRAY_START(n) (0x3000 + (n))
#define CONST_INT(n) (0x4001 + (n))

int download(const char *url, const char *filename);
int readfile(const char *path, char **buf, size_t *len);
int download_mem(const char *url, char **buf, size_t *len);

void *stream_download(const char *url);
int stream_read(void *stream_data, char *buf, size_t buflen, size_t *bytes_read);
void stream_close(void *stream_data);

void sprint(char *dst, size_t dstsize, const char *src, int srcsize);
void fsprint(FILE *fp, const char *buf, int maxlen);
void fwsprint(FILE *fp, const uint16_t *buf, int maxlen);
void fwsprintf(FILE *fp, const char *fmt, const uint16_t *buf, int maxlen);
void wsnprintf(uint16_t *dst, size_t dstsize, const char *src);

long serialize(FILE *fp, struct serializer *slzr_table,
		void *data, unsigned data_cnt);

#define LOG_ERROR 0
#define LOG_INFO 1
void pwlog(int type, const char *fmt, ...);

#endif /* PW_COMMON_H */
