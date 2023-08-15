/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <string.h>

#include "util.h"

#include "sbuf.h"

struct sbuf
{
    char *buf;
    size_t capacity;
    size_t len;
};

struct sbuf *sbuf_create(void)
{
    return ut_calloc(sizeof(struct sbuf));
}

#define CAPACITY_INCREASE_FACTOR (2)

static void assure_capacity(struct sbuf* sbuf, size_t capacity)
{
    if (sbuf->capacity < capacity) {
	sbuf->capacity = CAPACITY_INCREASE_FACTOR * capacity;
	sbuf->buf = ut_realloc(sbuf->buf, sbuf->capacity);
    }
}

char *sbuf_morph(struct sbuf *sbuf)
{
    assure_capacity(sbuf, sbuf->len + 1);

    char *str = sbuf->buf;
    str[sbuf->len] = '\0';

    ut_free(sbuf);

    return str;
}

void sbuf_destroy(struct sbuf *sbuf)
{
    if (sbuf != NULL) {
	ut_free(sbuf->buf);
	ut_free(sbuf);
    }
}

static void append(struct sbuf *sbuf, const char *data, size_t data_len)
{
    size_t new_len = sbuf->len + data_len;

    assure_capacity(sbuf, new_len);

    memcpy(&sbuf->buf[sbuf->len], data, data_len);

    sbuf->len = new_len;
}

void sbuf_append(struct sbuf *sbuf, const char *s)
{
    size_t s_len = strlen(s);

    append(sbuf, s, s_len);
}

void sbuf_append_c(struct sbuf *sbuf, char c)
{
    append(sbuf, &c, 1);
}

