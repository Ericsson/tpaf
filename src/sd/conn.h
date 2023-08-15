/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef CONN_H
#define CONN_H

#include <stdbool.h>
#include <stdint.h>

struct client;
struct service;
struct sub;
struct conn;

struct conn *conn_create(struct client *client, const char *remote_addr);
void conn_destroy(struct conn *conn);

bool conn_is_connected(const struct conn *conn);

double conn_get_connected_at(const struct conn *conn);
double conn_get_disconnected_at(const struct conn *conn);

const char *conn_get_remote_addr(const struct conn *conn);

void conn_mark_disconnected(struct conn *conn);

bool conn_is_stale(struct conn *conn);

void conn_add_service(struct conn *conn, int64_t service_id,
		      struct service *service);
void conn_del_service(struct conn *conn, int64_t service_id);
bool conn_has_service(const struct conn *conn, int64_t service_id);
struct service *conn_get_service(const struct conn *conn, int64_t service_id);

typedef bool (*conn_foreach_service_cb)(int64_t service_id,
					struct service *service,
					void *cb_data);
void conn_foreach_service(const struct conn *conn, conn_foreach_service_cb cb,
			  void *cb_data);

void conn_add_sub(struct conn *conn, int64_t sub_id, struct sub *sub);
void conn_del_sub(struct conn *conn, int64_t sub_id);
bool conn_has_sub(const struct conn *conn, int64_t sub_id);
struct sub *conn_get_sub(const struct conn *conn, int64_t sub_id);

typedef bool (*conn_foreach_sub_cb)(int64_t sub_id, struct sub *sub,
				    void *cb_data);
void conn_foreach_sub(const struct conn *conn, conn_foreach_sub_cb cb,
		      void *cb_data);
void conn_clear_subs(struct conn *conn);

#endif
