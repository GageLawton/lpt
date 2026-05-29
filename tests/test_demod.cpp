#include <cstdio>
#include <cassert>
#include <cstdint>
#include "dsp/demod.h"

int main()
{
    // Build a synthetic magnitude buffer encoding bits: 1 0 1 1 0
    // Each bit = 4 samples; bit=1 means first half > second half.
    const int BITS = 5;
    float mag[BITS * 4];
    const uint8_t pattern[BITS] = {1, 0, 1, 1, 0};

    for (int i = 0; i < BITS; i++) {
        float hi = 1.0f, lo = 0.2f;
        mag[i*4+0] = pattern[i] ? hi : lo;
        mag[i*4+1] = pattern[i] ? hi : lo;
        mag[i*4+2] = pattern[i] ? lo : hi;
        mag[i*4+3] = pattern[i] ? lo : hi;
    }

    uint8_t dst[112] = {};
    int n = demod_ook(mag, BITS * 4, dst);

    assert(n == BITS);
    for (int i = 0; i < BITS; i++)
        assert(dst[i] == pattern[i]);

    // Input shorter than one bit period (< 4 samples) → returns 0
    {
        float tiny[3] = {1.0f, 1.0f, 0.2f};
        uint8_t out[112] = {};
        assert(demod_ook(tiny, 3, out) == 0);
    }

    // Exactly 56 bits (short Mode S frame)
    {
        float mag56[56 * 4];
        uint8_t expected[56];
        for (int i = 0; i < 56; i++) {
            expected[i] = i % 2;
            float hi = 1.0f, lo = 0.2f;
            mag56[i*4+0] = expected[i] ? hi : lo;
            mag56[i*4+1] = expected[i] ? hi : lo;
            mag56[i*4+2] = expected[i] ? lo : hi;
            mag56[i*4+3] = expected[i] ? lo : hi;
        }
        uint8_t out[112] = {};
        int got = demod_ook(mag56, 56 * 4, out);
        assert(got == 56);
        for (int i = 0; i < 56; i++) assert(out[i] == expected[i]);
    }

    // Exactly 112 bits (long Mode S frame) — verify cap at 112
    {
        float mag112[112 * 4];
        uint8_t expected[112];
        for (int i = 0; i < 112; i++) {
            expected[i] = (i / 8) % 2;
            float hi = 1.0f, lo = 0.2f;
            mag112[i*4+0] = expected[i] ? hi : lo;
            mag112[i*4+1] = expected[i] ? hi : lo;
            mag112[i*4+2] = expected[i] ? lo : hi;
            mag112[i*4+3] = expected[i] ? lo : hi;
        }
        uint8_t out[112] = {};
        int got = demod_ook(mag112, 112 * 4, out);
        assert(got == 112);
        for (int i = 0; i < 112; i++) assert(out[i] == expected[i]);
    }

    printf("test_demod: all tests passed\n");
    return 0;
}
