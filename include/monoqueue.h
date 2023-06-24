#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * One-way queue optimized for static size and minimal operation complexity.
 * Single-shot design for transfering data between layers in an asynchronous
 * manner. Re-initialize to reset.
 * Not MT safe.
 */

typedef struct {
    const uint_least8_t *head;
    const uint_least8_t *limit;
} mqueue_t;

#define MQUEUE_EMPTY(q) ((q)->head == (q)->limit)

/**
 * Initialize a queue.
 * @param self the queue
 * @param buffer the location of the underlying buffer
 * @param size the size of the underlying buffer, in bytes
 * @return pointer to the initialized queue.
 */
mqueue_t *mqueue_init(mqueue_t *self, const uint_least8_t *buf, size_t size);


/**
 * Get the first element of the queue
 * @param self the queue
 * @return first element in the queue
 */
uint_least8_t mqueue_pop(mqueue_t *self);

/**
 * View the last element without removal.
 * @param self the queue
 * @return value on top of queue.
 */
uint_least8_t mqueue_peek(mqueue_t *self);
