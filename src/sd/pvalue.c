/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#include <string.h>
#include <assert.h>

#include "util.h"

#include <pvalue.h>

enum value_type {
    value_type_int64,
    value_type_str
};

struct pvalue
{
    enum value_type type;
    union {
        int64_t value_int64;
        char *value_str;
    };
};

bool pvalue_is_int64(const struct pvalue *value)
{
    return value->type == value_type_int64;
}

bool pvalue_is_str(const struct pvalue *value)
{
    return value->type == value_type_str;
}
    
static struct pvalue *create_value(enum value_type type)
{
    struct pvalue *value = malloc(sizeof(struct pvalue));

    *value = (struct pvalue) {
        .type = type
    };

    return value;
}

struct pvalue *pvalue_int64_create(int64_t value_int64)
{
    struct pvalue *value = create_value(value_type_int64);
    value->value_int64 = value_int64;
    return value;
}
    
int64_t pvalue_int64(const struct pvalue *value)
{
    assert(pvalue_is_int64(value));

    return value->value_int64;
}

struct pvalue *pvalue_str_create(const char *str)
{
    struct pvalue *value = create_value(value_type_str);
    value->value_str = ut_strdup(str);
    return value;
}

const char *pvalue_str(const struct pvalue *value)
{
    assert(pvalue_is_str(value));

    return value->value_str;
}

bool pvalue_equal(const struct pvalue *a, const struct pvalue *b)
{
    if (a->type != b->type)
        return false;

    switch (a->type) {
    case value_type_int64:
        return a->value_int64 == b->value_int64;
    case value_type_str:
        return strcmp(a->value_str, b->value_str) == 0;
    default:
        assert(0);
    }
}

struct pvalue *pvalue_clone(const struct pvalue *orig)
{
    switch (orig->type) {
    case value_type_int64:
        return pvalue_int64_create(orig->value_int64);
    case value_type_str:
        return pvalue_str_create(orig->value_str);
    default:
        assert(0);
    }
}    

void pvalue_destroy(struct pvalue *value)
{
    if (value != NULL) {
        if (value->type == value_type_str)
            ut_free(value->value_str);
        ut_free(value);
    }
}
