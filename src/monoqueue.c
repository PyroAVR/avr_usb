#include "monoqueue.h"

#include <limits.h>

mqueue_t *mqueue_init(mqueue_t *self, const uint_least8_t *buf, size_t size) {
    self->limit = buf + (size * (CHAR_BIT >> 3));
    self->head = buf;
    return self;
}

uint_least8_t mqueue_pop(mqueue_t *self) {
    uint_least8_t val = 0;
    if(!MQUEUE_EMPTY(self)) {
        val = *self->head;
        self->head++;
    }
    return val;
}

uint_least8_t mqueue_peek(mqueue_t *self) {
    uint_least8_t val = 0;
    if(!MQUEUE_EMPTY(self)) {
        val = *self->head;
    }
    return val;
}
