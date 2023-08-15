#include "util.h"

#include "pqueue.h"

struct pqueue
{
    void **elems;
    size_t capacity;
    size_t start_idx;
    size_t len;
};

#define CAPACITY_INCREASE_FACTOR (2)
#define MAX_SPARE_CAPACITY (128)

static void assure_capacity(struct pqueue* queue, size_t min_capacity)
{
    if (queue->capacity >= min_capacity)
	return;

    size_t new_capacity = min_capacity * CAPACITY_INCREASE_FACTOR;
    void **new_elems = ut_malloc(sizeof(void *) * new_capacity);

    size_t new_idx;
    for (new_idx = 0; new_idx < queue->len; new_idx++) {
	size_t old_idx = (queue->start_idx + new_idx) % queue->capacity;
	new_elems[new_idx] = queue->elems[old_idx];
    }

    queue->start_idx = 0;
    queue->capacity = new_capacity;

    ut_free(queue->elems);

    queue->elems = new_elems;
}

struct pqueue *pqueue_create(void)
{
    return ut_calloc(sizeof(struct pqueue));
}

void pqueue_destroy(struct pqueue *queue)
{
    if (queue != NULL) {
	ut_free(queue->elems);
	ut_free(queue);
    }
}

void pqueue_push(struct pqueue *queue, void *elem)
{
    assure_capacity(queue, queue->len + 1);

    size_t last_idx = (queue->start_idx + queue->len) % queue->capacity;

    queue->elems[last_idx] = elem;

    queue->len++;
}

static void consider_compacting(struct pqueue *queue)
{
    if (queue->len == 0 && queue->capacity > MAX_SPARE_CAPACITY) {
	ut_free(queue->elems);
	queue->elems = NULL;
	queue->capacity = 0;
	queue->start_idx = 0;
    }
}

void *pqueue_pop(struct pqueue *queue)
{
    if (queue->len == 0)
	return NULL;

    void *elem = queue->elems[queue->start_idx];

    queue->start_idx = (queue->start_idx + 1) % queue->capacity;

    queue->len--;

    consider_compacting(queue);

    return elem;
}

void *pqueue_peek(struct pqueue *queue)
{
    if (queue->len == 0)
	return NULL;

    return queue->elems[queue->start_idx];
}

size_t pqueue_len(const struct pqueue *queue)
{
    return queue->len;
}
