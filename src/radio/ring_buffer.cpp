#include "ring_buffer.h"
#include <algorithm>

void rb_init(RingBuffer* rb)
{
    rb->head.store(0, std::memory_order_relaxed);
    rb->tail.store(0, std::memory_order_relaxed);
}

bool rb_push(RingBuffer* rb, const uint8_t* src, uint32_t len)
{
    uint32_t h = rb->head.load(std::memory_order_relaxed);
    uint32_t t = rb->tail.load(std::memory_order_acquire);
    if (len > RING_BUFFER_SIZE - (h - t)) return false;
    for (uint32_t i = 0; i < len; i++)
        rb->data[(h + i) & (RING_BUFFER_SIZE - 1)] = src[i];
    rb->head.store(h + len, std::memory_order_release);
    return true;
}

uint32_t rb_pop(RingBuffer* rb, uint8_t* dst, uint32_t max_len)
{
    uint32_t t = rb->tail.load(std::memory_order_relaxed);
    uint32_t h = rb->head.load(std::memory_order_acquire);
    uint32_t len = std::min(h - t, max_len);
    for (uint32_t i = 0; i < len; i++)
        dst[i] = rb->data[(t + i) & (RING_BUFFER_SIZE - 1)];
    rb->tail.store(t + len, std::memory_order_release);
    return len;
}

uint32_t rb_available(const RingBuffer* rb)
{
    uint32_t h = rb->head.load(std::memory_order_acquire);
    uint32_t t = rb->tail.load(std::memory_order_relaxed);
    return h - t;
}
