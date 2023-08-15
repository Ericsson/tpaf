/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "sbuf.h"
#include "slist.h"
#include "flist.h"
#include "util.h"

#include "filter.h"

#define BEGIN_EXPR '('
#define END_EXPR ')'
#define ANY '*'
#define ESCAPE '\\'

#define NOT '!'
#define AND '&'
#define OR '|'
#define EQUAL '='
#define GREATER_THAN '>'
#define LESS_THAN '<'

#define SPECIAL_CHARS "()*\\!&|=<>"

struct filter_ops
{
    struct filter *(*clone)(const struct filter *filter);
    void (*destroy)(struct filter *filter);
    bool (*matches)(const struct filter *filter, const struct props *props);
    void (*str)(const struct filter *filter, struct sbuf *output);
};

struct filter
{
    const struct filter_ops *ops;
};

struct filter *filter_clone(const struct filter *filter)
{
    return filter->ops->clone(filter);
}

struct filter *filter_clone_non_null(const struct filter *filter)
{
    return filter != NULL ? filter_clone(filter) : NULL;
}

void filter_destroy(struct filter *filter)
{
    if (filter != NULL)
	filter->ops->destroy(filter);
}

bool filter_matches(const struct filter *filter,
		    const struct props *props)
{
    return filter->ops->matches(filter, props);
}

char *filter_str(const struct filter *filter)
{
    struct sbuf *output = sbuf_create();

    filter->ops->str(filter, output);

    return sbuf_morph(output);
}

bool filter_equal(const struct filter *filter_a,
		   const struct filter *filter_b)
{
    /* String representatino is canonical. Simple, not very performant. */

    char *a_s = filter_str(filter_a);
    char *b_s = filter_str(filter_b);

    bool equal = strcmp(a_s, b_s) == 0;

    ut_free(a_s);
    ut_free(b_s);

    return equal;
}

struct comparison
{
    struct filter filter;
    char op;
    char *key;
    char *value;
};

static struct filter *comparison_clone(const struct filter *filter);
static void comparison_destroy(struct filter *filter);
static bool comparison_matches(const struct filter *filter,
			       const struct props *props);
void comparison_str(const struct filter *filter, struct sbuf *output);

const static struct filter_ops comparison_ops = {
    .clone = comparison_clone,
    .destroy = comparison_destroy,
    .matches = comparison_matches,
    .str = comparison_str
};

static struct filter *comparison_create(char op, const char *key,
					const char *value)
{
    struct comparison *comparison = ut_malloc(sizeof(struct comparison));

    *comparison = (struct comparison) {
	.filter.ops = &comparison_ops,
	.op = op,
	.key = ut_strdup(key),
	.value = ut_strdup(value)
    };

    return (struct filter *)comparison;
}

static struct filter *comparison_clone(const struct filter *filter)
{
    struct comparison *comparison = (struct comparison *)filter;

    return comparison_create(comparison->op, comparison->key,
			     comparison->value);
}

static void comparison_destroy(struct filter *filter)
{
    if (filter != NULL) {
	struct comparison *comparison = (struct comparison *)filter;

	ut_free(comparison->key);
	ut_free(comparison->value);
	ut_free(comparison);
    }
}

static struct filter *equal_create(const char *key, const char *value)
{
    return comparison_create(EQUAL, key, value);
}

static struct filter *greater_than_create(const char *key, int64_t value)
{
    char value_s[64];
    snprintf(value_s, sizeof(value_s), "%ld", value);

    return comparison_create(GREATER_THAN, key, value_s);
}

static struct filter *less_than_create(const char *key, int64_t value)
{
    char value_s[64];
    snprintf(value_s, sizeof(value_s), "%ld", value);

    return comparison_create(LESS_THAN, key, value_s);
}

struct comparison_search
{
    struct comparison *comparison;
    bool match_found;
};

