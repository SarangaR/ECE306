#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include "msp430.h"
#include <stdint.h>

#define BUFFER_SIZE 256 // power of 2
#define BUFFER_MASK (BUFFER_SIZE - 1)

typedef struct {
    uint8_t buffer[BUFFER_SIZE];
    uint8_t head;  // Write index
    uint8_t tail;  // Read index
} RingBuffer;


void rb_init(RingBuffer *rb);
inline void rb_push(RingBuffer *rb, uint8_t data);
inline uint8_t rb_pop(RingBuffer *rb, uint8_t *dest);

#endif //RINGBUFFER_H