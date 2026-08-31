#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint8_t *buffer;
    uint16_t size;
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer_t;

void RingBuffer_Init(RingBuffer_t *ringBuffer, uint8_t *buffer, uint16_t size);
bool RingBuffer_Put(RingBuffer_t *ringBuffer, uint8_t data);
bool RingBuffer_Get(RingBuffer_t *ringBuffer, uint8_t *data);
uint16_t RingBuffer_Available(const RingBuffer_t *ringBuffer);

#endif /* RING_BUFFER_H */
