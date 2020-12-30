/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020 Darek Stojaczyk for pwmirage.com
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

#include "pw_elements.h"
#include "pw_npc.h"

static int
patch(const char *elements_path, const char *url)
{
	struct pw_elements elements;
	int rc;

	rc = pw_elements_load(&elements, elements_path);
	if (rc != 0) {
		return 1;
	}

	pw_elements_serialize(&elements);
	return 0;
}

int
main(int argc, char *argv[])
{
	int rc;
	setlocale(LC_ALL, "en_US.UTF-8");

    if (argc < 3) {
        printf("./%s path_to_npcgen.data world_name\n", argv[0]);
        printf("./%s path_to_elements.data\n", argv[0]);
        return 0;
    }

    rc = patch(argv[1], argv[2]);

	return rc;
}
