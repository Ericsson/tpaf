/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <inttypes.h>
#include <stdbool.h>

#include "service.h"
#include "sub.h"

#include "sd_err.h"

struct client;

struct db;

struct client *client_create(int64_t client_id, struct db *db);

void client_inc_ref(struct client *client);
void client_dec_ref(struct client *client);

int64_t client_get_client_id(const struct client *client);

bool client_is_connected(const struct client *client);

double client_get_conn_connected_at(const struct client *client);

const char *client_get_conn_remote_addr(const struct client *client);

bool client_is_stale(const struct client *client);

void client_connect(struct client *client, const char *remote_addr);
int client_reconnect(struct client *client, const char *remote_addr);
int client_disconnect(struct client *client);

int client_publish(struct client *client, int64_t service_id,
		   int64_t generation, const struct props *props,
		   int64_t ttl, service_change_cb change_cb,
		   void *change_cb_data);
int client_unpublish(struct client *client, int64_t service_id);

int client_create_sub(struct client *client, int64_t sub_id,
		      const struct filter *filter, sub_match_cb match_cb,
		      void *match_cb_data);
void client_activate_sub(struct client *client, int64_t sub_id);
int client_unsubscribe(struct client *client, int64_t sub_id);

void client_purge_orphan(struct client *client, int64_t service_id);

typedef bool (*client_foreach_cb)(int64_t sub_id, struct sub *sub,
				  void *cb_data);

void client_foreach_sub(struct client *client, client_foreach_cb foreach_cb,
			void *cb_data);

#endif
