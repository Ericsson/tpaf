/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include "plist.h"
#include "util.h"

#include "pmap.h"

#include <string.h>

struct entry
{
    uint64_t key;
    void *value;
};

static void *elist_elem_clone(const void *original)
{
    struct entry *copy = ut_malloc(sizeof(struct entry));

    memcpy(copy, original, sizeof(struct entry));

    return copy;
}

static bool elist_elem_equal(const void *a, const void *b)
{
    const struct entry *entry_a = a;
    const struct entry *entry_b = b;

    return entry_a->key == entry_b->key;
}

static void elist_elem_destroy(void *elem)
{
    ut_free(elem);
}

PLIST_GEN_BY_VALUE_WRAPPER_DEF(elist, struct elist, struct entry,
			       elist_elem_clone, elist_elem_destroy,
			       elist_elem_equal,
			       static __attribute__((unused)))

struct pmap
{
    struct elist *entries;
};

struct pmap *pmap_create(void)
{
    struct pmap *map = ut_malloc(sizeof(struct pmap));

    *map = (struct pmap) {
	.entries = elist_create()
    };

    return map;
}

void pmap_destroy(struct pmap *map)
{
    if (map != NULL) {
	elist_destroy(map->entries);
	ut_free(map);
    }
}

struct value_dec_ref_cb_data
{
    pmap_value_dec_ref value_dec_ref;
};

static bool value_dec_ref_cb(struct entry *entry, void *cb_data)
{
    struct value_dec_ref_cb_data *data = cb_data;

    data->value_dec_ref(entry->value);

    return true;
}

void pmap_destroy_cnted(struct pmap *map, pmap_value_dec_ref value_dec_ref)
{
    if (map != NULL) {
	pmap_clear_cnted(map, value_dec_ref);
	pmap_destroy(map);
    }
}

void pmap_add(struct pmap *map, uint64_t key, void *value)
{
    ut_assert(!pmap_has_key(map, key));

    struct entry entry = {
	.key = key,
	.value = value
    };

    elist_append(map->entries, &entry);
}

void pmap_add_cnted(struct pmap *map, pmap_value_inc_ref value_inc_ref,
		    uint64_t key, void *value)
{
    value_inc_ref(value);
    pmap_add(map, key, value);
}

static ssize_t index_of(const struct pmap *map, uint64_t key)
{
    struct entry entry = {
	.key = key
    };

    return elist_index_of(map->entries, &entry);
}

bool pmap_has_key(const struct pmap *map, uint64_t key)
{
    struct entry entry = {
	.key = key
    };

    return elist_has(map->entries, &entry);
}

void *pmap_get(const struct pmap *map, uint64_t key)
{
    ssize_t idx = index_of(map, key);

    if (idx < 0)
	return NULL;

    struct entry *entry = elist_get(map->entries, idx);

    return entry->value;
}

void pmap_del(struct pmap *map, uint64_t key)
{
    ssize_t idx = index_of(map, key);
    ut_assert(idx >= 0);

    elist_del(map->entries, idx);
}

void pmap_del_cnted(struct pmap *map, pmap_value_dec_ref value_dec_ref,
		    uint64_t key)
{
    void *value = pmap_get(map, key);

    pmap_del(map, key);

    value_dec_ref(value);
}

void pmap_clear(struct pmap *map)
{
    elist_clear(map->entries);
}

void pmap_clear_cnted(struct pmap *map, pmap_value_dec_ref value_dec_ref)
{
    struct value_dec_ref_cb_data cb_data = {
	.value_dec_ref = value_dec_ref
    };

    elist_foreach(map->entries, value_dec_ref_cb, &cb_data);

    elist_clear(map->entries);
}

size_t pmap_size(const struct pmap *map)
{
    return elist_len(map->entries);
}

struct forward_data
{
    pmap_foreach_cb user_cb;
    void *user_cb_data;
};

static bool forward_cb(struct entry *entry, void *cb_data)
{
    struct forward_data *data = cb_data;

    return data->user_cb(entry->key, entry->value, data->user_cb_data);
}

void pmap_foreach(const struct pmap *pmap, pmap_foreach_cb cb,
		   void *cb_data)
{
    struct forward_data data = {
	.user_cb = cb,
	.user_cb_data = cb_data
    };

    elist_foreach(pmap->entries, forward_cb, &data);
}
