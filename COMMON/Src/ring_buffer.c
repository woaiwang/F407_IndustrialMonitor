#include "ring_buffer.h"

#include <stddef.h>

static uint16_t RingBuffer_NextIndex(const RingBuffer_t *ringBuffer,
                                     uint16_t index)
{
    index++;

    if (index >= ringBuffer->size)
    {
        index = 0U;
    }

    return index;
}

void RingBuffer_Init(RingBuffer_t *ringBuffer, uint8_t *buffer, uint16_t size)
{
    if (ringBuffer == NULL)
    {
        return;
    }

    ringBuffer->buffer = buffer;
    ringBuffer->size = size;
    ringBuffer->head = 0U;
    ringBuffer->tail = 0U;
}

bool RingBuffer_Put(RingBuffer_t *ringBuffer, uint8_t data)
{
    uint16_t nextHead;

    if ((ringBuffer == NULL) || (ringBuffer->buffer == NULL) ||
        (ringBuffer->size < 2U))
    {
        return false;
    }

    nextHead = RingBuffer_NextIndex(ringBuffer, ringBuffer->head);

    if (nextHead == ringBuffer->tail)
    {
        return false;
    }

    ringBuffer->buffer[ringBuffer->head] = data;
    ringBuffer->head = nextHead;

    return true;
}

bool RingBuffer_Get(RingBuffer_t *ringBuffer, uint8_t *data)
{
    uint16_t nextTail;

    if ((ringBuffer == NULL) || (ringBuffer->buffer == NULL) ||
        (data == NULL) || (ringBuffer->size < 2U))
    {
        return false;
    }

    if (ringBuffer->head == ringBuffer->tail)
    {
        return false;
    }

    *data = ringBuffer->buffer[ringBuffer->tail];
    nextTail = RingBuffer_NextIndex(ringBuffer, ringBuffer->tail);
    ringBuffer->tail = nextTail;

    return true;
}

uint16_t RingBuffer_Available(const RingBuffer_t *ringBuffer)
{
    uint16_t head;
    uint16_t tail;

    if ((ringBuffer == NULL) || (ringBuffer->buffer == NULL) ||
        (ringBuffer->size < 2U))
    {
        return 0U;
    }

    head = ringBuffer->head;
    tail = ringBuffer->tail;

    if (head >= tail)
    {
        return head - tail;
    }

    return ringBuffer->size - tail + head;
}
