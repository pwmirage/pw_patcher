/* SPDX-License-Identifier: MIT
 * Copyright(c) 2019-2021 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_IDMAP_H
#define PW_IDMAP_H

#include <stdint.h>
#include <inttypes.h>

struct pw_idmap;
typedef void (*pw_idmap_async_fn)(void *el, void *ctx);

struct pw_idmap *pw_idmap_init(const char *name, bool clean_load);
long pw_idmap_register_type(struct pw_idmap *map);
void *pw_idmap_get(struct pw_idmap *map, long long lid, long type);
int pw_idmap_get_async(struct pw_idmap *map, long long lid, long type, pw_idmap_async_fn fn, void *fn_ctx);
void pw_idmap_set(struct pw_idmap *map, long long lid, long id, long type, void *data);
void pw_idmap_end_type_load(struct pw_idmap *map, long type, uint32_t max_id);
int pw_idmap_save(struct pw_idmap *map, const char *filename);

#endif /* PW_IDMAP_H */
