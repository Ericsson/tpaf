/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#include "utest.h"

#include <pvalue.h>

TESTSUITE(value, NULL, NULL)

TESTCASE(value, int64_equal)
{
    struct pvalue *int_17_value = pvalue_int64_create(-17);
    struct pvalue *int_42_0_value = pvalue_int64_create(42);
    struct pvalue *int_42_1_value = pvalue_int64_create(42);

    CHK(pvalue_equal(int_17_value, int_17_value));

    CHK(!pvalue_equal(int_17_value, int_42_0_value));

    CHK(pvalue_equal(int_42_0_value, int_42_1_value));

    pvalue_destroy(int_17_value);
    pvalue_destroy(int_42_0_value);
    pvalue_destroy(int_42_1_value);

    return UTEST_SUCCESS;

}

TESTCASE(value, str_equal)
{
    struct pvalue *str_a_value = pvalue_str_create("a");
    struct pvalue *str_b_0_value = pvalue_str_create("boo");
    struct pvalue *str_b_1_value = pvalue_str_create("boo");

    CHK(pvalue_equal(str_a_value, str_a_value));

    CHK(!pvalue_equal(str_a_value, str_b_0_value));

    CHK(pvalue_equal(str_b_0_value, str_b_1_value));

    pvalue_destroy(str_a_value);
    pvalue_destroy(str_b_0_value);
    pvalue_destroy(str_b_1_value);

    return UTEST_SUCCESS;

}

TESTCASE(value, equal_different_type)
{
    struct pvalue *int_value = pvalue_int64_create(42);
    struct pvalue *str_value = pvalue_str_create("foo");

    CHK(!pvalue_equal(int_value, str_value));

    pvalue_destroy(int_value);
    pvalue_destroy(str_value);

    return UTEST_SUCCESS;
}
