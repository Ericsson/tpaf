/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#ifndef PAF_VALUE_H
#define PAF_VALUE_H

#include <stdbool.h>
#include <stdint.h>

struct pvalue;
bool pvalue_is_int64(const struct pvalue *value);
bool pvalue_is_str(const struct pvalue *value);

struct pvalue *pvalue_int64_create(int64_t value);
int64_t pvalue_int64(const struct pvalue *value);

struct pvalue *pvalue_str_create(const char *value);
const char *pvalue_str(const struct pvalue *value);

bool pvalue_equal(const struct pvalue *value_a,
                     const struct pvalue *value_b);

struct pvalue *pvalue_clone(const struct pvalue *orig);

void pvalue_destroy(struct pvalue *value);

#endif
