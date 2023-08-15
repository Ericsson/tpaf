/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef SUB_H
#define SUB_H

#include <inttypes.h>
#include <stdbool.h>

#include "filter.h"
#include "service.h"
#include "sub_match.h"

struct sub;

typedef void (*sub_match_cb)(struct sub *sub, const struct service *service,
			     enum sub_match_type match_type,
			     void *cb_data);

struct sub *sub_create(int64_t sub_id, const struct filter *filter,
		       int64_t client_id, sub_match_cb match_cb,
		       void *match_cb_data);

void sub_inc_ref(struct sub *sub);
void sub_dec_ref(struct sub *sub);

void sub_notify(struct sub *sub, enum service_change_type change_type,
		const struct service *service);

int64_t sub_get_sub_id(const struct sub *sub);

const struct filter *sub_get_filter(const struct sub *sub);
char *sub_get_filter_str(const struct sub *sub);

int64_t sub_get_client_id(const struct sub *sub);

#endif
