/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <string.h>
#include <xcm.h>

#include "log.h"
#include "plist.h"
#include "proto_conn.h"
#include "sd.h"
#include "util.h"

#include "server.h"

#define CLEAN_OUT_INTERVAL 1.0
#define MAX_HANDSHAKE_TIME 2.0

static bool proto_conn_equal(const void *a, const void *b)
{
    return a == b;
}

PLIST_GEN_BY_REF_WRAPPER_DEF(proto_conn_list, struct proto_conn_list,
			     struct proto_conn,
			     proto_conn_equal, static __attribute__((unused)))

struct server
{
    char *name;

    struct event_base *event_base;

    struct xcm_socket *sock;
    struct event sock_event;

    struct event clean_out_event;

    struct sd *sd;

    bool running;

    struct proto_conn_list *client_conns;
    struct proto_conn_list *clientless_conns;

    struct log_ctx *log_ctx;
};

struct server *server_create(const char *name, const char *server_addr,
			     struct event_base *event_base)
{
    struct log_ctx *log_ctx;

    if (name != NULL)
	log_ctx = log_ctx_create_prefix(NULL, "<domain: %s> ", name);
    else
	log_ctx = log_ctx_create(NULL);

    struct xcm_socket *server_sock = xcm_server(server_addr);

    if (server_sock == NULL) {
	log_error_c(log_ctx, "Error creating server socket \"%s\": %s",
		    server_addr, strerror(errno));

	log_ctx_destroy(log_ctx);

	return NULL;
    }

    struct server *server = ut_malloc(sizeof(struct server));

    *server = (struct server) {
	.name = ut_strdup_non_null(name),
	.event_base = event_base,
	.sock = server_sock,
	.sd = sd_create(event_base),
	.client_conns = proto_conn_list_create(),
	.clientless_conns = proto_conn_list_create(),
	.log_ctx = log_ctx
    };

    log_info_c(log_ctx, "Configured domain bound to \"%s\".", server_addr);

    return server;
}

static bool destroy_conn(struct proto_conn *conn, void *cb_data)
{
    proto_conn_destroy(conn);

    return true;
}

void server_destroy(struct server *server)
{
    if (server != NULL) {
	log_info_c(server->log_ctx, "Tearing down domain server.");

	if (server->running) {
	    event_del(&server->sock_event);
	    event_del(&server->clean_out_event);
	}

	xcm_close(server->sock);

	ut_free(server->name);

	proto_conn_list_foreach(server->client_conns, destroy_conn, NULL);
	proto_conn_list_destroy(server->client_conns);

	proto_conn_list_foreach(server->clientless_conns, destroy_conn, NULL);
	proto_conn_list_destroy(server->clientless_conns);

	sd_destroy(server->sd);

	log_ctx_destroy(server->log_ctx);

	ut_free(server);
    }
}

static void conn_handshake_cb(struct proto_conn *conn, void *cb_data)
{
    struct server *server = cb_data;

    ssize_t idx = proto_conn_list_index_of(server->clientless_conns, conn);
    proto_conn_list_del(server->clientless_conns, idx);

    proto_conn_list_append(server->client_conns, conn);
}

static void conn_term_cb(struct proto_conn *conn, void *cb_data)
{
    struct server *server = cb_data;

    ssize_t idx = proto_conn_list_index_of(server->clientless_conns, conn);

    if (idx >= 0)
	proto_conn_list_del(server->clientless_conns, idx);
    else {
	idx = proto_conn_list_index_of(server->client_conns, conn);
	proto_conn_list_del(server->client_conns, idx);
    }

    proto_conn_destroy(conn);
}

static void accept_cb(int fd, short ev, void *cb_data)
{
    struct server *server = cb_data;

    struct xcm_socket *conn_sock = xcm_accept(server->sock);

    if (conn_sock == NULL)
	return;

    struct proto_conn *conn =
	proto_conn_create(conn_sock, server->sd, server->event_base,
			  server->log_ctx, conn_handshake_cb, conn_term_cb,
			  server);

    if (conn != NULL)
	proto_conn_list_append(server->clientless_conns, conn);

    log_info_c(server->log_ctx, "Accepted new client from \"%s\".",
	       xcm_remote_addr(conn_sock));
}

static bool consider_disconnect(struct proto_conn *clientless_conn,
				void *cb_data)
{
    struct proto_conn_list *expired = cb_data;

    double handshake_time =
	ut_ftime() - proto_client_get_established_at(clientless_conn);

    if (handshake_time > MAX_HANDSHAKE_TIME)
	/* can't modify 'clientless_conns' while iterating */
	proto_conn_list_append(expired, clientless_conn);

    return true;
}

static bool do_disconnect(struct proto_conn *expired_conn, void *cb_data)
{
    struct server *server = cb_data;

    log_info_c(server->log_ctx, "Dropping connection from \"%s\" failing to "
	       "complete the Pathfinder protocol handshake in %.0f s.",
	       proto_conn_remote_addr(expired_conn), MAX_HANDSHAKE_TIME);

    ssize_t idx =
	proto_conn_list_index_of(server->clientless_conns, expired_conn);
    proto_conn_list_del(server->clientless_conns, idx);

    proto_conn_destroy(expired_conn);

    return true;
}

static void clean_out_cb(int fd, short ev, void *cb_data)
{
    struct server *server = cb_data;

    struct proto_conn_list *expired = proto_conn_list_create();

    proto_conn_list_foreach(server->clientless_conns, consider_disconnect,
			    expired);

    proto_conn_list_foreach(expired, do_disconnect, server);

    proto_conn_list_destroy(expired);
}

int server_start(struct server *server)
{
    ut_assert(!server->running);

    if (xcm_set_blocking(server->sock, false) < 0) {
	log_error_c(server->log_ctx, "Unable to set blocking mode on "
		    "XCM server socket: %s.", strerror(errno));
	return -1;
    }

    if (xcm_await(server->sock, XCM_SO_ACCEPTABLE) < 0) {
	log_error_c(server->log_ctx, "Unable to set XCM_SO_ACCEPTABLE on "
		    "XCM server socket: %s.", strerror(errno));
	return -1;
    }

    int fd = xcm_fd(server->sock);

    if (fd < 0) {
	log_error_c(server->log_ctx, "Error retrieving XCM socket fd: %s.",
		    strerror(errno));
	return -1;
    }

    event_assign(&server->sock_event, server->event_base,
		 fd, EV_READ|EV_PERSIST, accept_cb, server);

    event_add(&server->sock_event, NULL);

    event_assign(&server->clean_out_event, server->event_base,
		 -1, EV_PERSIST, clean_out_cb, server);

    struct timeval clean_out_interval;
    ut_f_to_timeval(CLEAN_OUT_INTERVAL, &clean_out_interval);

    event_add(&server->clean_out_event, &clean_out_interval);

    server->running = true;

    log_debug_c(server->log_ctx, "Started serving domain.");

    return 0;
}
