/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include "sub.h"
#include "util.h"

#include "service_map.h"
#include "sub_map.h"

#include "conn.h"

struct conn
{
    char *remote_addr;
    double connected_at;
    double disconnected_at;

    struct client *client;
    struct service_map *services;
    struct sub_map *subs;
};

struct conn *conn_create(struct client *client, const char *remote_addr)
{
    struct conn *conn = ut_malloc(sizeof(struct conn));

    *conn = (struct conn) {
	.remote_addr = ut_strdup_non_null(remote_addr),
	.connected_at = ut_ftime(),
	.disconnected_at = -1,
	.client = client,
	.services = service_map_create(),
	.subs = sub_map_create()
    };

    return conn;
}

void conn_destroy(struct conn *conn)
{
    if (conn != NULL) {
	ut_free(conn->remote_addr);
	service_map_destroy(conn->services);
	sub_map_destroy(conn->subs);
	ut_free(conn);
    }
}

bool conn_is_connected(const struct conn *conn)
{
    return conn->disconnected_at < 0;
}

double conn_get_connected_at(const struct conn *conn)
{
    return conn->connected_at;
}

double conn_get_disconnected_at(const struct conn *conn)
{
    return conn->disconnected_at;
}

const char *conn_get_remote_addr(const struct conn *conn)
{
    return conn->remote_addr;
}

void conn_mark_disconnected(struct conn *conn)
{
    ut_assert(conn_is_connected(conn));

    conn->disconnected_at = ut_ftime();
}

bool conn_is_stale(struct conn *conn)
{
    return !conn_is_connected(conn) && service_map_size(conn->services) == 0;
}

#define GEN_RELAY_FUNS(type)						\
    void conn_add_ ## type(struct conn *conn, int64_t id, struct type *type) \
    {									\
	type ## _map_add(conn->type ## s, id, type);			\
    }									\
									\
    void conn_del_ ## type(struct conn *conn, int64_t id)		\
    {									\
	type ## _map_del(conn->type ## s, id);				\
    }									\
									\
    bool conn_has_ ## type(const struct conn *conn, int64_t id)		\
    {									\
	return type ## _map_has_key(conn->type ## s, id);		\
    }									\
									\
    struct type *conn_get_ ## type(const struct conn *conn, int64_t id)	\
    {									\
	return type ## _map_get(conn->type ## s, id);			\
    }									\
									\
    typedef bool (*conn_foreach_ ## type ## _cb)(int64_t id,		\
						 struct type *type,	\
						 void *cb_data);	\
									\
    void conn_foreach_ ## type(const struct conn *conn,			\
			       conn_foreach_ ## type ## _cb cb,		\
			       void *cb_data)				\
    {									\
	return type ## _map_foreach(conn->type ## s, cb, cb_data);	\
    }

GEN_RELAY_FUNS(service)
GEN_RELAY_FUNS(sub)

void conn_clear_subs(struct conn *conn)
{
    sub_map_clear(conn->subs);
}
