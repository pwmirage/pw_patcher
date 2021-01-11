/* SPDX-License-Identifier: MIT
 * Copyright(c) 2020 Darek Stojaczyk for pwmirage.com
 */

#include <stdlib.h>
#include "common.h"

struct pw_id_el {
	long long id;
	void *data;
	void *next;
};

#define PW_IDMAP_ARR_SIZE 3637

struct pw_idmap {
	struct pw_id_el *lists[PW_IDMAP_ARR_SIZE];
};

struct pw_idmap *
pw_idmap_init(void)
{
	return calloc(1, sizeof(struct pw_idmap));
}

void *
pw_idmap_get(struct pw_idmap *map, long long id)
{
	struct pw_id_el *el;

	el = map->lists[id % PW_IDMAP_ARR_SIZE];

	while (el) {
		if (el->id == id) {
			return el->data;
		}

		el = el->next;
	}

	return NULL;
}

void
pw_idmap_set(struct pw_idmap *map, long long id, void *data)
{
	struct pw_id_el *el, *last_el;

	el = map->lists[id % PW_IDMAP_ARR_SIZE];
	while (el) {
		if (el->id == id) {
			break;
		}

		last_el = el;
		el = el->next;
	}

	if (el) {
		el->data = data;
		return;
	}

	el = calloc(1, sizeof(struct pw_id_el));
	if (!el) {
		pwlog(LOG_ERROR, "pw_idmap_get: calloc() failed\n");
		return;
	}

	el->id = id;
	el->data = data;

	if (last_el) {
		last_el->next = el;
	} else {
		map->lists[id % PW_IDMAP_ARR_SIZE] = el;
	}
}
