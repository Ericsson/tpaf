/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include "utest.h"
#include "testutil.h"

#include "pqueue.h"

TESTSUITE(pqueue, NULL, NULL)

static int run_basic(void)
{
    struct pqueue *queue = pqueue_create();

    uintptr_t num_elems = 1 + tu_rand_max(1000);

    uintptr_t i;
    for (i = 0; i < num_elems; i++)
	pqueue_push(queue, (void *)i);

    uintptr_t num_iter = 1 + tu_rand_max(1000);

    for (i = 0; i < num_iter; i++) {
	void *old_elem = pqueue_pop(queue);

	CHKINTEQ((uintptr_t)old_elem, i);

	void *new_elem = (void *)(num_elems + i);
	pqueue_push(queue, new_elem);

	CHKINTEQ(num_elems, pqueue_len(queue));
    }

    for (i = 0; i < num_elems; i++) {
	void *old_elem = pqueue_pop(queue);

	CHKINTEQ((uintptr_t)old_elem, i + num_iter);
    }

    CHKINTEQ(0, pqueue_len(queue));

    CHK(pqueue_pop(queue) == NULL);

    /* test logic around saving memory on empty queue */
    void *elem = (void *)42;
    pqueue_push(queue, elem);

    CHKINTEQ(1, pqueue_len(queue));

    CHK(pqueue_pop(queue) == elem);
    
    pqueue_destroy(queue);

    return UTEST_SUCCESS;
}

#define BASIC_ITERATIONS (100)

TESTCASE(pqueue, basic)
{
    int i;

    for (i = 0; i < BASIC_ITERATIONS; i++)
	if (run_basic() != UTEST_SUCCESS)
	    return UTEST_FAILED;

    return UTEST_SUCCESS;
}
