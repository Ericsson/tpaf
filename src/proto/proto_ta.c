/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include <jansson.h>

#include "props.h"
#include "sub_match.h"
#include "util.h"
#include "log.h"

#include "proto_ta.h"

UT_STATIC_ASSERT(json_int_is_64, sizeof(json_int_t) == sizeof(int64_t));

static const struct proto_ta_type hello_ta =
{
    .cmd = PROTO_CMD_HELLO,
    .ia_type = proto_ia_type_single_response,
    .req_fields = {
        { PROTO_FIELD_CLIENT_ID, proto_field_type_uint63 },
        { PROTO_FIELD_PROTO_MIN_VERSION, proto_field_type_uint63 },
        { PROTO_FIELD_PROTO_MAX_VERSION, proto_field_type_uint63 }
    },
    .complete_fields = {
        { PROTO_FIELD_PROTO_VERSION, proto_field_type_uint63 }
    },
    .opt_fail_fields = {
        { PROTO_FIELD_FAIL_REASON, proto_field_type_str }
    }
};

static const struct proto_ta_type publish_ta =
{
    .cmd = PROTO_CMD_PUBLISH,
    .ia_type = proto_ia_type_single_response,
    .req_fields = {
        { PROTO_FIELD_SERVICE_ID, proto_field_type_uint63 },
        { PROTO_FIELD_GENERATION, proto_field_type_uint63 },
        { PROTO_FIELD_SERVICE_PROPS, proto_field_type_props },
        { PROTO_FIELD_TTL, proto_field_type_uint63 }
    },
    .opt_fail_fields = {
        { PROTO_FIELD_FAIL_REASON, proto_field_type_str }
    }
};

static const struct proto_ta_type unpublish_ta =
{
    .cmd = PROTO_CMD_UNPUBLISH,
    .ia_type = proto_ia_type_single_response,
    .req_fields = {
        { PROTO_FIELD_SERVICE_ID, proto_field_type_uint63 }
    },
    .opt_fail_fields = {
        { PROTO_FIELD_FAIL_REASON, proto_field_type_str }
    }
};

static const struct proto_ta_type subscribe_ta =
{
    .cmd = PROTO_CMD_SUBSCRIBE,
    .ia_type = proto_ia_type_multi_response,
    .req_fields = {
        { PROTO_FIELD_SUBSCRIPTION_ID, proto_field_type_uint63 }
    },
    .opt_req_fields = {
	{ PROTO_FIELD_FILTER, proto_field_type_str }
    },
    .notify_fields = {
        { PROTO_FIELD_MATCH_TYPE, proto_field_type_match_type },
        { PROTO_FIELD_SERVICE_ID, proto_field_type_uint63 }
    },
    .opt_notify_fields = {
        { PROTO_FIELD_GENERATION, proto_field_type_uint63 },
        { PROTO_FIELD_SERVICE_PROPS, proto_field_type_props },
        { PROTO_FIELD_TTL, proto_field_type_uint63 },
        { PROTO_FIELD_CLIENT_ID, proto_field_type_uint63 },
        { PROTO_FIELD_ORPHAN_SINCE, proto_field_type_number }
    },
    .opt_fail_fields = {
        { PROTO_FIELD_FAIL_REASON, proto_field_type_str }
    }
};

static const struct proto_ta_type unsubscribe_ta =
{
    .cmd = PROTO_CMD_UNSUBSCRIBE,
    .ia_type = proto_ia_type_single_response,
    .req_fields = {
        { PROTO_FIELD_SUBSCRIPTION_ID, proto_field_type_uint63 }
    },
    .opt_fail_fields = {
        { PROTO_FIELD_FAIL_REASON, proto_field_type_str }
    }
};

static const struct proto_ta_type ping_ta =
{
    .cmd = PROTO_CMD_PING,
    .ia_type = proto_ia_type_single_response,
    .opt_fail_fields = {
        { PROTO_FIELD_FAIL_REASON, proto_field_type_str }
    }
};