static bool comparison_matches_cb(const char *prop_name,
				  const struct pvalue *prop_value,
				  void *user)
{
    struct comparison_search *search = user;
    struct comparison *comparison = search->comparison;

    if (strcmp(search->comparison->key, prop_name) != 0)
	return true; /* continue looking */

    if (comparison->op == EQUAL) {
	const char *value_s;
	char int_value_s[64];

	if (pvalue_is_str(prop_value))
	    value_s = pvalue_str(prop_value);
	else {
	    snprintf(int_value_s, sizeof(int_value_s), "%"PRId64,
		     pvalue_int64(prop_value));
	    value_s = int_value_s;
	}

	if (strcmp(value_s, comparison->value) == 0)
	    search->match_found = true;
    } else if (pvalue_is_int64(prop_value)) {
	int64_t value = strtoll(comparison->value, NULL, 10);

	if (comparison->op == GREATER_THAN &&
	    pvalue_int64(prop_value) > value)
	    search->match_found = true;
	else if (comparison->op == LESS_THAN &&
		 pvalue_int64(prop_value) < value)
	    search->match_found = true;
    }

    return !search->match_found;
}

static bool comparison_matches(const struct filter *filter,
			       const struct props *props)
{
    struct comparison *comparison = (struct comparison *)filter;

    struct comparison_search search = {
	.comparison = comparison
    };

    props_foreach(props, comparison_matches_cb, &search);

    return search.match_found;
}

static void append_escaped(struct sbuf *sbuf, const char *s)
{
    char *escaped_s = filter_escape(s);

    sbuf_append(sbuf, escaped_s);

    ut_free(escaped_s);
}

static void append_filter_str(struct sbuf *sbuf, const struct filter *filter)
{
    char *filter_s = filter_str(filter);

    sbuf_append(sbuf, filter_s);

    ut_free(filter_s);
}

void comparison_str(const struct filter *filter, struct sbuf *output)
{
    struct comparison *comparison = (struct comparison *)filter;

    sbuf_append_c(output, BEGIN_EXPR);
    append_escaped(output, comparison->key);
    sbuf_append_c(output, comparison->op);
    append_escaped(output, comparison->value);
    sbuf_append_c(output, END_EXPR);
}

struct present
{
    struct filter filter;
    char *key;
};

static struct filter *present_clone(const struct filter *filter);
static void present_destroy(struct filter *filter);
static bool present_matches(const struct filter *filter,
			    const struct props *props);
static void present_str(const struct filter *filter, struct sbuf *output);

const static struct filter_ops present_ops = {
    .clone = present_clone,
    .destroy = present_destroy,
    .matches = present_matches,
    .str = present_str
};

static struct filter *present_create(const char *key)
{
    struct present *present = ut_malloc(sizeof(struct present));

    *present = (struct present) {
	.filter.ops = &present_ops,
	.key = ut_strdup(key)
    };

    return (struct filter *)present;
}

static struct filter *present_clone(const struct filter *filter)
{
    const struct present *present = (const struct present *)filter;

    return present_create(present->key);
}

static void present_destroy(struct filter *filter)
{
    if (filter != NULL) {
	struct present *present = (struct present *)filter;

	ut_free(present->key);
	ut_free(present);
    }
}

static bool present_matches(const struct filter *filter,
			    const struct props *props)
{
    struct present *present = (struct present *)filter;

    return props_has(props, present->key);
}

static void present_str(const struct filter *filter, struct sbuf *output)
{
    struct present *present = (struct present *)filter;

    sbuf_append_c(output, BEGIN_EXPR);
    append_escaped(output, present->key);
    sbuf_append_c(output, EQUAL);
    sbuf_append_c(output, ANY);
    sbuf_append_c(output, END_EXPR);
}

struct substring
{
    struct filter filter;
    char *key;
    char *initial_value;
    struct slist *intermediate_values;
    char *final_value;
};

static struct filter *substring_clone(const struct filter *filter);
static void substring_destroy(struct filter *filter);
static bool substring_matches(const struct filter *filter,
			      const struct props *props);
