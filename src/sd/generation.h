/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef GENERATION_H
#define GENERATION_H

#include <inttypes.h>
#include <stdbool.h>

#include "props.h"

struct generation;

struct generation *generation_create(void);

struct generation *generation_clone(struct generation *original);

void generation_set_generation(struct generation *generation,
			       int64_t number);
void generation_set_props(struct generation *generation,
			  const struct props *props);
void generation_set_ttl(struct generation *generation, int64_t ttl);
void generation_set_orphan_since(struct generation *generation,
				 double orphan_since);
void generation_set_client_id(struct generation *generation,
			      int64_t client_id);

int64_t generation_get_generation(struct generation *generation);
const struct props *generation_get_props(struct generation *generation);
int64_t generation_get_ttl(struct generation *generation);
double generation_get_orphan_since(struct generation *generation);
int64_t generation_get_client_id(struct generation *generation);

bool generation_is_consistent(const struct generation *generation);

void generation_destroy(struct generation *generation);

#endif
