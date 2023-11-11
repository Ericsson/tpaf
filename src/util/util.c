/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "log_ut.h"

#include "util.h"

static void assure_alloc_success(void* ptr)
{
    /* Attempting to handle out-of-memory conditions is generally a
       bad idea (on the process-level). It's a much more robust
       practice to crash. */
    if (ptr == NULL)
	ut_mem_exhausted();
}

void *ut_malloc(size_t size)
{
    void *ptr = malloc(size);
    assure_alloc_success(ptr);
    return ptr;
}

void *ut_realloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    assure_alloc_success(new_ptr);
    return new_ptr;
}

void *ut_calloc(size_t size)
{
    void *ptr = ut_malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

void ut_free(void *ptr)
{
    free(ptr);
}

void *ut_memdup(const void *buf, size_t len)
{
    void *copy = ut_malloc(len);
    memcpy(copy, buf, len);
    return copy;
}

char *ut_strdup(const char *str)
{
    char *copy = strdup(str);
    assure_alloc_success(copy);
    return copy;
}

char *ut_strdup_non_null(const char *str)
{
    return str != NULL ? ut_strdup(str) : NULL;
}

void ut_mem_exhausted(void)
{
    log_ut_mem_exhausted();
    abort();
}

char *ut_asprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    char *str = ut_vasprintf(fmt, ap);

    va_end(ap);

    return str;
}

char *ut_vasprintf(const char *fmt, va_list ap)
{
    char *str;
    int rc = vasprintf(&str, fmt, ap);

    if (rc < 0)
	ut_mem_exhausted();

    return str;
}

void ut_aprintf(char *buf, size_t capacity, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    ut_vaprintf(buf, capacity, format, ap);
    va_end(ap);
}

void ut_vaprintf(char *buf, size_t capacity, const char *format, va_list ap)
{
    size_t len = strlen(buf);

    assert (len < capacity);

    ssize_t left = capacity - len - 1;

    if (left == 0)
        return;

    vsnprintf(buf + len, left, format, ap);
}

void ut_f_to_timeval(double t, struct timeval *tv)
{
    tv->tv_sec = t;
    tv->tv_usec = (t - tv->tv_sec) * 1e6;
}

double ut_timespec_to_f(const struct timespec *ts)
{
    return (double)ts->tv_sec + (double)ts->tv_nsec / 1e9;
}

void ut_f_to_timespec(double t, struct timespec *ts)
{
    ts->tv_sec = t;
    ts->tv_nsec = (t - ts->tv_sec) * 1e9;
}

double ut_ftime(void)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return ut_timespec_to_f(&now);
}

static void entropy(void *buffer, size_t len)
{
    int rc = getentropy(buffer, len);

    if (rc < 0)
	abort();
}

int64_t ut_rand_id(void)
{
    int64_t num;

    entropy(&num, sizeof(num));

    return num >= 0 ? num : -num;
}

double ut_frand(void)
{
    uint64_t num;

    entropy(&num, sizeof(num));

    return (double)num / (double)UINT64_MAX;
}

double ut_frandomize(double d)
{
    double k = ut_frand() + 0.5;

    return k * d;
}

bool ut_str_begins_with(const char *s, char c)
{
    for (;;) {
	char s_c = *s;
	s++;
	if (s_c == '\0')
	    return false;
	if (isspace(s_c))
	    continue;
	if (s_c == c)
	    return true;
    }
}

bool ut_str_ary_has(char * const *ary, size_t ary_len, const char *needle)
{
    size_t i;
    for (i = 0; i < ary_len; i++)
	if (strcmp(ary[i], needle) == 0)
	    return true;
    return false;
}
