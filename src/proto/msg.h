/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#ifndef MSG_H
#define MSG_H

struct msg *msg_create(const void *data, size_t len);
struct msg *msg_create_prealloc(void *data, size_t len);
void msg_destroy(struct msg *msg);

const void *msg_data(const struct msg *msg);

size_t msg_len(const struct msg *msg);

#endif
