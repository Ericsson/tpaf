/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <event.h>

#include "client.h"
#include "db.h"
#include "pmap.h"
#include "util.h"

#include "sd.h"

struct orphan_timer
{
    struct sd *sd;
    struct service *service;
    struct event event;
};

PMAP_GEN_WRAPPER(orphan_map, struct orphan_map, int64_t, struct orphan_timer,
		 static __attribute__((unused)))

struct sd
{
    struct event_base *event_base;
    struct db *db;
    struct orphan_map *orphans;
};

static void orphan_timeout_cb(evutil_socket_t fd, short events, void *cb_data);


/* The conversion to epoll_wait() ms may cause the process to wake up
   a little early, which is a non-issue, except in some very picky
   test cases. */
#define EPOLL_ROUNDING_ERROR_MARGIN (1e-3)

static struct orphan_timer *orphan_timer_create(struct sd *sd,
						struct service *service,
						double timeout)
{
    struct orphan_timer *orphan_timer = ut_malloc(sizeof(struct orphan_timer));

    *orphan_timer = (struct orphan_timer) {
	.sd = sd,
	.service = service
    };

    event_assign(&orphan_timer->event, sd->event_base, -1, 0,
		 orphan_timeout_cb, orphan_timer);

    double time_left = service_orphan_time_left(service) +
	EPOLL_ROUNDING_ERROR_MARGIN;

    struct timeval tv;
    ut_f_to_timeval(time_left, &tv);

    event_add(&orphan_timer->event, &tv);

    service_inc_ref(service);

    return orphan_timer;
}

static void orphan_timer_destroy(struct orphan_timer *orphan_timer)
{
    if (orphan_timer != NULL) {
	event_del(&orphan_timer->event);

	service_dec_ref(orphan_timer->service);

	ut_free(orphan_timer);
    }
}

struct sd *sd_create(struct event_base *event_base)
{
    struct sd *sd = ut_malloc(sizeof(struct sd));

    *sd = (struct sd) {
	.event_base = event_base,
	.db = db_create(),
	.orphans = orphan_map_create()
    };

    return sd;
}


static bool destroy_orphan_timer_cb(int64_t service_id,
				    struct orphan_timer *orphan_timer,
				    void *cb_data)
{
    orphan_timer_destroy(orphan_timer);

    return true;
}

void sd_destroy(struct sd *sd)
{
    if (sd != NULL) {
	orphan_map_foreach(sd->orphans, destroy_orphan_timer_cb, NULL);
	orphan_map_destroy(sd->orphans);

	db_destroy(sd->db);

	ut_free(sd);
    }
}

int sd_client_connect(struct sd *sd, int64_t client_id, const char *remote_addr)
{
    struct client *client = db_get_client(sd->db, client_id);

    if (client == NULL) {
	client = client_create(client_id, sd->db);
	client_connect(client, remote_addr);
	client_dec_ref(client);
	return 0;
    } else
	return client_reconnect(client, remote_addr);
}

int sd_client_disconnect(struct sd *sd, int64_t client_id)
{
    struct client *client = db_get_client(sd->db, client_id);

    if (client == NULL)
	return SD_ERR_NO_SUCH_CLIENT;

    return client_disconnect(client);
}

struct service_change
{
    enum service_change_type change_type;
    const struct service *service;
};

static bool notify_sub_service_changed(int64_t service_id,
				       struct sub *sub,
				       void *cb_data)
{
    struct service_change *change = cb_data;

    sub_notify(sub, change->change_type, change->service);

    return true;
}

static void purge_orphan(struct sd *sd, struct service *service)
{
    struct client *client =
	db_get_client(sd->db, service_get_client_id(service));

    client_purge_orphan(client, service_get_id(service));
}

static void orphan_timeout_cb(evutil_socket_t fd, short events, void *cb_data)
{
    struct orphan_timer *orphan_timer = cb_data;
    struct sd *sd = orphan_timer->sd;
    struct service *service = orphan_timer->service;

    purge_orphan(sd, service);
}

static void add_orphan_timer(struct sd *sd, struct service *service)
{
    double time_left = service_orphan_time_left(service);

    struct orphan_timer *orphan_timer =
	orphan_timer_create(sd, service, time_left);

    orphan_map_add(sd->orphans, service_get_id(service), orphan_timer);
}

static void update_orphan_timer(struct sd *sd, struct service *service)
{
    struct orphan_timer *orphan_timer =
	orphan_map_get(sd->orphans, service_get_id(service));

    event_del(&orphan_timer->event);

    struct timeval timeout;
    ut_f_to_timeval(service_orphan_time_left(service), &timeout);

    event_add(&orphan_timer->event, &timeout);
}

