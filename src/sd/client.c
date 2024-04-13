/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include "conn.h"
#include "db.h"
#include "plist.h"
#include "util.h"
#include "sub.h"

#include "client.h"

static bool conn_equal(const void *a, const void *b)
{
    return a == b;
}

PLIST_GEN_BY_REF_WRAPPER_DEF(conn_list, struct conn_list, struct conn,
			     conn_equal, static __attribute__((unused)))

struct client
{
    int64_t client_id;
    struct db *db;
    struct conn *active_conn;
    struct conn_list *inactive_conns;

    int ref_cnt;
};

struct client *client_create(int64_t client_id, struct db *db)
{
    struct client *client = ut_malloc(sizeof(struct client));

    *client = (struct client) {
	.client_id = client_id,
	.db = db,
	.inactive_conns = conn_list_create(),
	.ref_cnt = 1
    };

    return client;
}

void client_inc_ref(struct client *client)
{
    ut_assert(client->ref_cnt > 0);

    client->ref_cnt++;
}

static bool destroy_conn(struct conn *conn, void *cb_data)
{
    ut_assert(conn != NULL);
    conn_destroy(conn);
    return true;
}

static void client_conn_foreach(const struct client *client,
				conn_list_foreach_cb foreach_cb,
				void *cb_data)
{
    if (client->active_conn != NULL &&
	!foreach_cb(client->active_conn, cb_data))
	return;

    conn_list_foreach(client->inactive_conns, foreach_cb, cb_data);
}

static void destroy(struct client *client)
{
    client_conn_foreach(client, destroy_conn, NULL);

    conn_list_destroy(client->inactive_conns);

    ut_free(client);
}

void client_dec_ref(struct client *client)
{
    if (client != NULL) {
	ut_assert(client->ref_cnt > 0);

	client->ref_cnt--;

	if (client->ref_cnt == 0)
	    destroy(client);
    }
}

int64_t client_get_client_id(const struct client *client)
{
    return client->client_id;
}

static bool find_non_stale(struct conn *conn, void *cb_data)
{
    bool *is_stale = cb_data;

    if (!conn_is_stale(conn)) {
	*is_stale = false;
	return false;
    }

    return true;
}

bool client_is_connected(const struct client *client)
{
    return client->active_conn != NULL;
}

double client_get_conn_connected_at(const struct client *client)
{
    return conn_get_connected_at(client->active_conn);
}

const char *client_get_conn_remote_addr(const struct client *client)
{
    return conn_get_remote_addr(client->active_conn);
}

bool client_is_stale(const struct client *client)
{
    bool is_stale = true;

    client_conn_foreach(client, find_non_stale, &is_stale);

    return is_stale;
}

void client_connect(struct client *client, const char *remote_addr)
{
    client->active_conn = conn_create(client, remote_addr);

    db_add_client(client->db, client->client_id, client);
}

int client_reconnect(struct client *client, const char *remote_addr)
{
    if (client_is_connected(client))
	return SD_ERR_CLIENT_ALREADY_EXISTS;

    client->active_conn = conn_create(client, remote_addr);

    return 0;
}

static bool sub_handle_conn_inactivation(int64_t sub_id, struct sub *sub,
					 void *cb_data)
{
    struct db *db = cb_data;

    db_del_sub(db, sub_id);

    return true;
}

static bool service_handle_conn_inactivation(int64_t service_id,
					     struct service *service,
					     void *cb_data)
{
    const double *disconnected_at = cb_data;

    service_modify_begin(service);
    service_set_orphan_since(service, *disconnected_at);
    service_commit(service);

    return true;
}

int client_disconnect(struct client *client)
{
    ut_assert(client_is_connected(client));

    struct conn *inactivated = client->active_conn;
    client->active_conn = NULL;

    conn_mark_disconnected(inactivated);

    conn_foreach_sub(inactivated, sub_handle_conn_inactivation, client->db);
    conn_clear_subs(inactivated);

    double disconnected_at = conn_get_disconnected_at(inactivated);
    conn_foreach_service(inactivated, service_handle_conn_inactivation,
			 &disconnected_at);

    if (conn_is_stale(inactivated))
	conn_destroy(inactivated);
    else
	conn_list_append(client->inactive_conns, inactivated);

    if (client_is_stale(client))
	db_del_client(client->db, client->client_id);

    return 0;
}

struct find_service_conn_data
{
    int64_t service_id;
    struct conn *conn;
};

static bool find_service_conn(struct conn *conn, void *cb_data)
{
    struct find_service_conn_data *data = cb_data;

    if (conn_has_service(conn, data->service_id)) {
	data->conn = conn;
	return false;
    }

    return true;
}
    
static struct conn *get_service_conn(const struct client *client,
				     int64_t service_id)
{
    struct find_service_conn_data data = {
	.service_id = service_id,
    };

    client_conn_foreach(client, find_service_conn, &data);

    return data.conn;
}

static void capture_service(struct client *client, struct service *service)
{
    int64_t service_id = service_get_id(service);
    int64_t victim_client_id = service_get_client_id(service);
    struct client *victim_client =
	db_get_client(client->db, victim_client_id);
    struct conn *victim_conn = get_service_conn(victim_client, service_id);

    conn_del_service(victim_conn, service_id);

    conn_add_service(client->active_conn, service_id, service);
}

