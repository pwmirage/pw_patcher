/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <zlib.h>

#include "pw_pck.h"

static int
read_entry(struct pw_pck *pck, FILE *fp, size_t compressed_size)
{
	struct pw_pck_entry_header ent_hdr;
	int rc;

	if (compressed_size == sizeof(ent_hdr)) {
		fread(&ent_hdr, compressed_size, 1, fp);
	} else {
		char *buf = calloc(1, compressed_size);

		if (!buf) {
			PWLOG(LOG_ERROR, "calloc() failed\n");
			return -ENOMEM;
		}

		fread(buf, compressed_size, 1, fp);

		uLongf uncompressed_size = sizeof(ent_hdr);
		rc = uncompress((Bytef *)&ent_hdr, &uncompressed_size, (const Bytef *)buf, compressed_size);
		free(buf);
		if (rc != Z_OK || uncompressed_size != sizeof(ent_hdr)) {
			return rc;
		}
	}

	char utf8_name[512];
	sprint(utf8_name, sizeof(utf8_name), ent_hdr.path, sizeof(ent_hdr.path));
	PWLOG(LOG_INFO, "entry: %s\n", utf8_name);
	return 0;
}

int
pw_pck_read(struct pw_pck *pck, const char *path)
{
	FILE *fp;
	size_t fsize;
	int rc;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		PWLOG(LOG_ERROR, "fopen() failed: %d\n", errno);
		return -errno;
	}

	fread(&pck->hdr, sizeof(pck->hdr), 1, fp);
	if (pck->hdr.magic0 != PCK_HEADER_MAGIC0 || pck->hdr.magic1 != PCK_HEADER_MAGIC1) {
		PWLOG(LOG_ERROR, "invalid pck header: %s\n", path);
		goto err_close;
	}

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);

	if (fsize < 4) {
		goto err_close;
	}


	fseek(fp, fsize - 4, SEEK_SET);
	fread(&pck->ver, 4, 1, fp);

	if (pck->ver != 0x20002 && pck->ver != 0x20001) {
		PWLOG(LOG_ERROR, "invalid pck version: 0x%x\n", pck->ver);
		goto err_close;
	}

	fseek(fp, fsize - 8, SEEK_SET);
	fread(&pck->entry_cnt, 4, 1, fp);

	fseek(fp, fsize - 8 - sizeof(struct pw_pck_footer), SEEK_SET);
	fread(&pck->ftr, sizeof(pck->ftr), 1, fp);

	if (pck->ftr.magic0 != PCK_FOOTER_MAGIC0 || pck->ftr.magic1 != PCK_FOOTER_MAGIC1) {
		PWLOG(LOG_ERROR, "invalid footer magic\n");
		goto err_close;
	}

	if(strstr(pck->ftr.description, "lica File Package") == NULL) {
		PWLOG(LOG_ERROR, "invalid pck description: \"%s\"\n", pck->ftr.description);
		goto err_close;
	}

	pck->ftr.entry_list_off ^= PW_PCK_XOR1;
	fseek(fp, pck->ftr.entry_list_off, SEEK_SET);

	for (int i = 0; i < pck->entry_cnt; i++) {
		uint32_t compressed_size;
		uint32_t compressed_size2;

		fread(&compressed_size, 4, 1, fp);
		compressed_size ^= PW_PCK_XOR1;

		fread(&compressed_size2, 4, 1, fp);
		compressed_size2 ^= PW_PCK_XOR2;

		if (compressed_size != compressed_size2) {
			PWLOG(LOG_ERROR, "Entry size mismatch: idx=%d, size1=%u, size2=%u\n",
					i, compressed_size, compressed_size2);
			goto err_cleanup;
		}

		rc = read_entry(pck, fp, compressed_size);
		if (rc != 0) {
			PWLOG(LOG_ERROR, "read_entry() failed: %d\n", rc);
			goto err_cleanup;
		}
	}

	fclose(fp);
	return 0;

err_cleanup:
err_close:
	fclose(fp);
	return -1;
}
