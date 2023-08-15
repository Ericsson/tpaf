/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <string.h>

#include "util.h"

#include "plist.h"

struct plist
{
    void **elems;
    size_t capacity;
    size_t len;
};

struct plist *plist_create(void)
{
    return ut_calloc(sizeof(struct plist));
}

struct plist *plist_clone(const struct plist *original)
{
    struct plist *copy = ut_calloc(sizeof(struct plist));

    if (original->len > 0) {
	copy->capacity = original->len;
	copy->len = original->len;
	copy->elems = ut_malloc(sizeof(void *) * original->len);

	memcpy(copy->elems, original->elems, sizeof(void *) * original->len);
    }

    return copy;
}

struct plist *plist_clone_deep(const struct plist *original,
			       plist_elem_clone elem_clone)
{
    struct plist *copy = plist_create();

    size_t i;
    for (i = 0; i < plist_len(original); i++)
	plist_append_deep(copy, elem_clone, plist_get(original, i));

    return copy;
}

struct plist *plist_clone_cnted(const struct plist *original,
				plist_elem_inc_ref elem_inc_ref)
{
    struct plist *copy = plist_create();

    size_t i;
    for (i = 0; i < plist_len(original); i++)
	plist_append_cnted(copy, elem_inc_ref, plist_get(original, i));

    return copy;
}

struct plist *plist_clone_non_null(const struct plist *original)
{
    return original != NULL ? plist_clone(original) : NULL;
}

struct plist *plist_clone_non_null_deep(const struct plist *plist,
					plist_elem_clone elem_clone)
{
    return plist != NULL ? plist_clone_deep(plist, elem_clone) : NULL;
}

struct plist *plist_clone_non_null_cnted(const struct plist *plist,
					 plist_elem_dec_ref elem_dec_ref)
{
    return plist != NULL ? plist_clone_cnted(plist, elem_dec_ref) : NULL;
}

void plist_destroy(struct plist *plist)
{
    if (plist != NULL) {
	ut_free(plist->elems);
	ut_free(plist);
    }
}

void plist_destroy_deep(struct plist *plist, plist_elem_destroy elem_destroy)
{
    if (plist != NULL) {
	size_t i;
	for (i = 0; i < plist_len(plist); i++)
	    elem_destroy(plist_get(plist, i));

	plist_destroy(plist);
    }
}

void plist_destroy_cnted(struct plist *plist, plist_elem_dec_ref elem_dec_ref)
{
    plist_destroy_deep(plist, elem_dec_ref);
}

#define CAPACITY_ASK_FACTOR (2)

static void assure_capacity(struct plist* plist, size_t capacity)
{
    if (plist->capacity < capacity) {
	plist->capacity = CAPACITY_ASK_FACTOR * capacity;
	plist->elems =
	    ut_realloc(plist->elems, sizeof(void *) * plist->capacity);
    }
}

void plist_append(struct plist *plist, void *elem)
{
    size_t new_len = plist->len + 1;

    assure_capacity(plist, new_len);

    plist->elems[plist->len] = elem;

    plist->len = new_len;
}

void plist_append_deep(struct plist *plist, plist_elem_clone elem_clone,
		       const void *elem)
{
    void *list_elem = elem_clone(elem);

    plist_append(plist, list_elem);
}

void plist_append_cnted(struct plist *plist, plist_elem_inc_ref elem_inc_ref,
			void *elem)
{
    elem_inc_ref(elem);
    plist_append(plist, elem);
}

void *plist_get(const struct plist *plist, size_t idx)
{
    ut_assert(idx < plist->len);

    return plist->elems[idx];
}

void plist_del(struct plist *plist, size_t idx)
{
    size_t last_idx = plist->len - 1;

    size_t num_higher = last_idx - idx;

    if (num_higher > 0)
	memmove(&plist->elems[idx], &plist->elems[idx + 1],
		num_higher * sizeof(void *));

    plist->len--;
}

void plist_del_deep(struct plist *plist, plist_elem_destroy elem_destroy,
		    size_t idx)
{
    void *ptr = plist_get(plist, idx);

    plist_del(plist, idx);

    elem_destroy(ptr);
}

void plist_del_cnted(struct plist *plist, plist_elem_dec_ref elem_dec_ref,
		     size_t idx)
{
    plist_del_deep(plist, elem_dec_ref, idx);
}

void plist_clear(struct plist *plist)
{
    if (plist->len > 0) {
	ut_free(plist->elems);
	plist->elems = NULL;
	plist->capacity = 0;
	plist->len = 0;
    }
}

void plist_clear_deep(struct plist *plist, plist_elem_destroy elem_destroy)
{
    size_t i;
    for (i = 0; i < plist->len; i++)
	elem_destroy(plist->elems[i]);

    plist_clear(plist);
}

void plist_clear_cnted(struct plist *plist, plist_elem_dec_ref elem_dec_ref)
{
    plist_clear_deep(plist, elem_dec_ref);
}

size_t plist_len(const struct plist *plist)
{
    return plist->len;
}

bool plist_has(const struct plist *plist, plist_elem_equal elem_equal,
	       const void *elem)
{
    ssize_t idx = plist_index_of(plist, elem_equal, elem);

    return idx < 0 ? false : true;
}

ssize_t plist_index_of(const struct plist *plist, plist_elem_equal elem_equal,
		       const void *elem)
{
    ssize_t i;
    for (i = 0; i < plist->len; i++) {
	const void *list_elem = plist->elems[i];
	if (elem_equal(list_elem, elem))
	    return i;
    }
    return -1;
}

void plist_foreach(const struct plist *plist, plist_foreach_cb cb,
		   void *cb_data)
{
    ssize_t i;
    bool cont;
    for (cont = true, i = 0; cont && i < plist->len; i++) {
	void *list_elem = plist->elems[i];

	cont = cb(list_elem, cb_data);
    }
}
