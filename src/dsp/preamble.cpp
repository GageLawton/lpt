#include "preamble.h"

// ADS-B preamble at 2MSPS: high chips at indices 0,2,7,9; low elsewhere (16 samples total)
static const int PREAMBLE_HI[4] = {0, 2, 7, 9};
static const int PREAMBLE_LO[5] = {1, 3, 4, 6, 8};

int preamble_search(const float* mag, uint32_t len)
{
    if (len < 16) return -1;

    for (uint32_t i = 0; i <= len - 16; i++) {
        float hi = (mag[i + 0] + mag[i + 2] + mag[i + 7] + mag[i + 9]) * 0.25f;
        float lo = (mag[i + 1] + mag[i + 3] + mag[i + 4] + mag[i + 6] + mag[i + 8]) * 0.2f;

        if (hi <= lo || hi < 2.0f * lo) continue;

        float thresh = (hi + lo) * 0.5f;
        bool  ok     = true;
        for (int k = 0; k < 4 && ok; k++)
            if (mag[i + PREAMBLE_HI[k]] < thresh) ok = false;
        for (int k = 0; k < 5 && ok; k++)
            if (mag[i + PREAMBLE_LO[k]] > thresh) ok = false;
        if (ok) return (int)i;
    }
    return -1;
}
