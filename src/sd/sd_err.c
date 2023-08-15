/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include "util.h"

#include "sd_err.h"

const char *sd_str_error(int err)
{
    ut_assert(err < 0);

    return "Unknown error";
}
