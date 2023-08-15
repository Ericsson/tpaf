/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <string.h>

#include "plist.h"
#include "util.h"

#include "slist.h"

static void *slist_elem_clone(const void *original)
{
    return ut_strdup(original);
}

static void slist_elem_destroy(void *original)
{
    ut_free(original);
}
	   
static bool slist_elem_equal(const void *a, const void *b)
{
    return strcmp(a, b) == 0;
}

PLIST_GEN_BY_VALUE_WRAPPER_DEF(slist, struct slist, char, slist_elem_clone,
			       slist_elem_destroy, slist_elem_equal, )
