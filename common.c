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

#include "common.h"

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
download_wget(const char *url, char *filename)
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
download(const char *url, char *filename)
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

void
fwsprint(FILE *fp, const uint16_t *buf, int maxlen)
{
	uint16_t v;
	int i;

	for (i = 0; i < maxlen; i++) {
		v = *buf++;

		if (v == 9675) {
			char utf8_white_circle[4] = { 0xE2, 0x97, 0x8B, 0 };
			fprintf(fp, utf8_white_circle);
			continue;
		} else if (v == 9679) {
			char utf8_black_circle[4] = { 0xE2, 0x97, 0x8F, 0 };
			fprintf(fp, utf8_black_circle);
			continue;
		} else if (v == 9733) {
			char utf8_black_star[4] = { 0xE2, 0x98, 0x85, 0 };
			fprintf(fp, utf8_black_star);
			continue;
		} else	if (v == 9734) {
			char utf8_white_star[4] = { 0xE2, 0x98, 0x86, 0 };
			fprintf(fp, utf8_white_star);
			continue;
		}

		if (v == 0 || v > 127) break;
		if (v == '\r') continue;

		if (v == '\n') {
			fprintf(fp, "\\n");
		} else if (v == '"') {
			fprintf(fp, "\\\"");
		} else {
			putc((char)v, fp);
		}
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

