/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#include "utest.h"

#include <stdbool.h>

#include "testutil.h"

#include "props.h"

TESTSUITE(props, NULL, NULL)

static int assure(const struct props *props_a,
                  const struct props *props_b, bool equal)
{
    bool res_0 = props_equal(props_a, props_b);
    bool res_1 = props_equal(props_b, props_a);

    if (res_0 != res_1)
        return -1;

    if (res_0 != equal)
        return -1;
    return 0;
}

static int assure_equal(const struct props *props_a,
                        const struct props *props_b)

{
    return assure(props_a, props_b, true);
}

static int assure_not_equal(const struct props *props_a,
                            const struct props *props_b)

{
    return assure(props_a, props_b, false);
}

TESTCASE(props, add_get_one)
{
    struct props *props = props_create();
    CHKINTEQ(props_num_values(props), 0);

    props_add_str(props, "name", "foo");
    CHKINTEQ(props_num_values(props), 1);

    const struct pvalue *name_value = props_get_one(props, "name");

    CHK(name_value != NULL);
    CHK(pvalue_is_str(name_value));
    CHKSTREQ(pvalue_str(name_value), "foo");

    props_add_int64(props, "age", 4711);
    props_add_int64(props, "name", -99);
    CHKINTEQ(props_num_values(props), 3);

    name_value = props_get_one(props, "name");

    CHK(name_value != NULL);
    if (pvalue_is_str(name_value))
        CHKSTREQ(pvalue_str(name_value), "foo");
    else
        CHKINTEQ(pvalue_int64(name_value), -99);

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(props, add_get)
{
    struct props *props = props_create();

    props_add_str(props, "value", "bar");
    props_add_str(props, "name", "foo");
    props_add_int64(props, "value", 42);
    CHKINTEQ(props_num_values(props), 3);
    CHKINTEQ(props_num_names(props), 2);

    const struct pvalue *values[16];

    CHKINTEQ(props_get(props, "name", NULL, 0), 1);

    CHKINTEQ(props_get(props, "name", values, 1), 1);
    CHK(pvalue_is_str(values[0]));
    CHK(strcmp(pvalue_str(values[0]), "foo") == 0);

    CHKINTEQ(props_get(props, "value", values, 1), 2);
    if (pvalue_is_str(values[0]))
        CHKSTREQ(pvalue_str(values[0]), "bar");
    else
        CHKINTEQ(pvalue_int64(values[0]), 42);

    memset(values, 0, sizeof(values));
    CHKINTEQ(props_get(props, "value", values, 2), 2);
    CHK((pvalue_is_str(values[0]) && pvalue_is_int64(values[1])) ||
        (pvalue_is_str(values[1]) && pvalue_is_int64(values[0])));

    CHKINTEQ(props_get(props, "value", NULL, 0), 2);

    props_destroy(props);

    return UTEST_SUCCESS;
}

static bool hash_prop(const char *prop_name, const struct pvalue *prop_value,
                      void *user)
{
    int64_t *sum = user;

    (*sum) += strlen(prop_name);
    if (pvalue_is_str(prop_value))
        (*sum) += strlen(pvalue_str(prop_value));
    else
        (*sum) += pvalue_int64(prop_value);

    return true;
}

struct count_param
{
    int max;
    int count;
};

static bool count(const char *prop_name, const struct pvalue *prop_value,
		  void *user)
{
    struct count_param *param = user;

    param->count++;

    return param->count < param->max;
}

TESTCASE(props, foreach)
{
    int64_t actual_sum;
    int64_t expected_sum = 0;
    struct props *props = props_create();

    actual_sum = 0;
    props_foreach(props, hash_prop, &actual_sum);
    CHKINTEQ(actual_sum, expected_sum);

    props_add_int64(props, "foo", 42);
    expected_sum += (strlen("foo") + 42);

    actual_sum = 0;
    props_foreach(props, hash_prop, &actual_sum);
    CHKINTEQ(actual_sum, expected_sum);

    props_add_str(props, "foobar", "kex");
    expected_sum += (strlen("foobar") + strlen("kex"));

    actual_sum = 0;
    props_foreach(props, hash_prop, &actual_sum);
    CHKINTEQ(actual_sum, expected_sum);

    props_add_int64(props, "foo", 99);
    expected_sum += (strlen("foo") + 99);

    actual_sum = 0;
    props_foreach(props, hash_prop, &actual_sum);
    CHKINTEQ(actual_sum, expected_sum);

    struct count_param param = {
	.max = tu_rand_max(3) + 1
    };

    props_foreach(props, count, &param);
    CHKINTEQ(param.count, param.max);

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(props, equal_props_considered_unordered)
{
    struct props *props_0 = props_create();

    props_add_int64(props_0, "name0", 4711);
    props_add_str(props_0, "name1", "foo");

    struct props *props_1 = props_create();

    props_add_str(props_1, "name1", "foo");
    props_add_int64(props_1, "name0", 4711);

    CHKNOERR(assure_equal(props_0, props_1));

    props_destroy(props_0);
    props_destroy(props_1);

    return UTEST_SUCCESS;
}

TESTCASE(props, equal_same_name_different_value)
{
    struct props *props_0 = props_create();
    props_add_int64(props_0, "age", 99);
    props_add_str(props_0, "name", "foo");

    struct props *props_1 = props_create();
    props_add_int64(props_0, "age", 99);
    props_add_int64(props_1, "name", 42);

    CHKNOERR(assure_not_equal(props_0, props_1));

    props_destroy(props_0);
    props_destroy(props_1);

    return UTEST_SUCCESS;
}

TESTCASE(props, equal_different_num)
{
    struct props *props_0 = props_create();
    props_add_str(props_0, "name", "foo");

    struct props *props_1 = props_create();
    props_add_str(props_1, "name", "foo");
    props_add_int64(props_1, "age", 99);

    CHKNOERR(assure_not_equal(props_0, props_1));

    props_destroy(props_0);
    props_destroy(props_1);

    return UTEST_SUCCESS;
}

TESTCASE(props, equal_multivalue_property)
{
    struct props *props_0 = props_create();
    props_add_int64(props_0, "age", 99);
    props_add_int64(props_0, "age", 42);
    props_add_str(props_0, "name", "foo");

    struct props *props_1 = props_create();
    props_add_str(props_1, "name", "foo");
    props_add_int64(props_1, "age", 42);
    props_add_int64(props_1, "age", 99);

    CHKNOERR(assure_equal(props_0, props_1));

    props_destroy(props_0);
    props_destroy(props_1);

    return UTEST_SUCCESS;
}

TESTCASE(props, equal_empty)
{
    struct props *props_0 = props_create();
    struct props *props_1 = props_create();

    CHKNOERR(assure_equal(props_0, props_1));

    props_add_int64(props_1, "name", 4711);

    CHKNOERR(assure_not_equal(props_0, props_1));

    props_destroy(props_0);
    props_destroy(props_1);

    return UTEST_SUCCESS;
}

TESTCASE(props, clone)
{
    struct props *props_orig = props_create();
    props_add_str(props_orig, "name", "foo");
    props_add_int64(props_orig, "name", 4711);
    props_add_int64(props_orig, "value", 42);

    struct props *props_copy = props_clone(props_orig);

    CHKNOERR(assure_equal(props_orig, props_copy));

    props_destroy(props_orig);
    props_destroy(props_copy);

    return UTEST_SUCCESS;
}

