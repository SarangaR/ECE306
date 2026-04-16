#include "include/ringbuffer.h"
#include <string.h>

void rb_init(RingBuffer* rb) {
    rb->head = 0;
    rb->tail = 0;
    memset(rb->buffer, 0, sizeof(uint8_t) * BUFFER_SIZE);
}

inline void rb_push(RingBuffer* rb, uint8_t data) {
    uint8_t next = (rb->head + 1) & BUFFER_MASK;

    if (next == rb->tail) {
        return;
    }

    rb->buffer[rb->head] = data;
    rb->head = next;
}

inline uint8_t rb_pop(RingBuffer* rb, uint8_t* dest) {
    if (rb->head == rb->tail) {
        return 0;
    }

    *dest = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) & BUFFER_MASK;
    return 1;
}
