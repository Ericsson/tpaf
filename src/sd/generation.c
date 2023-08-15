/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include "generation.h"

#include <math.h>

#include "util.h"

struct generation
{
    int64_t generation;
    struct props *props;
    int64_t ttl;
    double orphan_since;
    int64_t client_id;
};

struct generation *generation_create(void)
{
    struct generation *generation = ut_malloc(sizeof(struct generation));
    *generation = (struct generation) {
	.generation = -1,
	.props = NULL,
	.ttl = -1,
	.orphan_since = NAN,
	.client_id = -1
    };

    return generation;
}

struct generation *generation_clone(struct generation *original)
{
    struct generation *generation = ut_malloc(sizeof(struct generation));

    *generation = (struct generation) {
	.generation = original->generation,
	.props = props_clone(original->props),
	.ttl = original->ttl,
	.orphan_since = original->orphan_since,
	.client_id = original->client_id
    };

    return generation;
}

bool generation_is_consistent(const struct generation *generation)
{
    return generation->generation >= 0 && generation->props != NULL &&
	generation->ttl >= 0 && !isnan(generation->orphan_since) &&
	generation->client_id >= 0;
}

#define GEN_SIMPLE_SET_RELAY(attr_name, attr_type)			\
    void generation_set_ ## attr_name(struct generation *g,		\
				      attr_type attr_name)		\
    {									\
	g->attr_name = attr_name;					\
    }

#define GEN_SIMPLE_GET_RELAY(attr_name, attr_type)			\
    attr_type generation_get_ ## attr_name(struct generation *g)	\
    {									\
	return g->attr_name;						\
    }

#define GEN_SIMPLE_SET_GET_RELAY(attr_name, attr_type) \
    GEN_SIMPLE_SET_RELAY(attr_name, attr_type) \
    GEN_SIMPLE_GET_RELAY(attr_name, attr_type)

GEN_SIMPLE_SET_GET_RELAY(generation, int64_t)
GEN_SIMPLE_SET_GET_RELAY(ttl, int64_t)
GEN_SIMPLE_SET_GET_RELAY(orphan_since, double)
GEN_SIMPLE_SET_GET_RELAY(client_id, int64_t)

void generation_set_props(struct generation *generation,
			  const struct props *props)
{
    props_destroy(generation->props);
    generation->props = props_clone(props);
}

GEN_SIMPLE_GET_RELAY(props, const struct props *)

void generation_destroy(struct generation *generation)
{
    if (generation != NULL) {
	props_destroy(generation->props);
	ut_free(generation);
    }
}
