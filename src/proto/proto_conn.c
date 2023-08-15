/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <string.h>

#include "client.h"
#include "log.h"
#include "msg.h"
#include "pqueue.h"
#include "pmap.h"
#include "proto_ta.h"
#include "util.h"

#include "proto_conn.h"

PQUEUE_GEN_WRAPPER_DEF(msg_queue, struct msg_queue, struct msg,
		       static __attribute__((unused)))

PMAP_GEN_WRAPPER_DEF(proto_ta_map, struct proto_ta_map, int64_t,
		     struct proto_ta, static __attribute__((unused)))

struct proto_conn
{
    struct xcm_socket *sock;
    struct event sock_event;
    struct sd *sd;
    struct event_base *event_base;
    struct log_ctx *log_ctx;
    proto_conn_cb handshake_cb;
    proto_conn_cb term_cb;
    void *cb_data;

    double established_at;

    int64_t client_id;

    struct proto_ta_map *sub_tas;

    struct msg_queue *out_queue;

    bool term;
};

static bool has_finished_handshake(struct proto_conn *conn)
{
    return conn->client_id >= 0;
}

#define MAX_SEND_BATCH 64
#define MAX_RECEIVE_BATCH 4
#define SOFT_OUT_WIRE_LIMIT 128

static void await_update(struct proto_conn *conn)
{
    int condition = 0;

    if (!conn->term) {
	if (msg_queue_len(conn->out_queue) > 0)
	    condition |= XCM_SO_SENDABLE;

	/* Let client consume responses before accepting more work */
	if (msg_queue_len(conn->out_queue) < SOFT_OUT_WIRE_LIMIT)
	    condition |= XCM_SO_RECEIVABLE;
    }

    xcm_await(conn->sock, condition);
}

static void queue_response(struct proto_conn *conn, struct msg *msg)
{
    msg_queue_push(conn->out_queue, msg);

    await_update(conn);
}

static void handle_hello(struct proto_conn *conn, struct proto_ta *ta)
{
    const int64_t *client_id = proto_ta_get_req_field_uint63_value(ta, 0);
    const int64_t *min_version = proto_ta_get_req_field_uint63_value(ta, 1);
    const int64_t *max_version = proto_ta_get_req_field_uint63_value(ta, 2);

    struct msg *response;

    if (has_finished_handshake(conn)) {
	if (conn->client_id != *client_id) {
	    log_warn_c(conn->log_ctx, "Attempt to change client id denied.");
	    response = proto_ta_fail(ta, PROTO_FAIL_REASON_PERMISSION_DENIED);
	} else {
	    log_debug_c(conn->log_ctx, "Received hello from client with "
			"handshake procedure already successfully completed.");
	    int64_t previously_agreed_version = PROTO_VERSION;
	    response = proto_ta_complete(ta, &previously_agreed_version);
	}

	goto respond;
    }

    if (*min_version > PROTO_VERSION || *max_version < PROTO_VERSION) {
	log_info_c(conn->log_ctx, "Client protocol version range %"PRId64
		   " - %"PRId64" does not include supported server protocol "
		   "version (%"PRId64").", *min_version, *max_version,
		   PROTO_VERSION);
	response =
	    proto_ta_fail(ta, PROTO_FAIL_REASON_UNSUPPORTED_PROTOCOL_VERSION);

	goto respond;
    }

    const char *remote_addr = xcm_remote_addr(conn->sock);

    int rc = sd_client_connect(conn->sd, *client_id, remote_addr);

    if (rc == SD_ERR_CLIENT_ALREADY_EXISTS) {
	log_info_c(conn->log_ctx, "Client %"PRIx64" already exists.",
		   *client_id);
	response = proto_ta_fail(ta, PROTO_FAIL_REASON_CLIENT_ID_EXISTS);

	goto respond;
    }

    ut_assert(rc == 0);

    conn->client_id = *client_id;

    /* all error codes should be handled explicitly */
    ut_assert(rc == 0);

    log_ctx_set_prefix(conn->log_ctx, "<client: %"PRIx64"> ", *client_id);

    log_info_c(conn->log_ctx, "Connected using protocol version %"PRId64".",
	       PROTO_VERSION);

    int64_t selected_version = PROTO_VERSION;

    conn->handshake_cb(conn, conn->cb_data);

    response = proto_ta_complete(ta, &selected_version);

respond:
    queue_response(conn, response);
}