static const struct proto_ta_type services_ta =
{
    .cmd = PROTO_CMD_SERVICES,
    .ia_type = proto_ia_type_multi_response,
    .opt_req_fields = {
	{ PROTO_FIELD_FILTER, proto_field_type_str }
    },
    .notify_fields = {
        { PROTO_FIELD_SERVICE_ID, proto_field_type_uint63 },
        { PROTO_FIELD_GENERATION, proto_field_type_uint63 },
        { PROTO_FIELD_SERVICE_PROPS, proto_field_type_props },
        { PROTO_FIELD_TTL, proto_field_type_uint63 },
        { PROTO_FIELD_CLIENT_ID, proto_field_type_uint63 }
    },
    .opt_notify_fields = {
        { PROTO_FIELD_ORPHAN_SINCE, proto_field_type_number }
    },
    .opt_fail_fields = {
        { PROTO_FIELD_FAIL_REASON, proto_field_type_str }
    }
};

static const struct proto_ta_type subscriptions_ta =
{
    .cmd = PROTO_CMD_SUBSCRIPTIONS,
    .ia_type = proto_ia_type_multi_response,
    .notify_fields = {
        { PROTO_FIELD_SUBSCRIPTION_ID, proto_field_type_uint63 },
        { PROTO_FIELD_CLIENT_ID, proto_field_type_uint63 }
    },
    .opt_notify_fields = {
	{ PROTO_FIELD_FILTER, proto_field_type_str }
    }
};

static const struct proto_ta_type clients_ta =
{
    .cmd = PROTO_CMD_CLIENTS,
    .ia_type = proto_ia_type_multi_response,
    .notify_fields = {
        { PROTO_FIELD_CLIENT_ID, proto_field_type_uint63 },
        { PROTO_FIELD_CLIENT_ADDR, proto_field_type_str },
        { PROTO_FIELD_TIME, proto_field_type_uint63 }
    },
    .opt_fail_fields = {
        { PROTO_FIELD_FAIL_REASON, proto_field_type_str }
    }
};

static const struct proto_ta_type *proto_ta_types[] = {
    &hello_ta,
    &publish_ta,
    &unpublish_ta,
    &subscribe_ta,
    &unsubscribe_ta,
    &ping_ta,
    &services_ta,
    &subscriptions_ta,
    &clients_ta
};
static const size_t proto_ta_types_len = UT_ARRAY_LEN(proto_ta_types);

static const struct proto_ta_type *lookup_type(const char *cmd_name)
{
    size_t i;
    for (i = 0; i < proto_ta_types_len; i++) {
	const struct proto_ta_type *type = proto_ta_types[i];

	if (strcmp(type->cmd, cmd_name) == 0)
	    return type;
    }
    return NULL;
}

static int get_json(json_t *msg, const char *name, bool opt,
		    const struct log_ctx *log_ctx, json_t **value)
{
    *value = json_object_get(msg, name);
    if (*value == NULL && !opt) {
	log_info_c(log_ctx, "Request message is missing a required field "
		   "\"%s\".", name);
        return -1;
    }
    return 0;
}

