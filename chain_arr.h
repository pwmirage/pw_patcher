/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2020 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_CHAIN_ARR_H
#define PW_CHAIN_ARR_H

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "serializer.h"
#include "cjson.h"

struct pw_chain_el {
	struct pw_chain_el *next;
	size_t capacity;
	size_t count;
	char data[0];
};

struct pw_chain_table {
	const char *name;
	struct serializer *serializer;
	long idmap_type;
	size_t el_size;
	struct pw_chain_el *chain;
	struct pw_chain_el *chain_last;
};

#define _CHAIN_TABLE _CUSTOM, serialize_chunked_table_fn, deserialize_chunked_table_fn
#define PW_CHAIN_TABLE_FOREACH(_var, _chain, _table) \
	for ((_chain) = (_table)->chain; (_chain); (_chain) = (_chain)->next) \
	for ((_var) = (void *)(_chain)->data; (_var) != (void *)(_chain)->data + (_chain)->count * (_table)->el_size; (_var) += (_table)->el_size)

int pw_chain_table_init(struct pw_chain_table *table, const char *name, struct serializer *serializer, size_t el_size, size_t count);
struct pw_chain_table *pw_chain_table_alloc(const char *name, struct serializer *serializer, size_t el_size, size_t count);
struct pw_chain_table *pw_chain_table_fread(FILE *fp, const char *name, size_t el_count, struct serializer *el_serializer);
void *pw_chain_table_new_el(struct pw_chain_table *table);

size_t serialize_chunked_table_fn(FILE *fp, struct serializer *f, void *data);
size_t deserialize_chunked_table_fn(struct cjson *f, struct serializer *_slzr, void *data);

#endif /* PW_CHAIN_ARR_H */
