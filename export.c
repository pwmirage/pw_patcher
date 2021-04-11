/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
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
#include "pw_tasks.h"

static int
print_elements(const char *path)
{
	struct pw_elements elements;
	int rc;
	rc = pw_elements_load(&elements, path, NULL);
	if (rc != 0) {
		PWLOG(LOG_ERROR, "pw_elements_load(%s) failed: %d\n", path, rc);
		return 1;
	}

	pw_elements_prepare(&elements);
	pw_elements_serialize(&elements);

	pw_elements_save(&elements, "config/elements.data.srv", true);
	pw_elements_save(&elements, "config/elements.data.cl", false);

	return 0;
}

static int
print_npcgen()
{
	struct pw_npc_file npc;
	char tmpbuf[1024];
	int i;

	for (i = 0; i < PW_MAX_MAPS; i++) {
		const struct map_name *map = &g_map_names[i];
		snprintf(tmpbuf, sizeof(tmpbuf), "mkdir -p json/%s", map->name);
		system(tmpbuf);

		memset(&npc, 0, sizeof(npc));
		snprintf(tmpbuf, sizeof(tmpbuf), "config/%s/npcgen.data", map->dir_name);
		pw_npcs_load(&npc, map->name, tmpbuf, true);

		snprintf(tmpbuf, sizeof(tmpbuf), "json/%s/triggers.json", map->name);
		pw_npcs_serialize(&npc, tmpbuf);
	}

	return 0;
}

static int
print_tasks(const char *path)
{
	struct pw_task_file taskf;
	int rc;

	rc = pw_tasks_load(&taskf, path, NULL);
	if (rc) {
		PWLOG(LOG_ERROR, "pw_tasks_load(%s) failed: %d\n", path, rc);
		return rc;
	}

	rc = pw_tasks_serialize(&taskf, "tasks.json");
	if (rc) {
		PWLOG(LOG_ERROR, "pw_tasks_serialize() failed: %d\n", rc);
		return rc;
	}

	//pw_tasks_save(&taskf, "tasks2.data", true);

	return 0;
}

static void
print_help(char *argv[0])
{
	printf("%s npcs\n", argv[0]);
	printf("%s elements\n", argv[0]);
	printf("%s tasks\n", argv[0]);
}
int
main(int argc, char *argv[])
{
	int rc;

	if (argc < 2) {
		print_help(argv);
		return 0;
	}

	setlocale(LC_ALL, "en_US.UTF-8");
	const char *type = argv[1];

	if (strcmp(type, "elements") == 0) {
		rc = print_elements(argv[2]);
	} else if (strcmp(type, "tasks") == 0) {
		rc = print_tasks(argv[2]);
	} else if (strcmp(type, "npcs") == 0) {
		print_npcgen();
	} else {
		print_help(argv);
		return 1;
	}

	return rc;
}
