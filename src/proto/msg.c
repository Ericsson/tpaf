/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#include <stdlib.h>
#include <string.h>

#include "util.h"

#include "msg.h"

struct msg
{
    void *data;
    size_t len;
};

struct msg *msg_create(const void *data, size_t len)
{
    struct msg *msg = ut_malloc(sizeof(struct msg));

    msg->data = ut_memdup(data, len);
    msg->len = len;

    return msg;
}

struct msg *msg_create_prealloc(void *data, size_t len)
{
    struct msg *msg = ut_malloc(sizeof(struct msg));

    msg->data = data;
    msg->len = len;

    return msg;
}

void msg_destroy(struct msg *msg)
{
    if (msg != NULL) {
        ut_free(msg->data);
        ut_free(msg);
    }
}

const void *msg_data(const struct msg *msg)
{
    return msg->data;
}

size_t msg_len(const struct msg *msg)
{
    return msg->len;
}
