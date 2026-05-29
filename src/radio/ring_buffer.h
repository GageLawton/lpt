#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>

#define RING_BUFFER_SIZE (1 << 22) // 4MB

struct RingBuffer {
    uint8_t               data[RING_BUFFER_SIZE];
    std::atomic<uint32_t> head {0};
    std::atomic<uint32_t> tail {0};
};

void     rb_init(RingBuffer* rb);
bool     rb_push(RingBuffer* rb, const uint8_t* src, uint32_t len);
uint32_t rb_pop(RingBuffer* rb, uint8_t* dst, uint32_t max_len);
uint32_t rb_available(const RingBuffer* rb);
