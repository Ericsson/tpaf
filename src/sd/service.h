/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef SERVICE_H
#define SERVICE_H

#include <inttypes.h>
#include <stdbool.h>

#include "props.h"

enum service_change_type {
    service_change_type_none,
    service_change_type_added,
    service_change_type_modified,
    service_change_type_removed
};

struct service;

typedef void (*service_change_cb)(struct service *service,
				  enum service_change_type change_type,
				  void *cb_data);

struct service *service_create(int64_t service_id, service_change_cb change_cb,
			       void *change_cb_data);

void service_inc_ref(struct service *service);
void service_dec_ref(struct service *service);

void service_add_begin(struct service *service);
void service_modify_begin(struct service *service);
void service_commit(struct service *service);

void service_remove(struct service *service);

void service_abort(struct service *service);

void service_set_generation(struct service *service, int64_t generation);
void service_set_props(struct service *service, const struct props *props);
void service_set_ttl(struct service *service, int64_t ttl);
void service_set_orphan_since(struct service *service, double orphan_since);
void service_set_non_orphan(struct service *service);
void service_set_client_id(struct service *service, int64_t client_id);

int64_t service_get_id(const struct service *service);
int64_t service_get_generation(const struct service *service);
const struct props *service_get_props(const struct service *service);
int64_t service_get_ttl(const struct service *service);
double service_get_orphan_since(const struct service *service);
double service_orphan_time_left(const struct service *service);
bool service_is_orphan(const struct service *service);
int64_t service_get_client_id(const struct service *service);

int64_t service_get_prev_generation(const struct service *service);
const struct props *service_get_prev_props(const struct service *service);
int64_t service_get_prev_ttl(const struct service *service);
double service_get_prev_orphan_since(const struct service *service);
bool service_was_orphan(const struct service *service);
int64_t service_get_prev_client_id(const struct service *service);

#endif
