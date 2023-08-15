/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "testutil.h"

uint64_t tu_rand(void)
{
    uint64_t r;

    if (getentropy(&r, sizeof(r)) < 0)
	abort();

    return r;
}

uint64_t tu_rand_max(uint64_t max)
{
    return tu_rand() % max;
}

bool tu_randbool()
{
    return tu_rand() & 1;
}

void tu_fsleep(double t)
{
    struct timespec ts;
    ut_f_to_timespec(t, &ts);

    nanosleep(&ts, NULL);
}

void tu_f_to_timeval(double t, struct timeval *tv)
{
    tv->tv_sec = t;
    tv->tv_usec = (t - tv->tv_sec) * 1e6;
}