static void notify_sub_match(struct sub *sub, const struct service *service,
			     enum sub_match_type match_type, void *cb_data)
{
    struct proto_conn *conn = cb_data;
    struct proto_ta *sub_ta =
	proto_ta_map_get(conn->sub_tas, sub_get_sub_id(sub));

    int64_t service_id = service_get_id(service);

    struct msg *response;

    if (match_type == sub_match_type_disappeared)
	response = proto_ta_notify(sub_ta, &match_type, &service_id,
				   NULL, NULL, NULL, NULL, NULL);
    else {
	int64_t generation = service_get_generation(service);
	const struct props *props = service_get_props(service);
	int64_t ttl = service_get_ttl(service);
	int64_t client_id = service_get_client_id(service);

	double orphan_since_value;
	const double *orphan_since = NULL;

	if (service_is_orphan(service)) {
	    orphan_since_value = service_get_orphan_since(service);
	    orphan_since = &orphan_since_value;
	}

	response =
	    proto_ta_notify(sub_ta, &match_type, &service_id, &generation,
			    props, &ttl, &client_id, orphan_since);
    }

    queue_response(conn, response);
}

static void handle_subscribe(struct proto_conn *conn, struct proto_ta *ta)
{
    const int64_t *sub_id = proto_ta_get_req_field_uint63_value(ta, 0);
    const char *filter_s = proto_ta_get_opt_req_field_str_value(ta, 0);

    int rc = sd_create_sub(conn->sd, conn->client_id, *sub_id, filter_s,
			   notify_sub_match, conn);

    switch (rc) {
    case SD_ERR_SUB_ALREADY_EXISTS: {
	log_info_c(conn->log_ctx, "Subscription %"PRIx64" already exists.",
		   *sub_id);
	struct msg *response =
	    proto_ta_fail(ta, PROTO_FAIL_REASON_SUBSCRIPTION_ID_EXISTS);
	queue_response(conn, response);
	return;
    }
    case SD_ERR_INVALID_FILTER: {
	log_info_c(conn->log_ctx, "Received subscription request with invalid "
		   "filter \"%s\".", filter_s);
	struct msg *response =
	    proto_ta_fail(ta, PROTO_FAIL_REASON_INVALID_FILTER_SYNTAX);
	queue_response(conn, response);
	return;
    }
    }

    ut_assert(rc == 0);

    if (filter_s != NULL)
	log_debug_c(conn->log_ctx, "Installed subscription %"PRIx64" with "
		    "filter \"%s\".", *sub_id, filter_s);
    else
	log_debug_c(conn->log_ctx, "Installed subscription %"PRIx64".",
		    *sub_id);

    proto_ta_map_add(conn->sub_tas, *sub_id, ta);

    struct msg *response = proto_ta_accept(ta);

    queue_response(conn, response);

    sd_activate_sub(conn->sd, conn->client_id, *sub_id);
}

static void handle_unsubscribe(struct proto_conn *conn,
			       struct proto_ta *unsub_ta)
{
    const int64_t *sub_id = proto_ta_get_req_field_uint63_value(unsub_ta, 0);

    int rc = sd_unsubscribe(conn->sd, conn->client_id, *sub_id);

    struct msg *unsub_response;
    struct msg *sub_response = NULL;

    switch (rc) {
    case SD_ERR_NO_SUCH_SUB:
	log_info_c(conn->log_ctx, "Attempt to unsubscribe to non-existing "
		   "subscription %"PRIx64".", *sub_id);
	unsub_response = proto_ta_fail(unsub_ta,
			       PROTO_FAIL_REASON_NON_EXISTENT_SUBSCRIPTION_ID);
	break;
    case SD_ERR_PERM_DENIED:
	log_info_c(conn->log_ctx, "Permission to unsubscribe subscription "
		   "%"PRIx64" was denied.", *sub_id);
	unsub_response = proto_ta_fail(unsub_ta,
				       PROTO_FAIL_REASON_PERMISSION_DENIED);
	break;
    case 0: {
	log_debug_c(conn->log_ctx, "Unsubscribed subscription %"PRIx64".",
		    *sub_id);

	unsub_response = proto_ta_complete(unsub_ta);

	struct proto_ta *sub_ta = proto_ta_map_get(conn->sub_tas, *sub_id);

	proto_ta_map_del(conn->sub_tas, *sub_id);

	sub_response = proto_ta_complete(sub_ta);

	proto_ta_destroy(sub_ta);

	break;
    }
    default:
	ut_assert(0);
	break;
    }

