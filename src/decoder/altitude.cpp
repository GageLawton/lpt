#include "altitude.h"
#include <cstdint>
#include <climits>

// Gray-code to binary helper
static uint32_t gray_to_bin(uint32_t g)
{
    uint32_t b = 0;
    for (; g; g >>= 1) b ^= g;
    return b;
}

int32_t altitude_decode_gillham(uint16_t raw)
{
    // 13-bit Gillham: bits C1 A1 C2 A2 C4 A4 [M] B1 [Q] B2 D2 B4 D4
    // Q-bit (bit 4) selects encoding: 1 = 25ft increments, 0 = Gillham Gray
    int C1 = (raw >> 12) & 1;
    int A1 = (raw >> 11) & 1;
    int C2 = (raw >> 10) & 1;
    int A2 = (raw >> 9) & 1;
    int C4 = (raw >> 8) & 1;
    int A4 = (raw >> 7) & 1;
    int B1 = (raw >> 5) & 1;
    int Q  = (raw >> 4) & 1;
    int B2 = (raw >> 3) & 1;
    int D2 = (raw >> 2) & 1;
    int B4 = (raw >> 1) & 1;
    int D4 = (raw >> 0) & 1;

    if (Q) {
        // 25 ft encoding: remaining 11 bits as binary offset from -1200
        int n = (raw & ~0x10) >> 1; // strip Q bit, shift
        return (int32_t)n * 25 - 1200;
    }

    // Gillham Gray code: interleaved 500ft and 100ft gray codes
    uint32_t grayC = (C1 << 2) | (C2 << 1) | C4;
    uint32_t grayA = (A1 << 2) | (A2 << 1) | A4;
    uint32_t grayB = (B1 << 2) | (B2 << 1) | B4;
    uint32_t grayD = (D2 << 1) | D4;

    uint32_t D500 = gray_to_bin((grayC << 3) | grayA);
    uint32_t D100 = gray_to_bin((grayB << 2) | grayD);

    if (D100 == 0 || D100 == 5 || D100 == 6) return INT32_MIN;
    if (D100 == 7) D100 = 5;
    if (D500 & 1) D100 = 6 - D100;

    int32_t alt = (int32_t)(D500 * 500 + D100 * 100) - 1300;
    return alt;
}
