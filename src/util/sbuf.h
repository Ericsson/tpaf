/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef SBUF_H
#define SBUF_H

struct sbuf;

struct sbuf *sbuf_create(void);

char *sbuf_morph(struct sbuf *sbuf);
void sbuf_destroy(struct sbuf *sbuf);

void sbuf_append(struct sbuf *sbuf, const char *s);
void sbuf_append_c(struct sbuf *sbuf, char c);

#endif
