/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef FLIST_H
#define FLIST_H

#include <stdbool.h>
#include <sys/types.h>

struct filter;
struct flist;

struct flist *flist_create(void);
struct flist *flist_clone(const struct flist *flist);
struct flist *flist_clone_non_null(const struct flist *flist);
void flist_destroy(struct flist *flist);

void flist_append(struct flist *flist, const struct filter *filter);

struct filter *flist_get(const struct flist *flist, size_t index);

void flist_del(struct flist *flist, size_t index);

void flist_clear(struct flist *flist);

size_t flist_len(const struct flist *flist);

bool flist_has(const struct flist *flist, const struct filter *filter);
ssize_t flist_index_of(const struct flist *flist, const struct filter *str);

typedef bool (*flist_foreach_cb)(struct filter *str, void *cb_data);

void flist_foreach(const struct flist *flist, flist_foreach_cb cb,
		   void *cb_data);

#endif
