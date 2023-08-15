/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef SD_ERR_H
#define SD_ERR_H

#define SD_ERR_PERM_DENIED (-1)

#define SD_ERR_CLIENT_ALREADY_EXISTS (-2)
#define SD_ERR_NO_SUCH_CLIENT (-3)

#define SD_ERR_NO_SUCH_SERVICE (-4)
#define SD_ERR_SERVICE_SAME_GENERATION_BUT_DIFFERENT_DATA (-5)
#define SD_ERR_NEWER_SERVICE_GENERATION_EXISTS (-6)

#define SD_ERR_SUB_ALREADY_EXISTS (-7)
#define SD_ERR_INVALID_FILTER (-8)
#define SD_ERR_NO_SUCH_SUB (-9)

const char *sd_str_error(int err);

#endif
