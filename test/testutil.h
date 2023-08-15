/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef TESTUTIL_H
#define TESTUTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

uint64_t tu_rand(void);
uint64_t tu_rand_max(uint64_t max);
bool tu_randbool(void);

void tu_fsleep(double t);

void tu_f_to_timeval(double t, struct timeval *tv);

#endif
