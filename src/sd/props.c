/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#include <assert.h>
#include <sys/types.h>
#include <string.h>

#include "util.h"

#include <props.h>

struct props
{
    char **names;
    struct pvalue **values;
    size_t num;
};

struct props *props_create(void)
{
    struct props *props = ut_malloc(sizeof(struct props));
    *props = (struct props) { };
    return props;
}

static void add(struct props *props, const char *name, struct pvalue *value)
{
    assert(name != NULL);

    size_t new_idx = props->num;
    props->num++;

    props->names =
        ut_realloc(props->names, sizeof(const char *) * props->num);
    props->values =
        ut_realloc(props->values, sizeof(struct pvalue *) * props->num);

    props->names[new_idx] = ut_strdup(name);
    props->values[new_idx] = value;
}

void props_add(struct props *props, const char *name,
	       const struct pvalue *value)
{
    add(props, name, pvalue_clone(value));
}

void props_add_int64(struct props *props, const char *name,
                         int64_t value)
{
    add(props, name, pvalue_int64_create(value));
}

void props_add_str(struct props *props, const char *name,
                       const char *value)
{
    add(props, name, pvalue_str_create(value));
}

static bool has_pair(const struct props *props, const char *prop_name,
		     const struct pvalue *prop_value)
{
    size_t i;
    for (i = 0; i<props->num; i++)
        if (strcmp(props->names[i], prop_name) == 0 &&
            pvalue_equal(props->values[i], prop_value))
            return true;
    return false;
}

bool props_equal(const struct props *a, const struct props *b)
{
    if (a->num != b->num)
        return false;

    size_t i;
    for (i = 0; i < a->num; i++)
        if (!has_pair(b, a->names[i], a->values[i]))
            return false;
    return true;
}

size_t props_num_values(const struct props *props)
{
    return props->num;
}

size_t props_num_names(const struct props *props)
{
    size_t count = 0;
    size_t i;
    for (i = 0; i < props->num; i++) {
        bool duplicate = false;
        size_t j;
        for (j = 0; j < i; j++)
            if (strcmp(props->names[j], props->names[i]) == 0) {
                duplicate = true;
                break;
            }
        if (!duplicate)
            count++;
    }
    return count;
}

struct props *props_clone(const struct props *orig)
{
    struct props *copy = props_create();

    size_t i;
    for (i = 0; i<orig->num; i++)
        props_add(copy, orig->names[i], orig->values[i]);

    return copy;
}    

size_t props_get(const struct props *props, const char *prop_name,
                     const struct pvalue** values, size_t capacity)
{
    size_t len;
    size_t i;
    for (i = 0, len = 0; i < props->num; i++)
        if (strcmp(props->names[i], prop_name) == 0) {
            if (len < capacity)
                values[len] = props->values[i];
            len++;
        }
    return len;
}

const struct pvalue *props_get_one(const struct props *props,
				   const char *prop_name)
{
    size_t i;
    for (i = 0; i < props->num; i++)
        if (strcmp(props->names[i], prop_name) == 0)
            return props->values[i];
    return NULL;
}

void props_del_one(struct props *props, const char *prop_name)
{
    size_t i;
    for (i = 0; i < props->num; i++)
        if (strcmp(props->names[i], prop_name) == 0) {
	    ut_free(props->names[i]);
            pvalue_destroy(props->values[i]);

	    size_t n = props->num - 1 - i;
	    if (n > 0) {
		memmove(&props->names[i], &props->names[i + 1],
			n * sizeof(char *));
		memmove(&props->values[i], &props->values[i + 1],
			n * sizeof(struct pvalue *));
	    }

	    props->num--;

	    return;
	}
    ut_assert(0);
}

void props_foreach(const struct props *props, props_foreach_cb cb,
		   void *user)
{
    size_t i;
    bool cont;
    for (cont = true, i = 0; cont && i < props->num; i++)
        cont = cb(props->names[i], props->values[i], user);
}

bool props_has(const struct props *props, const char *prop_name)
{
    size_t i;
    for (i = 0; i<props->num; i++)
	if (strcmp(props->names[i], prop_name) == 0)
	    return true;
    return false;
}

void props_destroy(struct props *props)
{
    if (props != NULL) {
        size_t i;
        for (i = 0; i<props->num; i++) {
            ut_free(props->names[i]);
            pvalue_destroy(props->values[i]);
        }
        ut_free(props->names);
        ut_free(props->values);
        ut_free(props);
    }
}
