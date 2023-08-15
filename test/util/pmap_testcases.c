/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <limits.h>

#include "utest.h"
#include "testutil.h"

#include "pmap.h"

TESTSUITE(pmap, NULL, NULL)

static bool has_elem(uint64_t *ary, size_t ary_len, uint64_t elem)
{
    size_t i;

    for (i = 0; i < ary_len; i++)
	if (ary[i] == elem)
	    return true;

    return false;
}
    
TESTCASE(pmap, random_basic)
{
    struct pmap *map = pmap_create();

    size_t num_entries = 1000 + tu_rand_max(1000);
    uint64_t keys[num_entries];
    void *values[num_entries];

    size_t i;
    for (i = 0; i < num_entries; i++) {
	do {
	    keys[i] = tu_rand();
	    /* Collisions are going to be incredibly rare, but still */
	} while (has_elem(keys, i, keys[i]));

	values[i] = (void *)tu_rand();

	pmap_add(map, keys[i], values[i]);
    }

    for (i = 0; i < num_entries; i++) {
	CHK(pmap_has_key(map, keys[i]));

    	void *value = pmap_get(map, keys[i]);
	CHK(value == values[i]);
    }

    size_t num_deleted = 0;
    bool deleted[num_entries];
    for (i = 0; i < num_entries; i++) {
	deleted[i] = tu_randbool();

	if (deleted[i]) {
	    pmap_del(map, keys[i]);
	    num_deleted++;
	}
    }

    for (i = 0; i < num_entries; i++) {
	if (deleted[i])
	    CHK(!pmap_has_key(map, keys[i]));
	else {
	    CHK(pmap_has_key(map, keys[i]));

	    void *value = pmap_get(map, keys[i]);
	    CHK(value == values[i]);
	}
    }

    CHKINTEQ(num_entries - num_deleted, pmap_size(map));

    pmap_destroy(map);

    return UTEST_SUCCESS;
}

#define MAX_RECORDED 1024

struct pair
{
    uint64_t key;
    void *value;
};

static bool has_pair(struct pair *pairs, size_t pairs_len,
		     uint64_t key, void *value)
{
    size_t i;
    for (i = 0; i < pairs_len; i++)
	if (pairs[i].key == key && pairs[i].value == value)
	    return true;
    return false;
}

struct record_state
{
    struct pair pairs[MAX_RECORDED];
    size_t num_pairs;
};

static bool record_cb(uint64_t key, void *value, void *cb_data)
{
    struct record_state *state = cb_data;

    struct pair *pair = &state->pairs[state->num_pairs];

    pair->key = key;
    pair->value = value;

    state->num_pairs++;

    return true;
}

TESTCASE(pmap, foreach)
{
    struct pmap *map = pmap_create();

    struct record_state state = {};

    pmap_foreach(map, record_cb, &state);
    CHKINTEQ(state.num_pairs, 0);

    pmap_add(map, 17, (void *)99);
    pmap_add(map, 1, (void *)98);
    pmap_add(map, 42, (void *)97);

    pmap_foreach(map, record_cb, &state);

    CHKINTEQ(pmap_size(map), state.num_pairs);
    CHKINTEQ(state.num_pairs, 3);
    CHK(has_pair(state.pairs, state.num_pairs, 17, (void *)99));
    CHK(has_pair(state.pairs, state.num_pairs, 1, (void *)98));
    CHK(has_pair(state.pairs, state.num_pairs, 42, (void *)97));

    pmap_del(map, 1);

    state.num_pairs = 0;

    pmap_foreach(map, record_cb, &state);

    CHKINTEQ(pmap_size(map), state.num_pairs);
    CHKINTEQ(state.num_pairs, 2);
    CHK(has_pair(state.pairs, state.num_pairs, 17, (void *)99));
    CHK(has_pair(state.pairs, state.num_pairs, 42, (void *)97));

    pmap_del(map, 17);

    state.num_pairs = 0;

    pmap_foreach(map, record_cb, &state);

    CHKINTEQ(pmap_size(map), state.num_pairs);
    CHKINTEQ(state.num_pairs, 1);
    CHK(has_pair(state.pairs, state.num_pairs, 42, (void *)97));

    pmap_destroy(map);

    return UTEST_SUCCESS;
}
