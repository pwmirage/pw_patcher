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

typedef void (*chain_arr_new_el_fn)(void *el, void *ctx);

struct pw_chain_table {
	/* just an associated string */
	const char *name;
	/* serializer for each el */
	struct serializer *serializer;
	/* associated idmap type */
	long idmap_type;
	/* size of each el */
	size_t el_size;
	/* called when new element is created */
	chain_arr_new_el_fn new_el_fn;
	void *new_el_ctx;
	/* first chain */
	struct pw_chain_el *chain;
	struct pw_chain_el *chain_last;
};

#define _CHAIN_TABLE _CUSTOM, serialize_chunked_table_fn, deserialize_chunked_table_fn
#define PW_CHAIN_TABLE_FOREACH(_var, _table) \
	for (struct { struct pw_chain_el *chain; uint32_t i; } _pw_chain_internal = { (_table)->chain, 0 }; _pw_chain_internal.chain; _pw_chain_internal.chain = _pw_chain_internal.chain->next, _pw_chain_internal.i = 0) \
	for ((_var) = (void *)_pw_chain_internal.chain->data; _pw_chain_internal.i < _pw_chain_internal.chain->count; (_var) += (_table)->el_size, _pw_chain_internal.i++)

int pw_chain_table_init(struct pw_chain_table *table, const char *name, struct serializer *serializer, size_t el_size, size_t count);
struct pw_chain_table *pw_chain_table_alloc(const char *name, struct serializer *serializer, size_t el_size, size_t count);
struct pw_chain_table *pw_chain_table_fread(FILE *fp, const char *name, size_t el_count, struct serializer *el_serializer);
void *pw_chain_table_new_el(struct pw_chain_table *table);

size_t serialize_chunked_table_fn(FILE *fp, struct serializer *f, void *data);
size_t deserialize_chunked_table_fn(struct cjson *f, struct serializer *_slzr, void *data);

#endif /* PW_CHAIN_ARR_H */