#define GEN_TYPED_GET(type)						\
    static int get_json_ ## type(json_t *msg, const char *name, bool opt, \
				 const struct log_ctx *log_ctx,		\
				 json_t **value)			\
    {                                                                   \
	json_t *type;							\
	if (get_json(msg, name, opt, log_ctx, &type) < 0)		\
	    return -1;							\
	if (type != NULL && !json_is_## type(type)) {			\
	    log_debug_c(log_ctx, "Message field \"%s\" is not" \
		      "of the required %s type.", name, #type);		\
	    return -1;							\
	}								\
        *value = type;							\
        return 0;							\
    }

GEN_TYPED_GET(integer)
GEN_TYPED_GET(number)
GEN_TYPED_GET(string)
GEN_TYPED_GET(object)

static int get_uint63(json_t *msg, const char *name, bool opt,
		      const struct log_ctx *log_ctx, int64_t *uint63_value)
{
    json_t *json_value;
    if (get_json_integer(msg, name, opt, log_ctx, &json_value) < 0)
	return -1;

    int64_t value = json_integer_value(json_value);
    if (value < 0) {
	log_debug_c(log_ctx, "Non-negative integer type message field \"%s\" "
		    "has invalid value %"PRId64".", name, value);
	return -1;
    }

    *uint63_value = value;
    return 0;
}


static enum proto_msg_type get_msg_type(json_t *msg,
					const struct log_ctx *log_ctx)
{
    json_t *msg_type;
    if (get_json_string(msg, PROTO_FIELD_MSG_TYPE, false, log_ctx,
			&msg_type) < 0)
        return proto_msg_type_undefined;
    if (strcmp(json_string_value(msg_type), PROTO_MSG_TYPE_REQ) == 0)
        return proto_msg_type_req;
    if (strcmp(json_string_value(msg_type), PROTO_MSG_TYPE_ACCEPT) == 0)
        return proto_msg_type_accept;
    if (strcmp(json_string_value(msg_type), PROTO_MSG_TYPE_NOTIFY) == 0)
        return proto_msg_type_notify;
    if (strcmp(json_string_value(msg_type), PROTO_MSG_TYPE_COMPLETE) == 0)
        return proto_msg_type_complete;
    if (strcmp(json_string_value(msg_type), PROTO_MSG_TYPE_FAIL) == 0)
        return proto_msg_type_fail;

    log_debug_c(log_ctx, "Request is of an invalid message type \"%s\".",
		json_string_value(msg_type));

    return proto_msg_type_undefined;
}

static struct props *json_to_props(json_t *json_props,
				   const struct log_ctx *log_ctx)
{
    struct props *props = props_create();

    const char *prop_name;
    json_t *json_prop_values;
    json_object_foreach(json_props, prop_name, json_prop_values) {
        if (!json_is_array(json_prop_values)) {
	    log_debug_c(log_ctx, "Request service property has invalid type.");
            goto err_cleanup;
        }

        size_t i;
        json_t *json_prop_value;
        json_array_foreach(json_prop_values, i, json_prop_value) {
            struct pvalue *prop_value;
            if (json_is_integer(json_prop_value))
                prop_value =
                    pvalue_int64_create(json_integer_value(json_prop_value));
            else if (json_is_string(json_prop_value)) {
                prop_value =
                    pvalue_str_create(json_string_value(json_prop_value));
            } else {
		log_debug_c(log_ctx, "Service property value has invalid "
			    "type.");
                goto err_cleanup;
            }
            props_add(props, prop_name, prop_value);
            pvalue_destroy(prop_value);
        }
    }

    return props;

 err_cleanup:

    props_destroy(props);

    return NULL;
}

static void free_fields(const struct proto_field *fields, void **field_values)
{
    size_t i;
    for (i = 0; fields[i].name != NULL; i++) {
        switch (fields[i].type) {
        case proto_field_type_uint63:
        case proto_field_type_number:
        case proto_field_type_str:
        case proto_field_type_match_type:
            ut_free(field_values[i]);
            break;
        case proto_field_type_props:
            props_destroy(field_values[i]);
            break;
        default:
            assert(0);
            break;
        }
	field_values[i] = NULL;
    }
}

static int proto_match_type_to_enum(const char *match_type_str,
				    enum sub_match_type *match_type)
{
    if (strcmp(match_type_str, PROTO_MATCH_TYPE_APPEARED) == 0)
	*match_type = sub_match_type_appeared;
    else if (strcmp(match_type_str, PROTO_MATCH_TYPE_MODIFIED) == 0)
	*match_type = sub_match_type_modified;
    else if (strcmp(match_type_str, PROTO_MATCH_TYPE_DISAPPEARED) == 0)
	*match_type = sub_match_type_disappeared;
    else
	return -1;

    return 0;
}

static const char *enum_to_proto_match_type(enum sub_match_type match_type)
{
    switch (match_type) {
    case sub_match_type_appeared:
	return PROTO_MATCH_TYPE_APPEARED;
    case sub_match_type_modified:
	return PROTO_MATCH_TYPE_MODIFIED;
    case sub_match_type_disappeared:
	return PROTO_MATCH_TYPE_DISAPPEARED;
    default:
	ut_assert(0);
	break;
    }
}

static int get_fields(json_t *response, const struct proto_field *fields,
                      bool opt, const struct log_ctx *log_ctx,
		      void **field_values)
{
    int num_actual_fields = 0;
    int i;
    for (i = 0; fields != NULL && fields[i].name != NULL; i++) {

        void *arg = NULL;

        switch (fields[i].type) {
        case proto_field_type_uint63: {
            json_t *json_int;
            int rc = get_json_integer(response, fields[i].name, opt, log_ctx,
				      &json_int);
            if (rc < 0)
                goto err_free_fields;
            else if (json_int != NULL) {
		int64_t uint63_value = json_integer_value(json_int);

		if (uint63_value < 0) /* not non-negative */
		    goto err_free_fields;

                arg = ut_memdup(&uint63_value, sizeof(uint63_value));
            }
            break;
        }
        case proto_field_type_number: {
            json_t *json_number;
            int rc = get_json_number(response, fields[i].name, opt, log_ctx,
				     &json_number);
            if (rc < 0)
                goto err_free_fields;
            else if (json_number != NULL) {
                double *nnum = ut_malloc(sizeof(double));
                *nnum = json_number_value(json_number);
                arg = nnum;
            }
            break;
        }
        case proto_field_type_str: {
            json_t *json_str;
            int rc = get_json_string(response, fields[i].name, opt, log_ctx,
				     &json_str);
            if (rc < 0)
                goto err_free_fields;
            if (json_str != NULL)
                arg = ut_strdup(json_string_value(json_str));
            break;
        }
        case proto_field_type_props: {
            json_t *json_props;
            int rc = get_json_object(response, fields[i].name, opt, log_ctx,
				     &json_props);
            if (rc < 0)
                goto err_free_fields;
            else if (json_props != NULL) {
                struct props *props = json_to_props(json_props, log_ctx);
                if (props == NULL)
                    goto err_free_fields;
                arg = props;
            }

            break;
        }
        case proto_field_type_match_type: {
            json_t *json_match_type;
            int rc = get_json_string(response, fields[i].name, opt, log_ctx,
				     &json_match_type);
            if (rc < 0)
                goto err_free_fields;
            else if (json_match_type != NULL) {
                const char *match_type_str = json_string_value(json_match_type);

                enum sub_match_type match_type;

		if (proto_match_type_to_enum(match_type_str,
					     &match_type) < 0) {
		    log_debug_c(log_ctx, "Invalid match type \"%s\".",
				match_type_str);
                    goto err_free_fields;
                }

                arg = ut_memdup(&match_type, sizeof(enum sub_match_type));
            }

            break;
        }
        default:
            assert(0);
            break;
        }

        field_values[i] = arg;

	if (arg != NULL)
	    num_actual_fields++;

    }

    return num_actual_fields;

 err_free_fields:
    free_fields(fields, field_values);

    return -1;
}

struct proto_ta *proto_ta_create(const struct log_ctx *log_ctx)
{
    struct proto_ta *ta = ut_malloc(sizeof(struct proto_ta));

    *ta = (struct proto_ta) {
	.state = proto_ta_state_initialized,
	.ta_id = -1,
	.log_ctx = log_ctx_create(log_ctx)
    };

    return ta;
}

void proto_ta_destroy(struct proto_ta *ta)
{
    if (ta != NULL) {
	if (ta->type != NULL) {
	    free_fields(ta->type->req_fields, ta->req_field_values);
	    free_fields(ta->type->opt_req_fields, ta->opt_req_field_values);
	}

	log_ctx_destroy(ta->log_ctx);

	ut_free(ta);
    }
}

const char *proto_ta_get_cmd(struct proto_ta *ta)
{
    if (ta->type == NULL)
	return NULL;
    return ta->type->cmd;
}

int proto_ta_req(struct proto_ta *ta, const struct msg *req_msg)
{
    ut_assert(ta->state == proto_ta_state_initialized);

    json_error_t json_err;
    json_t *req_json =
	json_loadb(msg_data(req_msg), msg_len(req_msg), 0, &json_err);

    if (req_json == NULL) {
	log_debug_c(ta->log_ctx, "Error parsning request message JSON at "
		    "(%d, %d): %s.", json_err.line, json_err.column,
		    json_err.text);
	goto err;
    }

    int64_t ta_id;
    if (get_uint63(req_json, PROTO_FIELD_TA_ID, false, ta->log_ctx, &ta_id) < 0)
        goto err_free_req_json;

    ta->ta_id = ta_id;

    log_ctx_set_prefix(ta->log_ctx, "<ta: %"PRId64"> ", ta->ta_id);

    json_t *cmd;
    if (get_json_string(req_json, PROTO_FIELD_TA_CMD, false, ta->log_ctx,
			&cmd) < 0)
        goto err_free_req_json;

    enum proto_msg_type msg_type = get_msg_type(req_json, ta->log_ctx);
    if (msg_type != proto_msg_type_req)
        goto err_free_req_json;

    ta->type = lookup_type(json_string_value(cmd));

    if (ta->type == NULL) {
	log_debug_c(ta->log_ctx, "Request message has unknown command \"%s\".",
		    json_string_value(cmd));
	goto err_free_req_json;
    }

    int num_req_fields =
	get_fields(req_json, ta->type->req_fields, false, ta->log_ctx,
		   ta->req_field_values);
    if (num_req_fields < 0)
	goto err_free_req_json;

    int num_opt_req_fields =
	get_fields(req_json, ta->type->opt_req_fields, true, ta->log_ctx,
		   ta->opt_req_field_values);
    if (num_opt_req_fields < 0)
	goto err_free_req_fields;

    log_debug_c(ta->log_ctx, "\"%s\" command request received with "
		"transaction id %"PRId64".", json_string_value(cmd),
		ta_id);

    int total_fields_visited =
	PROTO_NUM_MANDANTORY_FIELDS + num_req_fields + num_opt_req_fields;

    if (json_object_size(req_json) > total_fields_visited) {
	log_info_c(ta->log_ctx, "Request message carries %zd unknown fields.",
		   json_object_size(req_json) - total_fields_visited);
	goto err_free_opt_req_fields;
    }

    json_decref(req_json);

    ta->state = proto_ta_state_requested;

    return 0;

err_free_opt_req_fields:
    free_fields(ta->type->opt_req_fields, ta->opt_req_field_values);
err_free_req_fields:
    free_fields(ta->type->req_fields, ta->req_field_values);
err_free_req_json:
    json_decref(req_json);
err:
    return -1;
}

static bool prop_to_json(const char *prop_name,
                         const struct pvalue *prop_value, void *user)
{
    json_t *json_props = user;

    json_t *value;

    if (pvalue_is_int64(prop_value))
        value = json_integer(pvalue_int64(prop_value));
    else {
        assert(pvalue_is_str(prop_value));
        value = json_string(pvalue_str(prop_value));
    }
    json_t *value_list = json_object_get(json_props, prop_name);

    if (value_list == NULL) {
        value_list = json_array();
        json_object_set_new(json_props, prop_name, value_list);
    }

    json_array_append_new(value_list, value);

    return true;
}

static json_t *props_to_json(const struct props *props)
{
    json_t *json_props = json_object();

    props_foreach(props, prop_to_json, json_props);

    return json_props;
}

static void set_field(json_t *req, const struct proto_field *field,
		      const void *field_value)
{
    switch (field->type) {
    case proto_field_type_uint63: {
	int64_t uint63_value = *((const int64_t *)field_value);
	ut_assert(uint63_value >= 0);
	json_object_set_new(req, field->name, json_integer(uint63_value));
	break;
    }
    case proto_field_type_number: {
	double double_value = *((const double *)field_value);
	json_object_set_new(req, field->name, json_real(double_value));
	break;
    }
    case proto_field_type_str:
	json_object_set_new(req, field->name,
			    json_string((const char *)field_value));
	break;
    case proto_field_type_props: {
	const struct props *props = field_value;
	json_t *json_props = props_to_json(props);
	json_object_set_new(req, field->name, json_props);
	break;
    }
    case proto_field_type_match_type: {
	enum sub_match_type match_type =
	    *((const enum sub_match_type *)field_value);

	const char *match_type_s = enum_to_proto_match_type(match_type);
	json_object_set_new(req, field->name, json_string(match_type_s));
	break;
    }
    }
}

static json_t *create_response(const char *cmd, int64_t ta_id,
			       const char *msg_type_str)
{
    json_t *response = json_object();

    if (response == NULL)
	ut_mem_exhausted();

    json_object_set_new(response, PROTO_FIELD_TA_CMD, json_string(cmd));
    json_object_set_new(response, PROTO_FIELD_TA_ID, json_integer(ta_id));
    json_object_set_new(response, PROTO_FIELD_MSG_TYPE,
			json_string(msg_type_str));
    return response;
}

static struct msg *produce_response(struct proto_ta *ta,
				    enum proto_msg_type msg_type,
				    va_list ap)
{
    const char *msg_type_str;
    const struct proto_field *fields;
    const struct proto_field *opt_fields;

    switch (msg_type) {
    case proto_msg_type_accept:
	msg_type_str = PROTO_MSG_TYPE_ACCEPT;
	fields = NULL;
	opt_fields = NULL;
	break;
    case proto_msg_type_notify:
	msg_type_str = PROTO_MSG_TYPE_NOTIFY;
	fields = ta->type->notify_fields;
	opt_fields = ta->type->opt_notify_fields;
	break;
    case proto_msg_type_complete:
	msg_type_str = PROTO_MSG_TYPE_COMPLETE;
	fields = ta->type->complete_fields;
	opt_fields = NULL;
	break;
    case proto_msg_type_fail:
	msg_type_str = PROTO_MSG_TYPE_FAIL;
	fields = NULL;
	opt_fields = ta->type->opt_fail_fields;
	break;
    default:
	ut_assert(false);
	break;
    }

    json_t *response = create_response(ta->type->cmd, ta->ta_id, msg_type_str);

    int i;
    for (i = 0; fields != NULL && fields[i].name != NULL; i++) {
	const void *field_value = va_arg(ap, const void *);

	set_field(response, &fields[i], field_value);
    }

    for (i = 0; opt_fields != NULL && opt_fields[i].name != NULL; i++) {
	const void *field_value = va_arg(ap, const void *);

	if (field_value != NULL) 
	    set_field(response, &opt_fields[i], field_value);
    }

    char *data = json_dumps(response, 0);

    json_decref(response);

    return msg_create_prealloc(data, strlen(data));
}

static const void *get_req_field_value(const struct proto_ta *ta,
				       enum proto_field_type field_type,
				       size_t field_idx)
{
    ut_assert(ta->type->req_fields[field_idx].name != NULL);
    ut_assert(ta->type->req_fields[field_idx].type == field_type);

    void *value = ta->req_field_values[field_idx];
    ut_assert(value != NULL);

    return value;
}

const char *proto_ta_get_req_field_str_value(const struct proto_ta *ta,
					     size_t field_idx)
{
    return get_req_field_value(ta, proto_field_type_str, field_idx);
}

const int64_t *proto_ta_get_req_field_uint63_value(const struct proto_ta *ta,
						  size_t field_idx)
{
    return get_req_field_value(ta, proto_field_type_uint63, field_idx);
}

const double *proto_ta_get_req_field_number_value(const struct proto_ta *ta,
						 size_t field_idx)
{
    return get_req_field_value(ta, proto_field_type_number, field_idx);
}

const struct props *proto_ta_get_req_field_props_value(
    const struct proto_ta *ta, size_t field_idx)
{
    return get_req_field_value(ta, proto_field_type_props, field_idx);
}

const enum sub_match_type *proto_ta_get_req_field_match_type_value(
    const struct proto_ta *ta, size_t field_idx)
{
    return get_req_field_value(ta, proto_field_type_match_type, field_idx);
}

const char *proto_ta_get_req_opt_field_str_value(const struct proto_ta *ta,
						 size_t field_idx);
const int64_t *proto_ta_get_req_opt_field_uint63_value(
    const struct proto_ta *ta, size_t field_idx);
const double *proto_ta_get_req_opt_field_number_value(
    const struct proto_ta *ta, size_t field_idx);
const struct props *proto_ta_get_req_opt_field_props_value(
    const struct proto_ta *ta, size_t field_idx);
const enum sub_match_type *proto_ta_get_req_opt_field_match_value(
    const struct proto_ta *ta, size_t field_idx);

static const void *get_opt_req_field_value(const struct proto_ta *ta,
				       enum proto_field_type field_type,
				       size_t field_idx)
{
    ut_assert(ta->type->opt_req_fields[field_idx].name != NULL);
    ut_assert(ta->type->opt_req_fields[field_idx].type == field_type);

    return ta->opt_req_field_values[field_idx];
}

const char *proto_ta_get_opt_req_field_str_value(const struct proto_ta *ta,
					     size_t field_idx)
{
    return get_opt_req_field_value(ta, proto_field_type_str, field_idx);
}

const int64_t *proto_ta_get_opt_req_field_uint63_value(
    const struct proto_ta *ta, size_t field_idx)
{
    return get_opt_req_field_value(ta, proto_field_type_uint63, field_idx);
}

const double *proto_ta_get_opt_req_field_number_value(const struct proto_ta *ta,
						 size_t field_idx)
{
    return get_opt_req_field_value(ta, proto_field_type_number, field_idx);
}

const struct props *proto_ta_get_opt_req_field_props_value(
    const struct proto_ta *ta, size_t field_idx)
{
    return get_opt_req_field_value(ta, proto_field_type_props, field_idx);
}

const enum sub_match_type *proto_ta_get_opt_req_field_match_type_value(
    const struct proto_ta *ta, size_t field_idx)
{
    return get_opt_req_field_value(ta, proto_field_type_match_type, field_idx);
}

const char *proto_ta_get_opt_req_opt_field_str_value(
    const struct proto_ta *ta, size_t field_idx);
const int64_t *proto_ta_get_opt_req_opt_field_uint63_value(
    const struct proto_ta *ta, size_t field_idx);
const double *proto_ta_get_opt_req_opt_field_number_value(
    const struct proto_ta *ta, size_t field_idx);
const struct props *proto_ta_get_opt_req_opt_field_props_value(
    const struct proto_ta *ta, size_t field_idx);
const enum sub_match_type *proto_ta_get_opt_req_opt_field_match_value(
    const struct proto_ta *ta, size_t field_idx);

struct msg *proto_ta_accept(struct proto_ta *ta, ...)
{
    va_list ap;
    va_start(ap, ta);

    ut_assert(ta->type->ia_type == proto_ia_type_multi_response &&
	      ta->state == proto_ta_state_requested);
	      
    struct msg *response = produce_response(ta, proto_msg_type_accept, ap);

    ta->state = proto_ta_state_accepted;

    va_end(ap);

    return response;
}

struct msg *proto_ta_notify(struct proto_ta *ta, ...)
{
    va_list ap;
    va_start(ap, ta);

    ut_assert(ta->type->ia_type == proto_ia_type_multi_response &&
	      ta->state == proto_ta_state_accepted);

    struct msg *response = produce_response(ta, proto_msg_type_notify, ap);

    va_end(ap);

    return response;
}

struct msg *proto_ta_complete(struct proto_ta *ta, ...)
{
    va_list ap;
    va_start(ap, ta);

    if (ta->type->ia_type == proto_ia_type_single_response)
	ut_assert(ta->state == proto_ta_state_requested);
    else
	ut_assert(ta->state == proto_ta_state_accepted);

    struct msg *response = produce_response(ta, proto_msg_type_complete, ap);

    ta->state = proto_ta_state_completed;

    va_end(ap);

    return response;
}

struct msg *proto_ta_fail(struct proto_ta *ta, ...)
{
    va_list ap;
    va_start(ap, ta);

    ut_assert(!proto_ta_has_term(ta));

    struct msg *response = produce_response(ta, proto_msg_type_fail, ap);

    ta->state = proto_ta_state_failed;

    va_end(ap);

    return response;
}

bool proto_ta_has_term(struct proto_ta *ta)
{
    return ta->state == proto_ta_state_failed ||
	ta->state == proto_ta_state_completed;

}
