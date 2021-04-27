/* SPDX-License-Identifier: MIT
 * Copyright(c) 2021 Darek Stojaczyk for pwmirage.com
 */

#include <stddef.h>
#include <stdio.h>

#include "avl.h"

struct pw_avl *
pw_avl_init(size_t el_size)
{
	struct pw_avl *avl;

	avl = calloc(1, sizeof(*avl));
	if (!avl) {
		return NULL;
	}

	avl->el_size = el_size;
	return avl;
}

void *
pw_avl_alloc(struct pw_avl *avl)
{
	struct pw_avl_node *node;

	node = calloc(1, sizeof(*node) + avl->el_size);
	if (!node) {
		return NULL;
	}

	node->height = 1;
	return (void *)node->data;
}

static int
height(struct pw_avl_node *node)
{
	return node ? node->height : 0;
}

static int
max(int a, int b)
{
	return a >= b ? a : b;
}

static void
calc_height(struct pw_avl_node *node)
{
	node->height = 1 + max(height(node->left), height(node->right));
}

static struct pw_avl_node *
rotate_right(struct pw_avl_node *root)
{
	struct pw_avl_node *new_root = root->left;
	struct pw_avl_node *moved = new_root->right;

	new_root->right = root;
	root->left = moved;
	calc_height(root);
	calc_height(new_root);

	return new_root;
}

struct pw_avl_node *
rotate_left(struct pw_avl_node *root)
{
	struct pw_avl_node *new_root = root->right;
	struct pw_avl_node *moved = new_root->left;

	new_root->left = root;
	root->right = moved;
	calc_height(root);
	calc_height(new_root);

	return new_root;
}

static struct pw_avl_node *
insert(struct pw_avl_node *parent, struct pw_avl_node *node)
{
	if (!parent) {
		return node;
	}

	if (node->key < parent->key) {
		parent->left  = insert(parent->left, node);
	} else if (node->key > parent->key) {
		parent->right = insert(parent->right, node);
	} else {
		node->next = parent->next;
		parent->next = node;
		/* no changes to the tree, just return */
		return parent;
	}

	calc_height(parent);

	int balance = height(parent->left) - height(parent->right);

	if (balance > 1) {
		if (node->key > parent->left->key) {
			parent->left = rotate_left(parent->left);
		}
		return rotate_right(parent);
	}

	if (balance < -1) {
		if (node->key < parent->right->key) {
			parent->right = rotate_right(parent->right);
		}
		return rotate_left(parent);
	}

	return parent;

}

void
pw_avl_insert(struct pw_avl *avl, unsigned key, void *data)
{
	struct pw_avl_node *node = (void *)(data - offsetof(struct pw_avl_node, data));

	node->key = key;
	avl->root = insert(avl->root, node);
}

void *
pw_avl_get(struct pw_avl *avl, unsigned key)
{
	struct pw_avl_node *node = avl->root;

	while (node && node->key != key) {
		if (key > node->key) {
			node = node->right;
		} else {
			node = node->left;
		}
	}

	return node ? (void *)node->data : NULL;
}

void *
pw_avl_get_next(struct pw_avl *avl, void *data)
{
	struct pw_avl_node *node = (void *)(data - offsetof(struct pw_avl_node, data));

	return node->next ? (void *)node->next->data : NULL;
}

static void
print_node(struct pw_avl_node *node)
{
	fprintf(stderr, "%d\n", node->key);
	if (node->next) {
		fprintf(stderr, "%d next:\n", node->key);
		print_node(node->next);
	}
	if (node->left) {
		fprintf(stderr, "%d left:\n", node->key);
		print_node(node->left);
	}

	if (node->right) {
		fprintf(stderr, "%d right:\n", node->key);
		print_node(node->right);
	}
}

void
pw_avl_print(struct pw_avl *avl)
{
	fprintf(stderr, "pw_avl_print():\n");

	if (avl->root) {
		print_node(avl->root);
	}
}

#ifdef PW_AVL_TEST
int
main(void)
{
	struct pw_avl *avl = pw_avl_init(0);
	int arr[] = { 1, 2, 3, 4, 10, 12, 7, 6, 40, 20, 2, 2, 4 };

	for (int i = 0; i < sizeof(arr) / sizeof(arr[0]); i++) {
		int num = arr[i];

		void *data = pw_avl_alloc(avl);
		pw_avl_insert(avl, num, data);
	}

	pw_avl_print(avl);
	/*       4
	 *      /  \___________,
	 *     2               10
	 *    / \             /  \
	 *   1   3           7   20
	 *                  /   /  \
	 *                 6   12  40
	 */
	return 0;
}
#endif