    queue_response(conn, unsub_response);

    if (sub_response != NULL)
	queue_response(conn, sub_response);
}

static void handle_publish(struct proto_conn *conn, struct proto_ta *ta)
{
    const int64_t *service_id = proto_ta_get_req_field_uint63_value(ta, 0);
    const int64_t *generation = proto_ta_get_req_field_uint63_value(ta, 1);
    const struct props *props = proto_ta_get_req_field_props_value(ta, 2);
    const int64_t *ttl = proto_ta_get_req_field_uint63_value(ta, 3);

    int rc = sd_publish(conn->sd, conn->client_id, *service_id, *generation,
			props, *ttl);

    struct msg *response;

    switch (rc) {
    case SD_ERR_SERVICE_SAME_GENERATION_BUT_DIFFERENT_DATA:
	log_info_c(conn->log_ctx, "Service %"PRIx64" exists but with "
		   "different data.", *service_id);
	response =
	    proto_ta_fail(ta, PROTO_FAIL_REASON_SAME_GENERATION_BUT_DIFFERENT);
	break;
    case SD_ERR_NEWER_SERVICE_GENERATION_EXISTS:
	log_info_c(conn->log_ctx, "Service %"PRIx64" already exists with "
		   "a newer generation.", *service_id);
	response = proto_ta_fail(ta, PROTO_FAIL_REASON_OLD_GENERATION);
	break;
    case 0:
	response = proto_ta_complete(ta);
	break;
    default:
	ut_assert(0);
	break;
    }

    queue_response(conn, response);
}

static void handle_unpublish(struct proto_conn *conn, struct proto_ta *ta)
{
    const int64_t *service_id = proto_ta_get_req_field_uint63_value(ta, 0);

    int rc = sd_unpublish(conn->sd, conn->client_id, *service_id);

    struct msg *response;

    switch (rc) {
    case SD_ERR_NO_SUCH_SERVICE:
	log_info_c(conn->log_ctx, "Attempt to unpublish non-existing service "
		   "%"PRIx64".", *service_id);
	response =
	    proto_ta_fail(ta, PROTO_FAIL_REASON_NON_EXISTENT_SERVICE_ID);
	break;
    case 0:
	response = proto_ta_complete(ta);
	break;
    default:
	ut_assert(0);
	break;
    }

    queue_response(conn, response);
}

static void handle_ping(struct proto_conn *conn, struct proto_ta *ta)
{
    queue_response(conn, proto_ta_complete(ta));
}

struct notify_param
{
    struct proto_conn *conn;
    struct proto_ta *ta;
};

static bool service_notify_cb(int64_t service_id, struct service *service,
			       void *cb_data)
{
    struct notify_param *param = cb_data;

    int64_t generation = service_get_generation(service);
    const struct props *props = service_get_props(service);
    int64_t ttl = service_get_ttl(service);
    int64_t client_id = service_get_client_id(service);

    double orphan_since_value;
    const double *orphan_since = NULL;

    if (service_is_orphan(service)) {
	orphan_since_value = service_get_orphan_since(service);
	orphan_since = &orphan_since_value;
    }

    struct msg *msg = proto_ta_notify(param->ta, &service_id, &generation,
				      props, &ttl, &client_id, orphan_since);

    queue_response(param->conn, msg);

    return true;
}

static void handle_services(struct proto_conn *conn, struct proto_ta *ta)
{
    const char *filter_s = proto_ta_get_opt_req_field_str_value(ta, 0);

    struct filter *filter = NULL;

    if (filter_s != NULL) {
	filter = filter_parse(filter_s);

	if (filter == NULL) {
	    log_info_c(conn->log_ctx, "Received services request with invalid "
		       "filter \"%s\".", filter_s);
	    struct msg *response =
		proto_ta_fail(ta, PROTO_FAIL_REASON_INVALID_FILTER_SYNTAX);
	    queue_response(conn, response);
	    return;
	}
    }

    queue_response(conn, proto_ta_accept(ta));

    struct notify_param param = { .conn = conn, .ta = ta };

    sd_foreach_service(conn->sd, filter, service_notify_cb, &param);

    queue_response(conn, proto_ta_complete(ta));

    filter_destroy(filter);
}

