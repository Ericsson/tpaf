/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include "sub.h"

#include "util.h"

struct sub
{
    int64_t sub_id;
    struct filter *filter;
    int64_t client_id;

    sub_match_cb match_cb;
    void *match_cb_data;

    int ref_cnt;
};

struct sub *sub_create(int64_t sub_id, const struct filter *filter,
		       int64_t client_id, sub_match_cb match_cb,
		       void *match_cb_data)
{
    struct sub *sub = ut_malloc(sizeof(struct sub));

    *sub = (struct sub) {
	.sub_id = sub_id,
	.filter = filter_clone_non_null(filter),
	.client_id = client_id,
	.match_cb = match_cb,
	.match_cb_data = match_cb_data,
	.ref_cnt = 1
    };

    return sub;
}

void sub_inc_ref(struct sub *sub)
{
    ut_assert(sub->ref_cnt > 0);

    sub->ref_cnt++;
}

static void destroy(struct sub *sub)
{
    filter_destroy(sub->filter);
    ut_free(sub);
}

void sub_dec_ref(struct sub *sub)
{
    if (sub != NULL) {
	ut_assert(sub->ref_cnt > 0);

	sub->ref_cnt--;

	if (sub->ref_cnt == 0)
	    destroy(sub);
    }
}

static bool matches(struct sub *sub, const struct props *props)
{
    return sub->filter != NULL ? filter_matches(sub->filter, props) : true;
}

void sub_notify(struct sub *sub, enum service_change_type change_type,
		const struct service *service)
{
    switch (change_type) {
    case service_change_type_added:
	if (matches(sub, service_get_props(service)))
	    sub->match_cb(sub, service, sub_match_type_appeared,
			  sub->match_cb_data);
	break;
    case service_change_type_modified: {
	const struct props *before = service_get_prev_props(service);
	const struct props *after = service_get_props(service);

	bool matches_before = matches(sub, before);
	bool matches_after = matches(sub, after);

	enum sub_match_type match_type;

	if (!matches_before && !matches_after)
	    break;
	else if (matches_before && matches_after)
	    match_type = sub_match_type_modified;
	else if (!matches_before && matches_after)
	    match_type = sub_match_type_appeared;
	else {
	    ut_assert(matches_before && !matches_after);
	    match_type = sub_match_type_disappeared;
	}

	sub->match_cb(sub, service, match_type, sub->match_cb_data);

	break;
    }
    case service_change_type_removed:
	if (matches(sub, service_get_prev_props(service)))
	    sub->match_cb(sub, service, sub_match_type_disappeared,
			  sub->match_cb_data);
	break;
    default:
	ut_assert(0);
	break;
    }
}

int64_t sub_get_sub_id(const struct sub *sub)
{
    return sub->sub_id;
}

const struct filter *sub_get_filter(const struct sub *sub)
{
    return sub->filter;
}

char *sub_get_filter_str(const struct sub *sub)
{
    return sub->filter != NULL ? filter_str(sub->filter) : NULL;
}

int64_t sub_get_client_id(const struct sub *sub)
{
    return sub->client_id;
}
