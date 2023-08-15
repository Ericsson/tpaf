/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <inttypes.h>
#include <stdbool.h>

#include "generation.h"
#include "props.h"
#include "util.h"

#include "service.h"

struct service
{
    int64_t service_id;
    service_change_cb change_cb;
    void *change_cb_data;

    enum service_change_type change_in_progress;

    struct generation *current;
    struct generation *prev;
    struct generation *next;

    int ref_cnt;
};


struct service *service_create(int64_t service_id, service_change_cb change_cb,
			       void *change_cb_data)
{
    struct service *service = ut_malloc(sizeof(struct service));

    *service = (struct service) {
	.service_id = service_id,
	.change_cb = change_cb,
	.change_cb_data = change_cb_data,
	.ref_cnt = 1
    };

    return service;
}

void service_inc_ref(struct service *service)
{
    ut_assert(service->ref_cnt > 0);

    service->ref_cnt++;
}

static void destroy(struct service *service)
{
    generation_destroy(service->current);
    generation_destroy(service->prev);
    generation_destroy(service->next);

    ut_free(service);
}

void service_dec_ref(struct service *service)
{
    if (service != NULL) {
	ut_assert(service->ref_cnt > 0);

	service->ref_cnt--;

	if (service->ref_cnt == 0)
	    destroy(service);
    }
}

static bool has_ongoing_change(struct service *service)
{
    switch (service->change_in_progress) {
    case service_change_type_added:
    case service_change_type_modified:
	ut_assert(service->next != NULL);
	return true;
    case service_change_type_none:
	ut_assert(service->next == NULL);
	return false;
    default:
	ut_assert(0);
    }
}

void service_add_begin(struct service *service)
{
    ut_assert(!has_ongoing_change(service));

    service->change_in_progress = service_change_type_added;
    service->next = generation_create();
}

void service_modify_begin(struct service *service)
{
    ut_assert(!has_ongoing_change(service));

    service->change_in_progress = service_change_type_modified;
    service->next = generation_clone(service->current);
}

void service_commit(struct service *service)
{
    ut_assert(has_ongoing_change(service));
    ut_assert(generation_is_consistent(service->next));

    generation_destroy(service->prev);
    service->prev = service->current;
    service->current = service->next;

    enum service_change_type change_type = service->change_in_progress;

    service->change_in_progress = service_change_type_none;
    service->next = NULL;

    service->change_cb(service, change_type, service->change_cb_data);
}

void service_remove(struct service *service)
{
    ut_assert(!has_ongoing_change(service));

    generation_destroy(service->prev);

    service->prev = service->current;
    service->current = NULL;

    service->change_cb(service, service_change_type_removed,
		       service->change_cb_data);
}

void service_abort(struct service *service)
{
    ut_assert(has_ongoing_change(service));

    service->change_in_progress = service_change_type_none;
    generation_destroy(service->next);
    service->next = NULL;
}

int64_t service_get_id(const struct service *service)
{
    return service->service_id;
}

#define GEN_SET_RELAY(attr_name, attr_type)				\
    void service_set_ ## attr_name(struct service *service,		\
				   attr_type attr_name)			\
    {									\
	ut_assert(has_ongoing_change(service));				\
									\
	generation_set_ ## attr_name(service->next, attr_name);		\
    }

#define GEN_GET_RELAY(generation, get_name, attr_name, attr_type)	\
    attr_type service_ ## get_name ## _ ## attr_name(const struct service *service) \
    {									\
	return generation_get_ ## attr_name(service->generation);	\
    }

#define GEN_SET_GET_RELAY(attr_name, attr_type)	\
    GEN_SET_RELAY(attr_name, attr_type) \
    GEN_GET_RELAY(current, get, attr_name, attr_type)	\
    GEN_GET_RELAY(prev, get_prev, attr_name, attr_type)

GEN_SET_GET_RELAY(generation, int64_t)
GEN_SET_GET_RELAY(props, const struct props *)
GEN_SET_GET_RELAY(ttl, int64_t)
GEN_SET_GET_RELAY(orphan_since, double)
GEN_SET_GET_RELAY(client_id, int64_t)
    
void service_set_non_orphan(struct service *service)
{
    service_set_orphan_since(service, -1);
}

bool service_is_orphan(const struct service *service)
{
    return service_get_orphan_since(service) >= 0;
}

double service_orphan_time_left(const struct service *service)
{
    ut_assert(service_is_orphan(service));

    double elapsed = ut_ftime() - service_get_orphan_since(service);
    double left = service_get_ttl(service) - elapsed;

    return left < 0 ? 0 : left;
}

bool service_was_orphan(const struct service *service)
{
    if (service->prev == NULL)
	return false;

    return service_get_prev_orphan_since(service) >= 0;
}
