/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef SUB_MAP_H
#define SUB_MAP_H

#include <stdint.h>

#include "pmap.h"

struct sub;

PMAP_GEN_REF_CNT_WRAPPER(sub_map, struct sub_map, int64_t, struct sub,
			 sub_inc_ref, sub_dec_ref,
			 static __attribute__((unused)))

#endif