static void substring_str(const struct filter *filter, struct sbuf *output);

const static struct filter_ops substring_ops = {
    .clone = substring_clone,
    .destroy = substring_destroy,
    .matches = substring_matches,
    .str = substring_str
};

static struct filter *substring_create(const char *key,
				       const char *initial_value,
				       const struct slist *intermediate_values,
				       const char *final_value)
{
    struct substring *substring = ut_malloc(sizeof(struct substring));

    *substring = (struct substring) {
	.filter.ops = &substring_ops,
	.key = ut_strdup(key),
	.initial_value = ut_strdup_non_null(initial_value),
	.intermediate_values = slist_clone_non_null(intermediate_values),
	.final_value = ut_strdup_non_null(final_value)
    };

    return (struct filter *)substring;
}

static struct filter *substring_clone(const struct filter *filter)
{
    const struct substring *substring = (const struct substring *)filter;

    return substring_create(substring->key, substring->initial_value,
			    substring->intermediate_values,
			    substring->final_value);
}

static void substring_destroy(struct filter *filter)
{
    if (filter != NULL) {
	struct substring *substring = (struct substring *)filter;

	ut_free(substring->key);

	ut_free(substring->initial_value);
	slist_destroy(substring->intermediate_values);
	ut_free(substring->final_value);

	ut_free(substring);
    }
}

struct substring_search
{
    struct substring *substring;
    bool match_found;
};

static bool substring_matches_cb(const char *prop_name,
				 const struct pvalue *prop_value,
				 void *user)
{
    struct substring_search *search = user;

    if (strcmp(prop_name, search->substring->key) != 0)
	return true;

    if (!pvalue_is_str(prop_value))
	return true;

    const char *value = pvalue_str(prop_value);
    const char *initial_value = search->substring->initial_value;

    size_t offset = 0;

    if (initial_value != NULL) {
	size_t initial_len = strlen(initial_value);

	if (strncmp(initial_value, value, initial_len) != 0)
	    return true;

        offset += initial_len;
    }

    const struct slist *intermediate_values =
	search->substring->intermediate_values;

    if (intermediate_values != NULL) {
	size_t i;
	for (i = 0; i < slist_len(intermediate_values); i++) {
	    const char *intermediate_value = slist_get(intermediate_values, i);

	    const char *start = strstr(&value[offset], intermediate_value);

	    if (start == NULL)
		return true;

	    offset = (value - start) + strlen(intermediate_value);
	}
    }

    const char *final_value = search->substring->final_value;

    if (final_value != NULL) {
	size_t final_len = strlen(final_value);
	size_t offset_len = strlen(&value[offset]);

	if (offset_len < final_len)
	    return true;

	offset += (offset_len - final_len);

	if (strcmp(final_value, &value[offset]) != 0)
	    return true;
    }

    search->match_found = true;

    return !search->match_found;
}

static bool substring_matches(const struct filter *filter,
			      const struct props *props)
{
    struct substring *substring = (struct substring *)filter;

    struct substring_search search = {
	.substring = substring
    };

    props_foreach(props, substring_matches_cb, &search);

    return search.match_found;
}

static void substring_str(const struct filter *filter, struct sbuf *output)
{
    struct substring *substring = (struct substring *)filter;

    sbuf_append_c(output, BEGIN_EXPR);
    append_escaped(output, substring->key);
    sbuf_append_c(output, EQUAL);

    if (substring->initial_value != NULL)
	append_escaped(output, substring->initial_value);

    sbuf_append_c(output, ANY);	
	
    if (substring->intermediate_values != NULL) {
	size_t i;
	for (i = 0; i < slist_len(substring->intermediate_values); i++) {
	    const char *intermediate_value =
		slist_get(substring->intermediate_values, i);

	    append_escaped(output, intermediate_value);

	    sbuf_append_c(output, ANY);	
	}
    }

    if (substring->final_value != NULL)
	append_escaped(output, substring->final_value);

    sbuf_append_c(output, END_EXPR);
}

