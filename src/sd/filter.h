/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef FILTER_H
#define FILTER_H

#include <stdbool.h>

#include "props.h"

struct filter *filter_parse(const char *s);

void filter_destroy(struct filter *filter);

struct filter *filter_clone(const struct filter *filter);

struct filter *filter_clone_non_null(const struct filter *filter);

bool filter_matches(const struct filter *filter,
		    const struct props *props);

char *filter_str(const struct filter *filter);

bool filter_equal(const struct filter *filter_a,
		   const struct filter *filter_b);


bool filter_is_valid(const char *s);

char *filter_escape(const char *s);

#endif
