#include "demod.h"

// At 2MSPS each ADS-B bit is 2µs = 4 samples; compare first half vs second half
int demod_ook(const float* mag, uint32_t len, uint8_t* dst)
{
    const uint32_t SAMPLES_PER_BIT = 4;
    int            bits            = 0;
    uint32_t       n               = len / SAMPLES_PER_BIT;
    if (n > 112) n = 112;

    for (uint32_t i = 0; i < n; i++) {
        const float* b      = mag + i * SAMPLES_PER_BIT;
        float        first  = b[0] + b[1];
        float        second = b[2] + b[3];
        dst[bits++]         = (first > second) ? 1 : 0;
    }
    return bits;
}
