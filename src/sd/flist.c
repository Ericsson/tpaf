/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <string.h>

#include "plist.h"
#include "filter.h"

#include "flist.h"

static void *flist_elem_clone(const void *original)
{
    return filter_clone(original);
}

static void flist_elem_destroy(void *filter)
{
    filter_destroy(filter);
}
	   
static bool flist_elem_equal(const void *a, const void *b)
{
    return filter_equal(a, b);
}

PLIST_GEN_BY_VALUE_WRAPPER_DEF(flist, struct flist, struct filter,
			       flist_elem_clone, flist_elem_destroy,
			       flist_elem_equal, )
