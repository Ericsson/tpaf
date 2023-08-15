/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#include "utest.h"

#include "util.h"

#include "filter.h"

TESTSUITE(filter, NULL, NULL)

static int check_filter_str(const char *expected_filter_s,
			    const struct filter *filter)
{
    char *filter_s = filter_str(filter);

    if (strcmp(expected_filter_s, filter_s) != 0)
	return UTEST_FAILED;

    ut_free(filter_s);

    return UTEST_SUCCESS;
}

static int check_valid(const char *s, bool expect_valid)
{
    bool valid = filter_is_valid(s);

    if (expect_valid != valid)
	return UTEST_FAILED;

    struct filter *filter = filter_parse(s);

    if (expect_valid != (filter != NULL))
	return UTEST_FAILED;

    if (filter != NULL && check_filter_str(s, filter) < 0)
	return UTEST_FAILED;

    filter_destroy(filter);

    return UTEST_SUCCESS;
}

static int expect_valid(const char *s)
{
    return check_valid(s, true);
}

static int expect_invalid(const char *s)
{
    return check_valid(s, false);
}

TESTCASE(filter, validate_simple)
{
    CHKNOERR(expect_valid("(foo=xx)"));
    CHKNOERR(expect_invalid("(foo=xx) "));
    CHKNOERR(expect_valid("(foo=xx)"));
    CHKNOERR(expect_valid("(foo=9)"));
    CHKNOERR(expect_valid("(name=)"));

    CHKNOERR(expect_invalid("(=xx)"));
    CHKNOERR(expect_invalid(""));
    CHKNOERR(expect_invalid(" (name=foo)"));
    CHKNOERR(expect_invalid("(name=foo) "));

    return UTEST_SUCCESS;
}

TESTCASE(filter, validate_substring)
{
    CHKNOERR(expect_valid("(foo=*)"));
    CHKNOERR(expect_valid("(foo=foo*bar)"));
    CHKNOERR(expect_valid("(foo=foo*bar*)"));
    CHKNOERR(expect_valid("(foo=*foo*bar*)"));

    CHKNOERR(expect_invalid("(foo=***)"));
    return UTEST_SUCCESS;
}

TESTCASE(filter, validate_comparison)
{
    CHKNOERR(expect_valid("(foo>9)"));
    CHKNOERR(expect_valid("(foo<9)"));
    CHKNOERR(expect_valid("(foo>9342434)"));
    CHKNOERR(expect_valid("(9<9)"));
    CHKNOERR(expect_valid("(bar>-4)"));

    CHKNOERR(expect_invalid("(foo>)"));
    CHKNOERR(expect_invalid("(foo>"));
    CHKNOERR(expect_invalid("(foo> 9)"));
    CHKNOERR(expect_invalid("(foo<9 )"));
    CHKNOERR(expect_invalid("(foo<9a)"));

    return UTEST_SUCCESS;
}

TESTCASE(filter, validate_not)
{
    CHKNOERR(expect_valid("(!(foo>9))"));

    CHKNOERR(expect_invalid("!(name=foo)"));
    CHKNOERR(expect_invalid("(!(name=foo)"));

    return UTEST_SUCCESS;
}

#define TEST_OP(op)                                                \
    CHKNOERR(expect_valid("(" op "(name=foo)(value=99))"));           \
    CHKNOERR(expect_valid("(" op "(name=foo)(value=*)(number>5))")); \
                                                                   \
    CHKNOERR(expect_invalid("(" op "(name=foo))"));                   \
    CHKNOERR(expect_invalid(op "(name=foo))"));                    \

TESTCASE(filter, validate_and)
{
    TEST_OP("&")

    return UTEST_SUCCESS;
}

TESTCASE(filter, validate_or)
{
    TEST_OP("|")

    return UTEST_SUCCESS;
}

static int check_match(const char *filter_s, const struct props *props,
		       bool expect_match)
{
    struct filter *filter = filter_parse(filter_s);

    if (filter == NULL)
	return UTEST_FAILED;

    if (filter != NULL && check_filter_str(filter_s, filter) < 0)
	return UTEST_FAILED;

    if (filter_matches(filter, props) != expect_match)
	return UTEST_FAILED;

    filter_destroy(filter);

    return UTEST_SUCCESS;
}

static int expect_match(const char *s, const struct props *props)
{
    return check_match(s, props, true);
}

static int expect_no_match(const char *s, const struct props *props)
{
    return check_match(s, props, false);
}