struct not
{
    struct filter filter;
    struct filter *operand;
};

static struct filter *not_clone(const struct filter *filter);
static void not_destroy(struct filter *filter);
static bool not_matches(const struct filter *filter,
			const struct props *props);
static void not_str(const struct filter *filter, struct sbuf *output);

const static struct filter_ops not_ops = {
    .clone = not_clone,
    .destroy = not_destroy,
    .matches = not_matches,
    .str = not_str
};

static struct filter *not_create(const struct filter *operand)
{
    struct not *not = ut_malloc(sizeof(struct not));

    *not = (struct not) {
	.filter.ops = &not_ops,
	.operand = filter_clone(operand)
    };

    return (struct filter *)not;
}

static struct filter *not_clone(const struct filter *filter)
{
    struct not *not = (struct not *)filter;

    return not_create(not->operand);
}

static void not_destroy(struct filter *filter)
{
    if (filter != NULL) {
	struct not *not = (struct not *)filter;

	filter_destroy(not->operand);
	ut_free(not);
    }
}

static bool not_matches(const struct filter *filter,
			const struct props *props)
{
    struct not *not = (struct not *)filter;

    return !filter_matches(not->operand, props);
}

static void not_str(const struct filter *filter, struct sbuf *output)
{
    struct not *not = (struct not *)filter;

    sbuf_append_c(output, BEGIN_EXPR);
    sbuf_append_c(output, NOT);

    append_filter_str(output, not->operand);

    sbuf_append_c(output, END_EXPR);
}

struct composite
{
    struct filter filter;
    char op;
    struct flist *operands;
};

static struct filter *composite_clone(const struct filter *filter);
static void composite_destroy(struct filter *filter);
static bool composite_matches(const struct filter *filter,
			      const struct props *props);
static void composite_str(const struct filter *filter, struct sbuf *output);

const static struct filter_ops composite_ops = {
    .clone = composite_clone,
    .destroy = composite_destroy,
    .matches = composite_matches,
    .str = composite_str
};

static struct filter *composite_create(char op, const struct flist *operands)
{
    struct composite *composite = ut_malloc(sizeof(struct composite));

    *composite = (struct composite) {
	.filter.ops = &composite_ops,
	.op = op,
	.operands = flist_clone(operands)
    };

    return (struct filter *)composite;
}

static struct filter *and_create(const struct flist* operands)
{
    return composite_create(AND, operands);
}

static struct filter *or_create(const struct flist* operands)
{
    return composite_create(OR, operands);
}

static bool and_matches(const struct composite *composite,
			const struct props *props)
{
    size_t i;
    for (i = 0; i < flist_len(composite->operands); i++) {
	const struct filter *operand = flist_get(composite->operands, i);

	if (!filter_matches(operand, props))
	    return false;
    }

    return true;
}

static bool or_matches(const struct composite *composite,
		       const struct props *props)
{
    size_t i;
    for (i = 0; i < flist_len(composite->operands); i++) {
	const struct filter *operand = flist_get(composite->operands, i);

	if (filter_matches(operand, props))
	    return true;
    }

    return false;
}

static struct filter *composite_clone(const struct filter *filter)
{
    struct composite *composite = (struct composite *)filter;

    return composite_create(composite->op, composite->operands);
}

static void composite_destroy(struct filter *filter)
{
    if (filter != NULL) {
	struct composite *composite = (struct composite *)filter;

	flist_destroy(composite->operands);

	ut_free(composite);
    }
}

static bool composite_matches(const struct filter *filter,
			      const struct props *props)
{
    struct composite *composite = (struct composite *)filter;

    if (composite->op == AND)
	return and_matches(composite, props);
    else
	return or_matches(composite, props);
}

