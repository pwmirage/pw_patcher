/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020-2021 Darek Stojaczyk for pwmirage.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

#include "common.h"
#include "idmap.h"

int
main(int argc, char *argv[])
{
	struct pw_idmap *map;
	const char *filename;
	int rc;

	setlocale(LC_ALL, "en_US.UTF-8");

	if (argc < 3) {
		fprintf(stderr, "./%s path_in path_out\n", argv[0]);
		return 0;
	}

	filename = argv[1];
	map = pw_idmap_init("map", filename, true);
	if (!map) {
		fprintf(stderr, "Can\'t open %s\n", filename);
		return 1;
	}

	filename = argv[2];
	rc = pw_idmap_save(map, filename);
	if (rc != 0) {
		fprintf(stderr, "Can\'t save %s: %d\n", filename, -rc);
		return 1;
	}

	return 0;
}
