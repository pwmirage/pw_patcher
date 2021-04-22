/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#ifndef PW_AVL_H
#define PW_AVL_H

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

struct pw_avl;

struct pw_avl *pw_avl_init(size_t el_size);
void *pw_avl_alloc(struct pw_avl *avl);
void pw_avl_insert(struct pw_avl *avl, unsigned key, void *data);
void *pw_avl_get(struct pw_avl *avl, unsigned key);
void *pw_avl_get_next(struct pw_avl *avl, void *data);
void pw_avl_print(struct pw_avl *avl);

#endif /* PW_AVL_H */