TESTCASE(filter, match_equal)
{
    struct props *props = props_create();

    CHKNOERR(expect_no_match("(a=x)", props));

    props_add_str(props, "a", "y");
    CHKNOERR(expect_no_match("(a=x)", props));

    props_add_str(props, "a", "x");
    CHKNOERR(expect_match("(a=x)", props));

    props_add_str(props, "b", "z");
    props_add_int64(props, "c", 42);
    CHKNOERR(expect_match("(a=x)", props));

    CHKNOERR(expect_match("(a=x)", props));

    CHKNOERR(expect_match("(c=42)", props));
    CHKNOERR(expect_no_match("(c=99)", props));

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(filter, match_presence)
{
    struct props *props = props_create();

    CHKNOERR(expect_no_match("(a=x)", props));

    props_add_str(props, "a", "y");
    props_add_str(props, "b", "x0");
    props_add_str(props, "b", "x1");

    CHKNOERR(expect_match("(a=*)", props));
    CHKNOERR(expect_match("(b=*)", props));
    CHKNOERR(expect_no_match("(c=*)", props));

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(filter, match_greater_than)
{
    struct props *props = props_create();

    CHKNOERR(expect_no_match("(a>42)", props));

    props_add_int64(props, "a", 4711);

    CHKNOERR(expect_match("(a>42)", props));
    CHKNOERR(expect_no_match("(a>4711)", props));
    CHKNOERR(expect_no_match("(b>4711)", props));

    props_add_str(props, "a", "x");

    CHKNOERR(expect_match("(a>42)", props));
    CHKNOERR(expect_no_match("(a>4711)", props));

    props_add_int64(props, "b", 17);
    props_add_int64(props, "b", 99);
    props_add_int64(props, "b", 9);
    CHKNOERR(expect_match("(b>42)", props));
    CHKNOERR(expect_no_match("(b>99)", props));

    props_add_int64(props, "c", -42);

    CHKNOERR(expect_match("(a>-99)", props));

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(filter, match_less_than)
{
    struct props *props = props_create();

    CHKNOERR(expect_no_match("(a<42)", props));

    props_add_int64(props, "a", 42);

    CHKNOERR(expect_match("(a<99)", props));
    CHKNOERR(expect_no_match("(a<17)", props));

    props_add_str(props, "a", "x");

    CHKNOERR(expect_match("(a<99)", props));
    CHKNOERR(expect_no_match("(a<17)", props));

    props_add_int64(props, "b", 17);
    props_add_int64(props, "b", 99);
    props_add_int64(props, "b", 9);
    CHKNOERR(expect_match("(b<42)", props));
    CHKNOERR(expect_no_match("(b<9)", props));

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(filter, match_substring)
{
    struct props *props = props_create();

    props_add_str(props, "key", "value");

    CHKNOERR(expect_match("(key=v*e)", props));
    CHKNOERR(expect_no_match("(key=v*u)", props));

    CHKNOERR(expect_match("(key=v*e*)", props));
    CHKNOERR(expect_match("(key=*v*e*)", props));
    CHKNOERR(expect_match("(key=*a*l*)", props));

    CHKNOERR(expect_no_match("(key=v\\*)", props));

    props_add_int64(props, "integer", 42);
    /* XXX: maybe this *should* match? */
    CHKNOERR(expect_no_match("(integer=4*)", props));

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(filter, match_not)
{
    struct props *props = props_create();

    props_add_str(props, "key", "value");

    CHKNOERR(expect_no_match("(!(key=value))", props));
    CHKNOERR(expect_match("(!(key=another_value))", props));

    CHKNOERR(expect_match("(!(!(key=value)))", props));

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(filter, match_multi_value)
{
    struct props *props = props_create();

    props_add_str(props, "key", "value");
    props_add_int64(props, "key", 42);

    CHKNOERR(expect_match("(key=value)", props));
    CHKNOERR(expect_match("(key=42)", props));

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(filter, match_and)
{
    struct props *props = props_create();

    props_add_str(props, "key0", "value0");
    props_add_str(props, "key1", "value1");
    props_add_str(props, "key2", "value2");

    CHKNOERR(expect_match("(&(key0=value0)(key1=*))", props));
    CHKNOERR(expect_no_match("(&(key0=value0)(key3=*))", props));

    CHKNOERR(expect_match("(&(key0=value0)(key1=value1)(key2=value2))",
			  props));
    CHKNOERR(expect_no_match("(&(key0=value0)(key1=value1)(key2=value2)"
			     "(key3=value3))", props));

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(filter, match_or)
{
    struct props *props = props_create();

    props_add_str(props, "key0", "value0");
    props_add_str(props, "key1", "value1");
    props_add_str(props, "key2", "value2");

    CHKNOERR(expect_match("(|(key0=value0)(key1=*))", props));
    CHKNOERR(expect_match("(|(key2=*)(key3=*))", props));
    CHKNOERR(expect_no_match("(|(key3=*)(key4=*))", props));

    CHKNOERR(expect_match("(|(key0=value0)(key0=value0))", props));

    CHKNOERR(expect_match("(|(key1=value1)(key3=value3)(key4=value4)"
			  "(key5=value5))", props));

    props_destroy(props);

    return UTEST_SUCCESS;
}

TESTCASE(filter, match_complex)
{
    struct props *props = props_create();

    const char *filter_s = "(&(key0=value0)(!(|(key1=value1)(key2=value2))))";
    CHKNOERR(expect_no_match(filter_s, props));

    props_add_str(props, "key0", "value0");
    CHKNOERR(expect_match(filter_s, props));
    
    props_add_str(props, "key1", "not-value1");
    CHKNOERR(expect_match(filter_s, props));

    props_del_one(props, "key1");
    props_add_str(props, "key1", "value1");
    CHKNOERR(expect_no_match(filter_s, props));

    props_add_str(props, "key2", "value2");
    CHKNOERR(expect_no_match(filter_s, props));

    props_del_one(props, "key0");
    props_add_str(props, "key0", "not-value0");
    CHKNOERR(expect_no_match(filter_s, props));

    props_destroy(props);

    return UTEST_SUCCESS;
}
