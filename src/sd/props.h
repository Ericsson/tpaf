/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#ifndef PROPS_H
#define PROPS_H

#include <stdbool.h>
#include <stddef.h>

#include "pvalue.h"

struct props;

struct props *props_create(void);

void props_add(struct props *props, const char *name,
                   const struct pvalue *value);
void props_add_int64(struct props *props, const char *name, int64_t value);
void props_add_str(struct props *props, const char *name, const char *value);

size_t props_get(const struct props *props, const char *prop_name,
		 const struct pvalue **values, size_t capacity);

const struct pvalue *props_get_one(const struct props *props,
				      const char *prop_name);

void props_del_one(struct props *props, const char *prop_name);

typedef bool (*props_foreach_cb)(const char *prop_name,
				 const struct pvalue *prop_value,
				 void *user);
void props_foreach(const struct props *props, props_foreach_cb cb, void *user);

bool props_has(const struct props *props, const char *prop_name);

bool props_equal(const struct props *props_a, const struct props *props_b);

size_t props_num_values(const struct props *props);
size_t props_num_names(const struct props *props);
struct props *props_clone(const struct props *orig);
void props_destroy(struct props *props);

#endif
