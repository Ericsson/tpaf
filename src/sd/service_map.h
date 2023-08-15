/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef SERVICE_MAP_H
#define SERVICE_MAP_H

#include <stdint.h>

#include "pmap.h"

struct service;

PMAP_GEN_REF_CNT_WRAPPER(service_map, struct service_map, int64_t,
			 struct service, service_inc_ref, service_dec_ref,
			 static __attribute__((unused)))

#endif