static bool sub_notify_cb(int64_t sub_id, struct sub *sub, void *cb_data)
{
    struct notify_param *param = cb_data;

    int64_t client_id = sub_get_client_id(sub);
    char *filter_s = sub_get_filter_str(sub);

    struct msg *msg =
	proto_ta_notify(param->ta, &sub_id, &client_id, filter_s);

    queue_response(param->conn, msg);

    ut_free(filter_s);

    return true;
}

static void handle_subscriptions(struct proto_conn *conn, struct proto_ta *ta)
{
    queue_response(conn, proto_ta_accept(ta));

    struct notify_param param = { .conn = conn, .ta = ta };

    sd_foreach_sub(conn->sd, sub_notify_cb, &param);

    queue_response(conn, proto_ta_complete(ta));
}

static bool client_notify_cb(int64_t client_id, struct client *client,
			     void *cb_data)
{
    if (client_is_connected(client)) {
	struct notify_param *param = cb_data;
	const char *client_addr = client_get_conn_remote_addr(client);
	int64_t connection_time =
	    (int64_t)client_get_conn_connected_at(client);

	struct msg *msg = proto_ta_notify(param->ta, &client_id, client_addr,
					  &connection_time);

	queue_response(param->conn, msg);
    }
    return true;
}

static void handle_clients(struct proto_conn *conn, struct proto_ta *ta)
{
    queue_response(conn, proto_ta_accept(ta));

    struct notify_param param = { .conn = conn, .ta = ta };

    sd_foreach_client(conn->sd, client_notify_cb, &param);

    queue_response(conn, proto_ta_complete(ta));
}

static void term(struct proto_conn *conn)
{
    ut_assert(!conn->term);

    if (has_finished_handshake(conn))
	sd_client_disconnect(conn->sd, conn->client_id);

    event_del(&conn->sock_event);

    conn->term = true;

    conn->term_cb(conn, conn->cb_data);
}

static int handle_req(struct proto_conn *conn, const struct msg *req_msg)
{
    struct proto_ta *ta = proto_ta_create(conn->log_ctx);

    int rc = proto_ta_req(ta, req_msg);

    if (rc < 0)
	goto err;

    if (strcmp(proto_ta_get_cmd(ta), PROTO_CMD_HELLO) == 0)
	handle_hello(conn, ta);
    else {
	if (!has_finished_handshake(conn)) {
	    log_info_c(conn->log_ctx, "Denied to issue non-hello command "
		       "before finishing handshake.");
	    goto err;
	}

	if (strcmp(proto_ta_get_cmd(ta), PROTO_CMD_SUBSCRIBE) == 0)
	    handle_subscribe(conn, ta);
	else if (strcmp(proto_ta_get_cmd(ta), PROTO_CMD_UNSUBSCRIBE) == 0)
	    handle_unsubscribe(conn, ta);
	else if (strcmp(proto_ta_get_cmd(ta), PROTO_CMD_PUBLISH) == 0)
	    handle_publish(conn, ta);
	else if (strcmp(proto_ta_get_cmd(ta), PROTO_CMD_UNPUBLISH) == 0)
	    handle_unpublish(conn, ta);
	else if (strcmp(proto_ta_get_cmd(ta), PROTO_CMD_PING) == 0)
	    handle_ping(conn, ta);
	else if (strcmp(proto_ta_get_cmd(ta), PROTO_CMD_SERVICES) == 0)
	    handle_services(conn, ta);
	else if (strcmp(proto_ta_get_cmd(ta), PROTO_CMD_SUBSCRIPTIONS) == 0)
	    handle_subscriptions(conn, ta);
	else if (strcmp(proto_ta_get_cmd(ta), PROTO_CMD_CLIENTS) == 0)
	    handle_clients(conn, ta);
	else
	    ut_assert(0);
    }

    if (proto_ta_has_term(ta))
	proto_ta_destroy(ta);

    return 0;

err:
    proto_ta_destroy(ta);

    return -1;
}