static void composite_str(const struct filter *filter, struct sbuf *output)
{
    struct composite *composite = (struct composite *)filter;

    sbuf_append_c(output, BEGIN_EXPR);

    sbuf_append_c(output, composite->op);

    size_t i;
    for (i = 0; i < flist_len(composite->operands); i++)
	append_filter_str(output, flist_get(composite->operands, i));

    sbuf_append_c(output, END_EXPR);
}

struct input
{
    const char* data;
    size_t offset;
};

static int input_current(struct input *input, char *current)
{
    char c = input->data[input->offset];

    if (c == '\0')
        return -1;

    *current = c;

    return 0;
}

static int input_is_current(struct input *input, char expected)
{
    char actual;
    
    if (input_current(input, &actual) < 0)
        return -1;

    return actual == expected;
}

static int input_expect(struct input *input, char expected)
{
    if (input_is_current(input, expected) < 0)
        return -1;

    input->offset++;

    return 0;
}

static int input_skip(struct input *input)
{
    char next = input->data[input->offset];

    if (next == '\0')
        return -1;

    input->offset++;

    return 0;
}

static size_t input_left(struct input *input)
{
    return strlen(&input->data[input->offset]);
}

static bool is_special(char c)
{
    return strchr(SPECIAL_CHARS, c) != NULL;
}

static char *parse_str(struct input *input)
{
    bool escaped = false;
    struct sbuf *result = sbuf_create();

    for (;;) {
	char c;

        if (input_current(input, &c) < 0)
            goto err;

        bool special = is_special(c);

        if (escaped) {
            if (!special)
		goto err;

            if (input_skip(input) < 0)
		goto err;

	    sbuf_append_c(result, c);

            escaped = false;
        } else {
            if (c == ESCAPE)
                escaped = true;
            else if (special)
		return sbuf_morph(result);
	    else
		sbuf_append_c(result, c);

            if (input_skip(input) < 0)
		goto err;
        }
    }

err:
    sbuf_destroy(result);

    return NULL;
}

static int parse_int(struct input *input, int64_t *result)
{
    char *value_s = parse_str(input);
    int rc = -1;

    if (value_s == NULL)
	goto out;

    if (strlen(value_s) == 0)
	goto out;

    if (isspace(value_s[0]))
	goto out;

    char *endptr;
    int64_t value = strtoll(value_s, &endptr, 10);

    if (*endptr != '\0')
	goto out;

    *result = value;
    rc = 0;

out:
    ut_free(value_s);
    return rc;
}

static struct filter *parse(struct input *input);

static struct filter *parse_not(struct input* input)
{
    if (input_expect(input, NOT) < 0)
	return NULL;

    if (input_expect(input, BEGIN_EXPR) < 0)
	return NULL;

    struct filter *operand = parse(input);

    if (operand == NULL)
	return NULL;

    if (input_expect(input, END_EXPR) < 0) {
	filter_destroy(operand);
	return NULL;
    }

    struct filter *not = not_create(operand);

    filter_destroy(operand);

    return not;
}

static struct filter *parse_substring_and_present(struct input *input,
						  const char *key,
						  const char *first_part_value)
{
    struct filter *filter = NULL;
    const char *initial = strlen(first_part_value) > 0 ?
	first_part_value : NULL;
    struct slist *intermediate = NULL;
    char *final = NULL;
    char *next_value = NULL;

    for (;;) {
	ut_free(next_value);

	next_value = parse_str(input);

	if (next_value == NULL)
	    goto out;

	int is_any = input_is_current(input, ANY);

	if (is_any < 0)
	    goto out;

	if (is_any) {
	    if (intermediate == NULL)
		intermediate = slist_create();

	    if (strlen(next_value) == 0)
		goto out;

	    slist_append(intermediate, next_value);

	    input_skip(input);
	} else {
	    if (strlen(next_value) > 0)
		final = ut_strdup(next_value);
	    break;
	}
    }

    if (initial == NULL && intermediate == NULL && final == NULL)
	filter = present_create(key);
    else
	filter = substring_create(key, initial, intermediate, final);

out:
    slist_destroy(intermediate);
    ut_free(final);
    ut_free(next_value);

    return filter;
}

