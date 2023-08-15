/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef PROTO_CONN_H
#define PROTO_CONN_H

#include <event.h>
#include <xcm.h>

#include "sd.h"

struct proto_conn;

typedef void (*proto_conn_cb)(struct proto_conn *conn, void *cb_data);

struct proto_conn *proto_conn_create(struct xcm_socket *conn_sock,
				     struct sd *sd,
				     struct event_base *event_base,
				     const struct log_ctx *log_ctx,
				     proto_conn_cb handshake_cb,
				     proto_conn_cb term_cb,
				     void *cb_data);
void proto_conn_destroy(struct proto_conn *conn);

double proto_client_get_established_at(struct proto_conn *conn);

const char *proto_conn_remote_addr(struct proto_conn *conn);

#endif