static int try_receive(struct proto_conn *conn)
{
    char buf[65536];

    int xcm_rc = xcm_receive(conn->sock, buf, sizeof(buf) - 1);

    if (xcm_rc > 0) {
	/* NUL terminate */
	buf[xcm_rc] = '\0';
	log_debug_c(conn->log_ctx, "Received message: %s", buf);

	struct msg *msg = msg_create(buf, xcm_rc);

	int rc = handle_req(conn, msg);

	msg_destroy(msg);

	if (rc < 0)
	    goto term;

    } else if (xcm_rc < 0) {
	if (errno != EAGAIN) {
	    log_info_c(conn->log_ctx, "Error receiving message on "
		       "socket: %s", strerror(errno));
	    goto term;
	}
    } else {
	log_info_c(conn->log_ctx, "Peer closed connection.");

	goto term;
    }

    await_update(conn);

    return 0;

term:
    term(conn);
    return -1;
}

static int try_send(struct proto_conn *conn)
{
    int i;
    for (i = 0; i < MAX_SEND_BATCH; i++) {
	struct msg *out_msg = msg_queue_peek(conn->out_queue);

	if (out_msg == NULL)
	    return 0;

	int rc = xcm_send(conn->sock, msg_data(out_msg), msg_len(out_msg));

	if (rc < 0) {
	    if (errno == EAGAIN)
		return 0;

	    term(conn);

	    return -1;
	}

	if (log_is_debug_enabled()) {
	    size_t len = msg_len(out_msg);

	    /* NUL terminate */
	    char sdata[len + 1];
	    memcpy(sdata, msg_data(out_msg), len);
	    sdata[len] = '\0';

	    log_debug_c(conn->log_ctx, "Sent message: %s", sdata);
	}

	msg_queue_pop(conn->out_queue);

	msg_destroy(out_msg);
    }

    await_update(conn);

    return 0;
}

static void conn_process(struct proto_conn *conn)
{
    if (try_receive(conn) < 0)
	return;
    if (try_send(conn) < 0)
	return;
}

static void process_cb(int fd, short ev, void *cb_data)
{
    conn_process(cb_data);
}

struct proto_conn *proto_conn_create(struct xcm_socket *conn_sock,
				     struct sd *sd,
				     struct event_base *event_base,
				     const struct log_ctx *log_ctx,
				     proto_conn_cb handshake_cb,
				     proto_conn_cb term_cb,
				     void *cb_data)
{
    struct proto_conn *conn = ut_malloc(sizeof(struct proto_conn));

    *conn = (struct proto_conn) {
	.sock = conn_sock,
	.sd = sd,
	.event_base = event_base,
	.log_ctx = log_ctx_create_prefix(log_ctx, "%s", "<client: ?> "),
	.handshake_cb = handshake_cb,
	.term_cb = term_cb,
	.cb_data = cb_data,
	.established_at = ut_ftime(),
	.client_id = -1,
	.sub_tas = proto_ta_map_create(),
	.out_queue = msg_queue_create()
    };

    int fd = xcm_fd(conn_sock);

    event_assign(&conn->sock_event, conn->event_base, fd, EV_READ|EV_PERSIST,
		 process_cb, conn);

    event_add(&conn->sock_event, NULL);

    xcm_await(conn->sock, XCM_SO_RECEIVABLE);

    return conn;
}

static bool destroy_proto_ta(int64_t sub_id, struct proto_ta *ta,
			     void *cb_data)
{
    proto_ta_destroy(ta);
    return true;
}

void proto_conn_destroy(struct proto_conn *conn)
{
    if (conn != NULL) {
	if (conn->client_id >= 0)
	    log_debug_c(conn->log_ctx, "Tearing down protocol connection for "
			"client %"PRIx64".", conn->client_id);
	else
	    log_debug_c(conn->log_ctx, "Tearing down protocol connection for "
			"unknown client.");

	event_del(&conn->sock_event);

	xcm_close(conn->sock);

	proto_ta_map_foreach(conn->sub_tas, destroy_proto_ta, NULL);
	proto_ta_map_destroy(conn->sub_tas);

	struct msg *msg;
	while ((msg = msg_queue_pop(conn->out_queue)) != NULL)
	    msg_destroy(msg);

	msg_queue_destroy(conn->out_queue);

	log_ctx_destroy(conn->log_ctx);

	ut_free(conn);
    }
}

double proto_client_get_established_at(struct proto_conn *conn)
{
    return conn->established_at;
}

const char *proto_conn_remote_addr(struct proto_conn *conn)
{
    return xcm_remote_addr(conn->sock);
}
