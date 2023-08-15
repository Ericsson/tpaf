/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef SD_H
#define SD_H

#include <stdbool.h>
#include <stdint.h>
#include <event2/event.h>

#include "client.h"
#include "props.h"
#include "sd_err.h"
#include "service.h"
#include "sub.h"

struct sd;

struct sd *sd_create(struct event_base *event_base);
void sd_destroy(struct sd *sd);

int sd_client_connect(struct sd *sd, int64_t client_id,
		      const char *remote_addr);
int sd_client_disconnect(struct sd *sd, int64_t client_id);

int sd_publish(struct sd *sd, int64_t client_id, int64_t service_id,
	       int64_t generation, const struct props *props,
	       int64_t ttl);

int sd_unpublish(struct sd *sd, int64_t client_id, int64_t service_id);

int sd_create_sub(struct sd *sd, int64_t client_id, int64_t sub_id,
		  const char *filter_s, sub_match_cb match_cb,
		  void *match_cb_data);
void sd_activate_sub(struct sd *sd, int64_t client_id, int64_t sub_id);
int sd_unsubscribe(struct sd *sd, int64_t client_id, int64_t sub_id);

typedef bool (*sd_foreach_client_cb)(int64_t client_id,
				     struct client *client,
				     void *foreach_cb_data);
void sd_foreach_client(struct sd *sd, sd_foreach_client_cb foreach_cb,
			void *foreach_cb_data);

typedef bool (*sd_foreach_service_cb)(int64_t service_id,
				      struct service *service,
				      void *foreach_cb_data);
void sd_foreach_service(struct sd *sd, const struct filter *filter,
			sd_foreach_service_cb foreach_cb,
			void *foreach_cb_data);

typedef bool (*sd_foreach_sub_cb)(int64_t sub_id, struct sub *sub,
				  void *foreach_cb_data);
void sd_foreach_sub(struct sd *sd, sd_foreach_sub_cb foreach_cb,
		    void *foreach_cb_data);

#endif