int client_publish(struct client *client, int64_t service_id,
		   int64_t generation, const struct props *props,
		   int64_t ttl, service_change_cb change_cb,
		   void *change_cb_data)
{
    ut_assert(client_is_connected(client));

    struct service *service = db_get_service(client->db, service_id);

    if (service != NULL) {
	int64_t prev_client_id = service_get_client_id(service);
	bool changed_client_id = prev_client_id != client->client_id;

	if (generation == service_get_generation(service)) {
	    if (!props_equal(props, service_get_props(service)) ||
		ttl != service_get_ttl(service))
		return SD_ERR_SERVICE_SAME_GENERATION_BUT_DIFFERENT_DATA;

	    if (changed_client_id) {
		capture_service(client, service);

		service_modify_begin(service);
		service_set_non_orphan(service);
		service_set_client_id(service, client->client_id);
		service_commit(service);
	    } else if (service_is_orphan(service)) {
		/* old parent is back */
		service_modify_begin(service);
		service_set_non_orphan(service);
		service_commit(service);
	    }
	} else if (generation > service_get_generation(service)) {
	    if (changed_client_id)
		capture_service(client, service);

	    service_modify_begin(service);
	    service_set_generation(service, generation);
	    service_set_props(service, props);
	    service_set_ttl(service, ttl);
	    service_set_non_orphan(service);
	    service_set_client_id(service, client->client_id);
	    service_commit(service);
	} else
	    return SD_ERR_NEWER_SERVICE_GENERATION_EXISTS;
    } else {
	struct service *service =
	    service_create(service_id, change_cb, change_cb_data);

	service_add_begin(service);
	service_set_generation(service, generation);
	service_set_props(service, props);
	service_set_ttl(service, ttl);
	service_set_non_orphan(service);
	service_set_client_id(service, client->client_id);
	service_commit(service);

	db_add_service(client->db, service_id, service);

	conn_add_service(client->active_conn, service_id, service);

	service_dec_ref(service);
    }

    return 0;
}

static void remove_conn(struct client *client, struct conn *conn)
{
    ssize_t idx = conn_list_index_of(client->inactive_conns, conn);
    conn_list_del(client->inactive_conns, idx);

    conn_destroy(conn);
}

static void remove_service(struct client *client, int64_t service_id)
{
    struct service *service = db_get_service(client->db, service_id);
    struct conn *conn = get_service_conn(client, service_id);

    /* Avoid use-after-free in case client and/or service are garbage
       collected */
    service_inc_ref(service);
    client_inc_ref(client);

    conn_del_service(conn, service_id);

    if (conn_is_stale(conn))
	remove_conn(client, conn);

    if (client_is_stale(client))
	db_del_client(client->db, client->client_id);

    db_del_service(client->db, service_id);

    service_remove(service);

    service_dec_ref(service);
    client_dec_ref(client);
}

int client_unpublish(struct client *client, int64_t service_id)
{
    ut_assert(client_is_connected(client));

    struct service *service = db_get_service(client->db, service_id);

    if (service == NULL)
	return SD_ERR_NO_SUCH_SERVICE;

    /* The client doing the unpublishing may not be the owner */
    int64_t owner_id = service_get_client_id(service);

    bool changed_client_id = client->client_id != owner_id;
    bool is_orphan = service_is_orphan(service);

    /* A non-owner unpublish or an unpublish of an orphan service
       implies a republish operation before the actual unpublish, to
       allow the client to notice the difference between an unpublish
       and an orphan timeout. */
    if (changed_client_id || is_orphan) {
	if (changed_client_id)
	    capture_service(client, service);

	service_modify_begin(service);
	service_set_non_orphan(service);

	if (changed_client_id)
	    service_set_client_id(service, client->client_id);
	if (is_orphan)
	    service_set_non_orphan(service);

	service_commit(service);
    }

    remove_service(client, service_id);

    return 0;
}

int client_create_sub(struct client *client, int64_t sub_id,
		      const struct filter *filter, sub_match_cb match_cb,
		      void *match_cb_data)
{
    ut_assert(client_is_connected(client));

    if (db_has_sub(client->db, sub_id))
	return SD_ERR_SUB_ALREADY_EXISTS;

    struct sub *sub =
	sub_create(sub_id, filter, client->client_id, match_cb, match_cb_data);

    conn_add_sub(client->active_conn, sub_id, sub);

    db_add_sub(client->db, sub_id, sub);

    sub_dec_ref(sub);

    return 0;
}

static bool notify_sub_service_added(int64_t service_id,
				     struct service *service,
				     void *cb_data)
{
    struct sub *sub = cb_data;

    sub_notify(sub, service_change_type_added, service);

    return true;
}

void client_activate_sub(struct client *client, int64_t sub_id)
{
    ut_assert(client_is_connected(client));

    struct sub *sub = db_get_sub(client->db, sub_id);

    db_foreach_service(client->db, notify_sub_service_added, sub);
}

int client_unsubscribe(struct client *client, int64_t sub_id)
{
    ut_assert(client_is_connected(client));

    if (!db_has_sub(client->db, sub_id))
	return SD_ERR_NO_SUCH_SUB;

    if (!conn_has_sub(client->active_conn, sub_id))
	/* Subscription exists, but belongs to some other client */
	return SD_ERR_PERM_DENIED;

    conn_del_sub(client->active_conn, sub_id);
    db_del_sub(client->db, sub_id);

    return 0;
}

void client_purge_orphan(struct client *client, int64_t service_id)
{
    ut_assert(!client_is_connected(client));

    struct service *service = db_get_service(client->db, service_id);

    ut_assert(service_get_client_id(service) == client->client_id);

    remove_service(client, service_id);
}

void client_foreach_sub(struct client *client, client_foreach_cb foreach_cb,
			void *cb_data)
{
    if (client->active_conn != NULL)
	conn_foreach_sub(client->active_conn, foreach_cb, cb_data);
}
