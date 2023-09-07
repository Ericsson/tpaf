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

void log_init(int filter_level, bool stderr, int facility)
{
    int option = LOG_PID;

    if (stderr)
	option |= LOG_PERROR;

    openlog(NULL, option, facility);

    setlogmask(LOG_UPTO(filter_level));
}

struct named_value
{
    int value;
    const char *name;
};

static const struct named_value facilities[] = {
    { .value = LOG_AUTH, .name = "auth"},
    { .value = LOG_AUTHPRIV, .name = "authpriv"},
    { .value = LOG_CRON, .name = "cron"},
    { .value = LOG_DAEMON, .name = "daemon"},
    { .value = LOG_FTP, .name = "ftp"},
    { .value = LOG_KERN, .name = "kern"},
    { .value = LOG_LOCAL0, .name = "local0"},
    { .value = LOG_LOCAL1, .name = "local1"},
    { .value = LOG_LOCAL2, .name = "local2"},
    { .value = LOG_LOCAL3, .name = "local3"},
    { .value = LOG_LOCAL4, .name = "local4"},
    { .value = LOG_LOCAL5, .name = "local5"},
    { .value = LOG_LOCAL6, .name = "local6"},
    { .value = LOG_LOCAL7, .name = "local7"},
    { .value = LOG_LPR, .name = "lpr"},
    { .value = LOG_MAIL, .name = "mail"},
    { .value = LOG_NEWS, .name = "news"},
    { .value = LOG_SYSLOG, .name = "syslog"},
    { .value = LOG_USER, .name = "user"},
    { .value = LOG_UUCP, .name = "uucp" }
};

static const size_t facilities_len = UT_ARRAY_LEN(facilities);

static void named_values_foreach(const struct named_value *values, size_t len,
				 log_foreach_cb cb, void *cb_data)
{
    size_t i;
    for (i = 0; i < len; i++) {
	const struct named_value *nv = &values[i];
	cb(nv->value, nv->name, cb_data);
    }
}

struct search_state {
    const char *name;
    int value;
};

static void name_lookup_cb(int value, const char *name, void *cb_data)
{
    struct search_state *state = cb_data;

    if (state->value == value)
	state->name = name;
}

static const char *name_lookup(const struct named_value *values, size_t len,
			       int needle_value)
{
    struct search_state state = {
	.name = NULL,
	.value = needle_value
    };

    named_values_foreach(values, len, name_lookup_cb, &state);

    return state.name;
}

static void value_lookup_cb(int value, const char *name, void *cb_data)
{
    struct search_state *state = cb_data;

    if (strcmp(state->name, name) == 0)
	state->value = value;
}

static int value_lookup(const struct named_value *values, size_t len,
			const char *needle_name)
{
    struct search_state state = {
	.name = needle_name,
	.value = -1
    };

    named_values_foreach(values, len, value_lookup_cb, &state);

    return state.value;
}

int log_str_to_facility(const char *facility_s)
{
    return value_lookup(facilities, facilities_len, facility_s);
}

const char *log_facility_to_str(int facility)
{
    return name_lookup(facilities, facilities_len, facility);
}

void log_facility_foreach(log_foreach_cb cb, void *cb_data)
{
    named_values_foreach(facilities, facilities_len, cb, cb_data);
}

static const struct named_value levels[] = {
    { .value = LOG_DEBUG, .name = "debug" },
    { .value = LOG_INFO, .name = "info" },
    { .value = LOG_NOTICE, .name = "notice" },
    { .value = LOG_WARNING, .name = "warning" },
    { .value = LOG_ERR, .name = "error" }
};
static const size_t levels_len = UT_ARRAY_LEN(levels);

int log_str_to_level(const char *level_s)
{
    return value_lookup(levels, levels_len, level_s);
}

const char *log_level_to_str(int level_value)
{
    return name_lookup(levels, levels_len, level_value);
}

void log_level_foreach(log_foreach_cb cb, void *cb_data)
{
    named_values_foreach(levels, levels_len, cb, cb_data);
}

bool log_is_debug_enabled(void)
{
    int mask = setlogmask(0);

    return mask & LOG_DEBUG;
}
