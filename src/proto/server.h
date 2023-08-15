/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef SERVER_H
#define SERVER_H

#include <event.h>

#include "sd.h"

struct server;

struct server *server_create(const char *name, const char *server_addr,
			     struct event_base *event_base);
void server_destroy(struct server *server);

int server_start(struct server *server);

#endif
