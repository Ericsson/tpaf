/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <stdio.h>
#include <string.h>

#include "util.h"

#include "log.h"

struct log_ctx
{
    const struct log_ctx *parent_ctx;
    char *prefix;
};

struct log_ctx *log_ctx_create(const struct log_ctx *parent_ctx)
{
    return log_ctx_create_prefix(parent_ctx, "%s", "");
}

struct log_ctx *log_ctx_create_prefix(const struct log_ctx *parent_ctx,
				      const char *format, ...)
{
    struct log_ctx *log_ctx = ut_malloc(sizeof(struct log_ctx));
    va_list ap;
    va_start(ap, format);

    *log_ctx = (struct log_ctx) {
	.parent_ctx = parent_ctx,
	.prefix = ut_vasprintf(format, ap)
    };

    va_end(ap);

    return log_ctx;
}

void log_ctx_destroy(struct log_ctx *log_ctx)
{
    if (log_ctx != NULL) {
	ut_free(log_ctx->prefix);
	ut_free(log_ctx);
    }
}

void log_ctx_set_prefix(struct log_ctx *log_ctx, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    ut_free(log_ctx->prefix);

    log_ctx->prefix = ut_vasprintf(format, ap);

    va_end(ap);
}

static char *ctx_get_full_prefix(const struct log_ctx *log_ctx)
{
    if (log_ctx->parent_ctx != NULL) {
	char *parent_prefix = ctx_get_full_prefix(log_ctx->parent_ctx);
	char *full_prefix =
	    ut_asprintf("%s%s", parent_prefix, log_ctx->prefix);

	ut_free(parent_prefix);

	return full_prefix;
    } else
	return ut_strdup(log_ctx->prefix);
}

static void vlog_event(int syslog_prio, const struct log_ctx *log_ctx,
		       const char *format, va_list ap)
{
    char *prefix = ctx_get_full_prefix(log_ctx);
    char *payload = ut_vasprintf(format, ap);
    char *msg = ut_asprintf("%s%s", prefix, payload);

    syslog(syslog_prio, "%s", msg);

    ut_free(prefix);
    ut_free(payload);
    ut_free(msg);
}

#define GEN_LOG_FUN(name, prio)			\
    void name(const char *format, ...)		\
    {						\
	va_list ap;				\
	va_start(ap, format);			\
						\
	vlog_event(prio, NULL, format, ap);	\
						\
	va_end(ap);				\
    }						\

GEN_LOG_FUN(log_debug, LOG_DEBUG)
GEN_LOG_FUN(log_info, LOG_INFO)
GEN_LOG_FUN(log_warn, LOG_WARNING)
GEN_LOG_FUN(log_error, LOG_ERR)

#define GEN_LOG_C_FUN(name, prio)		\
    void name(const struct log_ctx *log_ctx, const char *format, ...)	\
    {						\
	va_list ap;				\
	va_start(ap, format);			\
						\
	vlog_event(prio, log_ctx, format, ap);	\
						\
	va_end(ap);				\
    }						\

GEN_LOG_C_FUN(log_debug_c, LOG_DEBUG)
GEN_LOG_C_FUN(log_info_c, LOG_INFO)
GEN_LOG_C_FUN(log_warn_c, LOG_WARNING)
GEN_LOG_C_FUN(log_error_c, LOG_ERR)

#define TPAF_FACILITY LOG_DAEMON

void log_init(int filter_level, bool stderr)
{
    int option = LOG_PID;

    if (stderr)
	option |= LOG_PERROR;

    openlog(NULL, option, TPAF_FACILITY);

    setlogmask(LOG_UPTO(filter_level));
}

struct level
{
    int value;
    const char *name;
};

static const struct level levels[] = {
    {
	.value = LOG_DEBUG,
	.name = "debug"
    },
    {
	.value = LOG_INFO,
	.name = "info"
    },
    {
	.value = LOG_ERR,
	.name = "error"
    }
};
static const size_t levels_len = UT_ARRAY_LEN(levels);

int log_str_to_level(const char *level_s)
{
    size_t i;

    for (i = 0; i < levels_len; i++) {
	const struct level *level = &levels[i];

	if (strcmp(level->name, level_s) == 0)
	    return level->value;
    }

    return -1;
}

const char *log_level_to_str(int level_value)
{
    size_t i;

    for (i = 0; i < levels_len; i++) {
	const struct level *level = &levels[i];

	if (level->value == level_value)
	    return level->name;
    }

    return NULL;
}

bool log_is_debug_enabled(void)
{
    int mask = setlogmask(0);

    return mask & LOG_DEBUG;
}
