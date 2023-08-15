/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef PQUEUE_H
#define PQUEUE_H

#include <stdbool.h>
#include <sys/types.h>

struct pqueue;

struct pqueue *pqueue_create(void);
void pqueue_destroy(struct pqueue *queue);

void pqueue_push(struct pqueue *queue, void *elem);
void *pqueue_pop(struct pqueue *queue);
void *pqueue_peek(struct pqueue *queue);

size_t pqueue_len(const struct pqueue *queue);

#define PQUEUE_GEN_WRAPPER_DEF(queue_name, queue_type, elem_type, fun_attrs) \
    queue_type;								\
									\
    fun_attrs queue_type *queue_name ## _create(void)			\
    {									\
	return (queue_type *)pqueue_create();				\
    }									\
									\
    fun_attrs void queue_name ## _destroy(queue_type *queue)		\
    {									\
	pqueue_destroy((struct pqueue *)queue);				\
    }									\
									\
    fun_attrs void queue_name ## _push(queue_type *queue, elem_type *elem) \
    {									\
	pqueue_push((struct pqueue *)queue, elem);			\
    }									\
									\
    fun_attrs elem_type *queue_name ## _pop(queue_type *queue)		\
    {									\
	return pqueue_pop((struct pqueue *)queue);			\
    }									\
									\
    fun_attrs elem_type *queue_name ## _peek(queue_type *queue)		\
    {									\
	return pqueue_peek((struct pqueue *)queue);			\
    }									\
									\
    fun_attrs size_t queue_name ## _len(const queue_type *queue)	\
    {									\
	return pqueue_len((const struct pqueue *)queue);		\
    }									\
									\

#endif
