#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "cjson_ext.h"

static struct cjson g_null_json = {};

struct cjson_mempool {
	struct cjson_mempool *next;
	unsigned count;
	unsigned capacity;
	struct cjson obj[0];
};

static struct cjson *
new_obj(struct cjson_mempool **mem_p)
{
	struct cjson_mempool *mem = *mem_p;
	size_t num_items = mem->capacity * 2;

	if (mem->count < mem->capacity) {
		return &mem->obj[mem->count++];
	}

	mem = calloc(1, sizeof(*mem) + num_items * sizeof(struct cjson));
	if (!mem) {
		assert(false);
		return NULL;
	}

	mem->capacity = num_items;
	mem->count = 1;

	(*mem_p)->next = mem;
	*mem_p = mem;
	return &mem->obj[0];
}

void
cjson_free(struct cjson *json)
{
	struct cjson_mempool *mem = json->mem;

	while (mem) {
		struct cjson_mempool *next = mem;

		next = mem->next;
		free(mem);
		mem = next;
	}
}

static int
add_child(struct cjson *parent, struct cjson *child)
{
	struct cjson *last = parent->a;

	if (!parent || (parent->type != CJSON_TYPE_OBJECT && parent->type != CJSON_TYPE_ARRAY)) {
		assert(false);
		return -EINVAL;
	}

	if (!last) {
		parent->a = child;
		assert(child->next == NULL);
		child->next = NULL;
		return 0;
	}

	while (last->next) last = last->next;
	last->next = child;
	assert(child->next == NULL);
	child->next = NULL;
	parent->count++;
	return 0;
}

struct cjson *
cjson_parse(char *str)
{
	struct cjson_mempool *mem;
	struct cjson *top_obj; 
	struct cjson *cur_obj;
	char *cur_key = NULL;
	char *b = str;
	bool need_comma = false;

	if (*b != '{' && *b != '[') {
		assert(false);
		return NULL;
	}

	mem = calloc(1, sizeof(*mem) + CJSON_MIN_POOLSIZE * sizeof(struct cjson));
	if (!mem) {
		assert(false);
		return NULL;
	}
	mem->capacity = CJSON_MIN_POOLSIZE;

	top_obj = cur_obj = new_obj(&mem);
	cur_obj->parent = NULL;
	cur_obj->key = "";
	cur_obj->type = *b == '{' ? CJSON_TYPE_OBJECT : CJSON_TYPE_ARRAY;
	cur_obj->mem = mem;

	/* we handled the root object/array separately, go on */
	b++;

	while (*b) {
		switch(*b) {
			case '[':
			case '{': {
				struct cjson *obj;

				need_comma = false;
				if (!cur_key && cur_obj->type != CJSON_TYPE_ARRAY) {
					assert(false);
					goto err;
				}

				obj = new_obj(&mem);
				if (!obj) {
					assert(false);
					goto err;
				}
				obj->parent = cur_obj;
				obj->key = cur_key;
				obj->type = *b == '{' ? CJSON_TYPE_OBJECT : CJSON_TYPE_ARRAY;
				if (add_child(cur_obj, obj) != 0) {
					assert(false);
					goto err;
				}
				cur_obj = obj;
				cur_key = NULL;
				break;
			}
			case ']':
			case '}': {
				need_comma = true;
				if (cur_key || (*b == ']' && cur_obj->type == CJSON_TYPE_OBJECT) ||
				    (*b == '}' && cur_obj->type == CJSON_TYPE_ARRAY)) {
					assert(false);
					goto err;
				}

				cur_obj = cur_obj->parent;
				if (!cur_obj) {
					return top_obj;
				}
				break;
			}
			case '"': {
				char *start = ++b;

				while (*b && *b != '"') b++;
				if (*b == 0 || *(b + 1) == 0) {
					goto err;
				}
				*b++ = 0;

				if (cur_key || cur_obj->type == CJSON_TYPE_ARRAY) {
					struct cjson *obj = new_obj(&mem);

					if (!obj) {
						assert(false);
						goto err;
					}
					obj->parent = cur_obj;
					obj->key = cur_key;
					obj->type = CJSON_TYPE_STRING;
					obj->s = start;
					if (add_child(cur_obj, obj) != 0) {
						assert(false);
						goto err;
					}

					cur_key = NULL;
					need_comma = true;
				} else {
					if (need_comma) {
						assert(false);
						goto err;
					}

					if (cur_obj->type != CJSON_TYPE_OBJECT) {
						assert(false);
						goto err;
					}

					cur_key = start;
				}
				break;
			}
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '-':
			case '.':
			{
				struct cjson *obj;
				char *end;

				if (!cur_key && cur_obj->type != CJSON_TYPE_ARRAY) {
					assert(false);
					goto err;
				}


				obj = new_obj(&mem);
				if (!obj) {
					assert(false);
					goto err;
				}
				obj->parent = cur_obj;
				obj->key = cur_key;

				errno = 0;
				obj->i = strtoll(b, &end, 0);
				if (end == b || errno == ERANGE) {
					assert(false);
					goto err;
				}

				if (*end == '.' || *end == 'e' || *end == 'E') {
					obj->d = strtod(b, &end);
					if (end == b || errno == ERANGE) {
						assert(false);
						goto err;
					}
					obj->type = CJSON_TYPE_FLOAT;
				} else {
					obj->type = CJSON_TYPE_INTEGER;
				}

				if (add_child(cur_obj, obj) != 0) {
					assert(false);
					goto err;
				}

				b = end - 1; /* will be incremented */
				cur_key = NULL;
				break;
			}
			case ',':
				need_comma = false;
				break;
			case ':':
			case ' ':
			default:
				break;
		}
		b++;
	}

	return top_obj;
err:
	cjson_free(top_obj);
	return NULL;
}

struct cjson *
cjson_obj(struct cjson *json, const char *key)
{
	struct cjson *entry = json->a;

	if (json->type == CJSON_TYPE_ARRAY) {
		char *end;
		uint64_t i;

		/* this can't be an address */
		if ((uintptr_t)key < 65536) {
			i = (uintptr_t)key;
		} else {
			errno = 0;
			i = strtoll(key, &end, 0);
			if (end == key || errno == ERANGE) {
				return &g_null_json;
			}
		}

		while (entry) {
			if (i-- == 0) {
				return entry;
			}
			entry = entry->next;
		}

		return &g_null_json;
	}

	while (entry) {
		if (strcmp(entry->key, key) == 0) {
			return entry;
		}
		entry = entry->next;
	}

	return &g_null_json;
}

struct cjson *
cjson_js_ext(size_t argc, ...)
{
	struct cjson *obj;
	va_list ap;
	int i;

	va_start(ap, argc);
	obj = va_arg(ap, struct cjson *);
	for (i = 0; i < argc - 1; i++) {
		const char *key = va_arg(ap, const char *);

		obj = cjson_obj(obj, key);
	}
	va_end(ap);

	return obj;
}
