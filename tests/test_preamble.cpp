#include <cassert>
#include <cstdio>
#include <cstring>
#include "dsp/preamble.h"

static void make_preamble(float* buf, uint32_t offset) {
    // high chips at 0,2,7,9; low everywhere else (16 samples)
    const int hi_idx[] = {0,2,7,9};
    for (int i = 0; i < 16; i++) buf[offset+i] = 0.5f;  // low
    for (int i = 0; i < 4; i++) buf[offset + hi_idx[i]] = 5.0f;  // high
}

int main()
{
    // Test 1: clean preamble at offset 0
    {
        float buf[64] = {};
        make_preamble(buf, 0);
        assert(preamble_search(buf, 16) == 0);
    }

    // Test 2: preamble buried at offset 20
    {
        float buf[64] = {};
        for (int i = 0; i < 64; i++) buf[i] = 0.1f;  // background noise
        make_preamble(buf, 20);
        assert(preamble_search(buf, 64) == 20);
    }

    // Test 3: all-zero buffer → -1
    {
        float buf[64];
        memset(buf, 0, sizeof(buf));
        assert(preamble_search(buf, 64) == -1);
    }

    // Test 4: buffer shorter than 16 samples → -1
    {
        float buf[64] = {};
        make_preamble(buf, 0);
        assert(preamble_search(buf, 15) == -1);
    }

    // Test 5: hi/lo swapped → -1 (low at 0,2,7,9; high elsewhere)
    {
        float buf[64] = {};
        for (int i = 0; i < 16; i++) buf[i] = 5.0f;   // all high
        buf[0] = buf[2] = buf[7] = buf[9] = 0.5f;      // hi indices are now low
        assert(preamble_search(buf, 16) == -1);
    }

    // Test 6: two valid preambles — earliest is returned
    {
        float buf[64] = {};
        for (int i = 0; i < 64; i++) buf[i] = 0.1f;
        make_preamble(buf, 0);
        make_preamble(buf, 20);
        assert(preamble_search(buf, 64) == 0);
    }

    // Test 7: preamble at the last valid start position (len − 16)
    {
        float buf[64] = {};
        for (int i = 0; i < 64; i++) buf[i] = 0.1f;
        make_preamble(buf, 48); // 64 - 16 = 48
        assert(preamble_search(buf, 64) == 48);
    }

    printf("test_preamble: all tests passed\n");
    return 0;
}
