/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include "client.h"
#include "service.h"
#include "sub.h"

#include "pmap.h"
#include "util.h"

#include "client_map.h"
#include "service_map.h"
#include "sub_map.h"

#include "db.h"

struct db
{
    struct client_map *clients;
    struct service_map *services;
    struct sub_map *subs;
};

struct db *db_create(void)
{
    struct db *db = ut_malloc(sizeof(struct db));

    *db = (struct db) {
	.clients = client_map_create(),
	.services = service_map_create(),
	.subs = sub_map_create()
    };

    return db;
}

void db_destroy(struct db *db)
{
    if (db != NULL) {
	client_map_destroy(db->clients);
	service_map_destroy(db->services);
	sub_map_destroy(db->subs);

	ut_free(db);
    }	
}

#define GEN_RELAY_FUNS(type)						\
    bool db_has_ ## type(struct db *db, int64_t id)			\
    {									\
	return type ## _map_has_key(db->type ## s, id);			\
    }									\
									\
    struct type *db_get_ ## type(struct db *db, int64_t id)		\
    {									\
	return type ## _map_get(db->type ## s, id);			\
    }									\
    									\
    void db_add_ ## type(struct db *db, int64_t id, struct type *type)	\
    {									\
	ut_assert(id >= 0);						\
	type ## _map_add(db->type ## s, id, type);			\
    }									\
    									\
    void db_del_ ## type(struct db *db, int64_t id)			\
    {									\
	ut_assert(id >= 0);						\
	type ## _map_del(db->type ## s, id);				\
    }									\
    									\
    void db_foreach_ ## type(struct db *db,				\
				 db_foreach_ ## type ## _cb foreach_cb,	\
				 void *foreach_cb_data)			\
    {									\
	type ## _map_foreach(db->type ## s, foreach_cb, foreach_cb_data); \
    }

GEN_RELAY_FUNS(client)
GEN_RELAY_FUNS(service)
GEN_RELAY_FUNS(sub)
