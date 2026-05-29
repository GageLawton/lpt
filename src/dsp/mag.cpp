#include "mag.h"
#include <cmath>

void iq_to_mag(const uint8_t* iq, uint32_t iq_len, float* mag_out)
{
    if (iq_len == 0) return;
    for (uint32_t i = 0; i < iq_len / 2; i++) {
        float I    = (float)iq[2 * i] - 127.5f;
        float Q    = (float)iq[2 * i + 1] - 127.5f;
        mag_out[i] = hypotf(I, Q);
    }
}
