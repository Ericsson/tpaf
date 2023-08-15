/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef CLIENT_MAP_H
#define CLIENT_MAP_H

#include <stdint.h>

#include "pmap.h"

struct client;

PMAP_GEN_REF_CNT_WRAPPER(client_map, struct client_map, int64_t,
			 struct client, client_inc_ref, client_dec_ref,
			 static __attribute__((unused)))

#endif
