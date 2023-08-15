/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#ifndef PROTO_H
#define PROTO_H

#include <stdarg.h>

#include "msg.h"

#define PROTO_VERSION ((int64_t)2)

#define PROTO_MSG_TYPE_REQ "request"
#define PROTO_MSG_TYPE_ACCEPT "accept"
#define PROTO_MSG_TYPE_NOTIFY "notify"
#define PROTO_MSG_TYPE_COMPLETE "complete"
#define PROTO_MSG_TYPE_FAIL "fail"

#define PROTO_CMD_HELLO "hello"
#define PROTO_CMD_SUBSCRIBE "subscribe"
#define PROTO_CMD_UNSUBSCRIBE "unsubscribe"
#define PROTO_CMD_PUBLISH "publish"
#define PROTO_CMD_UNPUBLISH "unpublish"
#define PROTO_CMD_PING "ping"
#define PROTO_CMD_SUBSCRIPTIONS "subscriptions"
#define PROTO_CMD_SERVICES "services"
#define PROTO_CMD_CLIENTS "clients"

#define PROTO_NUM_MANDANTORY_FIELDS 3 /* TA_CMD, TA_ID and MSG_TYPE */

#define PROTO_FIELD_TA_CMD "ta-cmd"
#define PROTO_FIELD_TA_ID "ta-id"
#define PROTO_FIELD_MSG_TYPE "msg-type"

#define PROTO_FIELD_FAIL_REASON "fail-reason"

#define PROTO_FIELD_PROTO_MIN_VERSION "protocol-minimum-version"
#define PROTO_FIELD_PROTO_MAX_VERSION "protocol-maximum-version"
#define PROTO_FIELD_PROTO_VERSION "protocol-version"

#define PROTO_FIELD_SERVICE_ID "service-id"
#define PROTO_FIELD_SERVICE_PROPS "service-props"

#define PROTO_FIELD_GENERATION "generation"
#define PROTO_FIELD_TTL "ttl"
#define PROTO_FIELD_ORPHAN_SINCE "orphan-since"

#define PROTO_FIELD_SUBSCRIPTION_ID "subscription-id"
#define PROTO_FIELD_FILTER "filter"

#define PROTO_FIELD_CLIENT_ID "client-id"
#define PROTO_FIELD_CLIENT_ADDR "client-address"
#define PROTO_FIELD_TIME "time"

#define PROTO_FIELD_MATCH_TYPE "match-type"

#define PROTO_MATCH_TYPE_APPEARED "appeared"
#define PROTO_MATCH_TYPE_MODIFIED "modified"
#define PROTO_MATCH_TYPE_DISAPPEARED "disappeared"

#define PROTO_FAIL_REASON_NO_HELLO "no-hello"
#define PROTO_FAIL_REASON_CLIENT_ID_EXISTS "client-id-exists"
#define PROTO_FAIL_REASON_INVALID_FILTER_SYNTAX "invalid-filter-syntax"
#define PROTO_FAIL_REASON_SUBSCRIPTION_ID_EXISTS "subscription-id-exists"
#define PROTO_FAIL_REASON_NON_EXISTENT_SUBSCRIPTION_ID \
    "non-existent-subscription-id"
#define PROTO_FAIL_REASON_NON_EXISTENT_SERVICE_ID "non-existent-service-id"
#define PROTO_FAIL_REASON_UNSUPPORTED_PROTOCOL_VERSION \
    "unsupported-protocol-version"
#define PROTO_FAIL_REASON_PERMISSION_DENIED "permission-denied"
#define PROTO_FAIL_REASON_OLD_GENERATION "old-generation"
#define PROTO_FAIL_REASON_SAME_GENERATION_BUT_DIFFERENT \
    "same-generation-but-different"
#define PROTO_FAIL_REASON_INSUFFICIENT_RESOURCES "insufficient-resources"

enum proto_msg_type {
    proto_msg_type_req,
    proto_msg_type_accept,
    proto_msg_type_notify,
    proto_msg_type_complete,
    proto_msg_type_fail,
    proto_msg_type_undefined
};

typedef void (*proto_response_cb)(int64_t ta_id, enum proto_msg_type msg_type,
                                  void **args, void **optargs, void *user);

enum proto_field_type {
    proto_field_type_str,
    /* '63' since the protocol field type uses the non-negative
     * portion of a signed 64-bit integer */
    proto_field_type_uint63, 
    proto_field_type_number,
    proto_field_type_props,
    proto_field_type_match_type
};

struct proto_field
{
    const char *name;
    enum proto_field_type type;
};

enum proto_ia_type {
    proto_ia_type_single_response,
    proto_ia_type_multi_response
};

#define MAX_FIELDS (16)

struct proto_ta_type
{
    const char *cmd;
    enum proto_ia_type ia_type;
    struct proto_field req_fields[MAX_FIELDS];
    struct proto_field opt_req_fields[MAX_FIELDS];
    struct proto_field notify_fields[MAX_FIELDS];
    struct proto_field opt_notify_fields[MAX_FIELDS];
    struct proto_field complete_fields[MAX_FIELDS];
    struct proto_field opt_fail_fields[MAX_FIELDS];
};

enum proto_ta_state
{
    proto_ta_state_initialized,
    proto_ta_state_requested,
    proto_ta_state_accepted,
    proto_ta_state_completed,
    proto_ta_state_failed
};

struct proto_ta
{
    const struct proto_ta_type *type;

    enum proto_ta_state state;

    int64_t ta_id;

    void *req_field_values[MAX_FIELDS];
    void *opt_req_field_values[MAX_FIELDS];

    struct log_ctx *log_ctx;
};

struct proto_ta *proto_ta_create(const struct log_ctx *log_ctx);
void proto_ta_destroy(struct proto_ta *ta);

const char *proto_ta_get_cmd(struct proto_ta *ta);

int proto_ta_req(struct proto_ta *ta, const struct msg *req_msg);

const char *proto_ta_get_req_field_str_value(const struct proto_ta *ta,
					     size_t field_idx);
const int64_t *proto_ta_get_req_field_uint63_value(const struct proto_ta *ta,
						   size_t field_idx);
const double *proto_ta_get_req_field_number_value(const struct proto_ta *ta,
						  size_t field_idx);
const struct props *proto_ta_get_req_field_props_value(
    const struct proto_ta *ta, size_t field_idx);
const enum sub_match_type *proto_ta_get_req_field_match_type_value(
    const struct proto_ta *ta, size_t field_idx);

const char *proto_ta_get_opt_req_field_str_value(const struct proto_ta *ta,
						 size_t field_idx);
const int64_t *proto_ta_get_opt_req_field_uint63_value(
    const struct proto_ta *ta, size_t field_idx);
const double *proto_ta_get_opt_req_field_number_value(const struct proto_ta *ta,
						      size_t field_idx);
const struct props *proto_ta_get_opt_req_field_props_value(
    const struct proto_ta *ta, size_t field_idx);
const enum sub_match_type *proto_ta_get_opt_req_field_match_type_value(
    const struct proto_ta *ta, size_t field_idx);

struct msg *proto_ta_accept(struct proto_ta *ta, ...);
struct msg *proto_ta_notify(struct proto_ta *ta, ...);
struct msg *proto_ta_complete(struct proto_ta *ta, ...);
struct msg *proto_ta_fail(struct proto_ta *ta, ...);

bool proto_ta_has_term(struct proto_ta *ta);

#endif