static struct filter *parse_equal(struct input *input, const char *key)
{
    struct filter *filter = NULL;

    if (input_expect(input, EQUAL) < 0)
	goto out;

    char *value = parse_str(input);

    if (value == NULL)
	goto out;

    int is_any = input_is_current(input, ANY);

    if (is_any < 0)
	goto out_free;

    if (is_any) {
	input_skip(input);
	filter = parse_substring_and_present(input, key, value);
    } else
	filter = equal_create(key, value);

out_free:
    ut_free(value);
out:
    return filter;
}

typedef struct filter *(*icmp_create)(const char *key, int64_t value);

static struct filter *parse_greater_and_less_than(struct input *input,
						  const char *key, char op,
			       struct filter *(*create)(const char *, int64_t))
{
    if (input_expect(input, op) < 0)
	return NULL;

    int64_t value;
    if (parse_int(input, &value) < 0)
	return NULL;

    return create(key, value);
}

static struct filter *parse_simple(struct input *input)
{
    struct filter *filter = NULL;

    char *key = parse_str(input);

    if (key == NULL)
	goto out;
    if (strlen(key) == 0)
	goto out_free;

    char c;

    if (input_current(input, &c) < 0)
	goto out_free;

    switch (c) {
    case EQUAL:
        filter = parse_equal(input, key);
	break;
    case GREATER_THAN:
        filter = parse_greater_and_less_than(input, key, GREATER_THAN,
					     greater_than_create);
	break;
    case LESS_THAN:
        filter = parse_greater_and_less_than(input, key, LESS_THAN,
					     less_than_create);
	break;
    }

out_free:
    ut_free(key);
out:
    return filter;
}

static struct filter *parse_composite(struct input *input, char op,
			      struct filter *(*create)(const struct flist *))
{
    input_expect(input, op);

    struct flist *operands = flist_create();

    for (;;) {
        char c;
	if (input_current(input, &c) < 0)
            goto err;
        else if (c == BEGIN_EXPR) {
            if (input_skip(input) < 0)
                goto err;

	    struct filter *operand = parse(input);

	    if (operand == NULL)
		goto err;

	    flist_append(operands, operand);

	    filter_destroy(operand);

	    if (input_expect(input, END_EXPR) < 0)
		goto err;
        } else if (c == END_EXPR) {
            if (flist_len(operands) < 2)
		goto err;

	    struct filter *filter = create(operands);

	    flist_destroy(operands);

	    return filter;
        } else
	    goto err;
    }

err:
    flist_destroy(operands);
    return NULL;
}

static struct filter *parse(struct input *input)
{
    char c;

    if (input_current(input, &c) < 0)
	return NULL;

    switch (c) {
    case AND:
	return parse_composite(input, AND, and_create);
    case OR:
	return parse_composite(input, OR, or_create);
    case NOT:
        return parse_not(input);
    default:
        return parse_simple(input);
    }
}

struct filter *filter_parse(const char *s)
{
    struct filter *filter = NULL;
    struct input input = {
        .data = s
    };

    if (input_expect(&input, BEGIN_EXPR) < 0)
	goto err;

    filter = parse(&input);

    if (filter == NULL)
	goto err;

    if (input_expect(&input, END_EXPR) < 0)
	goto err;

    if (input_left(&input) > 0)
	goto err;

    return filter;

err:
    filter_destroy(filter);
    return NULL;
}

bool filter_is_valid(const char *s)
{
    struct filter *filter = filter_parse(s);

    filter_destroy(filter);

    return filter != NULL;
}

char *filter_escape(const char *s)
{
    struct sbuf *output = sbuf_create();
    size_t i;

    for (i = 0; i < strlen(s); i++) {
        char c = s[i];

        if (is_special(c))
            sbuf_append_c(output, ESCAPE);

	sbuf_append_c(output, c);
    }

    return sbuf_morph(output);
}
