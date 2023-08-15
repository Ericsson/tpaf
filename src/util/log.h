/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include <stdarg.h>
/* for the LOG_* level macros */
#include <syslog.h>

struct log_ctx;

struct log_ctx *log_ctx_create(const struct log_ctx *parent_ctx);

struct log_ctx *log_ctx_create_prefix(const struct log_ctx *parent_ctx,
				      const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

void log_ctx_destroy(struct log_ctx *log_ctx);

void log_ctx_set_prefix(struct log_ctx *log_ctx, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

void log_init(int filter_level, bool stderr);

void log_debug(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));
void log_info(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));
void log_warn(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));
void log_error(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));

void log_debug_c(const struct log_ctx *log_ctx, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
void log_info_c(const struct log_ctx *log_ctx, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
void log_warn_c(const struct log_ctx *log_ctx, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
void log_error_c(const struct log_ctx *log_ctx, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

int log_str_to_level(const char *level_s);
const char *log_level_to_str(int level);

bool log_is_debug_enabled(void);

#endif
