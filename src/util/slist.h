/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef SLIST_H
#define SLIST_H

#include <stdbool.h>
#include <sys/types.h>

struct slist;

struct slist *slist_create(void);
struct slist *slist_clone(const struct slist *slist);
struct slist *slist_clone_non_null(const struct slist *slist);
void slist_destroy(struct slist *slist);

void slist_append(struct slist *slist, const char *str);

char *slist_get(const struct slist *slist, size_t index);

void slist_del(struct slist *slist, size_t index);

void slist_clear(struct slist *slist);

size_t slist_len(const struct slist *slist);

bool slist_has(const struct slist *slist, const char *str);
ssize_t slist_index_of(const struct slist *slist, const char *str);

typedef bool (*slist_foreach_cb)(char *str, void *cb_data);

void slist_foreach(const struct slist *slist, slist_foreach_cb cb,
		   void *cb_data);

#endif
