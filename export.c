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

void extra_drops_load(const char *filename);

static void
load_icons(void) {
	FILE *fp = fopen("patcher/iconlist_ivtrm.txt", "r");
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	if (fp == NULL) {
		PWLOG(LOG_ERROR, "failed to open iconlist_ivtrm.txt\n");
		return;
	}

	/* skip header */
	for (int i = 0; i < 4; i++) {
		getline(&line, &len, fp);
		line = NULL;
		len = 0;
	}

	unsigned i = 0;
	while (i < PW_ELEMENTS_ICON_COUNT &&
			(read = getline(&line, &len, fp)) != -1) {
		int printed_cnt = snprintf(g_icon_names[i], sizeof(g_icon_names[0]), "%s", line);
		if (g_icon_names[i][printed_cnt - 1] == '\n') {
			g_icon_names[i][printed_cnt - 1] = 0;
		}

		i++;
		len = 0;
	}

	free(line);
}

static int
load_descriptions(void)
{
	FILE *fp = fopen("patcher/item_ext_desc.txt", "r");
	size_t len = 0;
	ssize_t read;
	char *line = NULL;

	if (fp == NULL) {
		PWLOG(LOG_ERROR, "Can't open item_ext_desc.txt\n");
		return -1;
	}

	/* skip header */
	for (int i = 0; i < 5; i++) {
		getline(&line, &len, fp);
	}

	unsigned i = 0;

	len = 2048;
	line = malloc(len);
	if (!line) {
		PWLOG(LOG_ERROR, "malloc() failed\n");
		return -1;
	}
	while ((read = getline(&line, &len, fp)) != -1) {
		char *id, *txt;

		id = strtok(line, " ");
		if (id == NULL || strlen(id) == 0) {
			break;
		}

		txt = strtok(NULL, "\"");
		txt = strtok(NULL, "\"");
		g_item_descs[atoi(id)] = txt;
		/* FIXME: line is technically leaked */

		while (*txt) {
			if (*txt == '\r') {
				*txt = '\n';
			}
			txt++;
		}

		len = 2048;
		line = malloc(len);
		if (!line) {
			PWLOG(LOG_ERROR, "malloc() failed\n");
			return -1;
		}
		i++;
	}

	PWLOG(LOG_INFO, "Parsed %u item descriptions\n", i);
	fclose(fp);
	return 0;
}


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

	load_icons();
	load_descriptions();
	pw_elements_prepare(&elements);

	char tmpbuf[1024];
	char *buf;
	size_t num_bytes = 0;

	snprintf(tmpbuf, sizeof(tmpbuf), "https://pwmirage.com/editor/project/cache/public/rates.json");
	rc = download_mem(tmpbuf, &buf, &num_bytes);
	if (rc) {
		PWLOG(LOG_ERROR, "download_mem failed for %s\n", tmpbuf);
		return 1;
	}

	struct cjson *rates = cjson_parse(buf);
	pw_elements_adjust_rates(&elements, rates);
	cjson_free(rates);
	free(buf);

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

	char *buf;
	size_t num_bytes = 0;
	char tmpbuf[1024];

	snprintf(tmpbuf, sizeof(tmpbuf), "https://pwmirage.com/editor/project/cache/public/rates.json");
	rc = download_mem(tmpbuf, &buf, &num_bytes);
	if (rc) {
		PWLOG(LOG_ERROR, "download_mem failed for %s\n", tmpbuf);
		return 1;
	}

	struct cjson *rates = cjson_parse(buf);
	pw_tasks_adjust_rates(&taskf, rates);
	cjson_free(rates);
	free(buf);

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
	int rc = 0;

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
	} else if (strcmp(type, "extra_drops") == 0) {
		extra_drops_load("extra_drops.sev");
	} else {
		print_help(argv);
		return 1;
	}

	return rc;
}
