#include "callsign.h"
#include <cstring>

static const char CHARSET[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ#####"
                              " ###############0123456789######";

bool callsign_decode(const uint8_t* me, char out[9])
{
    uint8_t tc = (me[0] >> 3) & 0x1F;
    if (tc < 1 || tc > 4) return false;

    uint64_t bits = ((uint64_t)me[1] << 40) | ((uint64_t)me[2] << 32) | ((uint64_t)me[3] << 24)
        | ((uint64_t)me[4] << 16) | ((uint64_t)me[5] << 8) | ((uint64_t)me[6]);

    for (int i = 0; i < 8; i++) out[i] = CHARSET[(bits >> (42 - i * 6)) & 0x3F];
    out[8] = '\0';

    for (int i = 7; i >= 0 && out[i] == ' '; i--) out[i] = '\0';
    return true;
}