static void remove_orphan_timer(struct sd *sd, struct service *service)
{
    int64_t service_id = service_get_id(service);
    struct orphan_timer *orphan_timer =
	orphan_map_get(sd->orphans, service_id);

    orphan_map_del(sd->orphans, service_id);

    event_del(&orphan_timer->event);

    orphan_timer_destroy(orphan_timer);
}

static void maintain_orphans(struct sd *sd, struct service *service,
			     enum service_change_type change_type)
{
    switch (change_type)
    {
    case service_change_type_added:
	/* Adding an orphan would unusual, but possible */
	if (service_is_orphan(service))
	    add_orphan_timer(sd, service);
	break;
    case service_change_type_modified: {
	bool is_orphan = service_is_orphan(service);
	bool was_orphan = service_was_orphan(service);

	if (was_orphan && !is_orphan)
	    remove_orphan_timer(sd, service);
	else if (!was_orphan && is_orphan)
	    add_orphan_timer(sd, service);
	else if (was_orphan && is_orphan)
	    update_orphan_timer(sd, service);
	break;
    }
    case service_change_type_removed:
	if (service_was_orphan(service))
	    remove_orphan_timer(sd, service);
	break;
    case service_change_type_none:
	ut_assert(0);
    }
}

/* XXX: Move below to client class? It handles added-type
   notifications, and is also guaranteed to live as long as the
   service is alive. */
static void service_changed(struct service *service,
			    enum service_change_type change_type,
			    void *cb_data)
{
    struct sd *sd = cb_data;

    struct service_change change = {
	.change_type = change_type,
	.service = service
    };

    db_foreach_sub(sd->db, notify_sub_service_changed, &change);

    maintain_orphans(sd, service, change_type);
}

int sd_publish(struct sd *sd, int64_t client_id, int64_t service_id,
	       int64_t generation, const struct props *props,
	       int64_t ttl)
{
    struct client *client = db_get_client(sd->db, client_id);

    if (client == NULL)
	return SD_ERR_NO_SUCH_CLIENT;

    return client_publish(client, service_id, generation, props, ttl,
			  service_changed, sd);
}


int sd_unpublish(struct sd *sd, int64_t client_id, int64_t service_id)
{
    struct client *client = db_get_client(sd->db, client_id);

    if (client == NULL)
	return SD_ERR_NO_SUCH_CLIENT;

    return client_unpublish(client, service_id);
}

int sd_create_sub(struct sd *sd, int64_t client_id, int64_t sub_id,
		  const char *filter_s, sub_match_cb match_cb,
		  void *match_cb_data)
{
    struct client *client = db_get_client(sd->db, client_id);

    if (client == NULL)
	return SD_ERR_NO_SUCH_CLIENT;

    struct filter *filter = NULL;

    if (filter_s != NULL) {
	filter = filter_parse(filter_s);

	if (filter == NULL)
	    return SD_ERR_INVALID_FILTER;
    }

    int rc = client_create_sub(client, sub_id, filter, match_cb, match_cb_data);

    filter_destroy(filter);

    return rc;
}

void sd_activate_sub(struct sd *sd, int64_t client_id, int64_t sub_id)
{
    struct client *client = db_get_client(sd->db, client_id);

    client_activate_sub(client, sub_id);
}

int sd_unsubscribe(struct sd *sd, int64_t client_id, int64_t sub_id)
{
    struct client *client = db_get_client(sd->db, client_id);

    if (client == NULL)
	return SD_ERR_NO_SUCH_CLIENT;

    return client_unsubscribe(client, sub_id);
}

void sd_foreach_client(struct sd *sd, sd_foreach_client_cb foreach_cb,
			void *foreach_cb_data)
{
    db_foreach_client(sd->db, foreach_cb, foreach_cb_data);
}

struct forward_if_matches_param
{
    const struct filter *filter;
    sd_foreach_service_cb user_cb;
    void *user_cb_data;
};

static bool forward_if_matches(int64_t service_id, struct service *service,
			       void *cb_data)
{
    struct forward_if_matches_param *param = cb_data;

    bool match;

    if (param->filter == NULL)
	match = true;
    else {
	const struct props *props = service_get_props(service);

	match = filter_matches(param->filter, props);
    }

    if (match)
	param->user_cb(service_id, service, param->user_cb_data);

    return true;
}

void sd_foreach_service(struct sd *sd, const struct filter *filter,
			sd_foreach_service_cb foreach_cb,
			void *foreach_cb_data)
{
    struct forward_if_matches_param param = {
	.filter = filter,
	.user_cb = foreach_cb,
	.user_cb_data = foreach_cb_data
    };

    db_foreach_service(sd->db, forward_if_matches, &param);
}

void sd_foreach_sub(struct sd *sd, sd_foreach_sub_cb foreach_cb,
		    void *foreach_cb_data)
{
    db_foreach_sub(sd->db, foreach_cb, foreach_cb_data);
}
