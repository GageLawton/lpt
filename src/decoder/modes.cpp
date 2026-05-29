#include "modes.h"
#include "crc.h"
#include <cstring>

bool modes_parse(const uint8_t* raw, uint8_t len, ModeSFrame* out)
{
    if (len != 7 && len != 14) return false;
    if (!modes_crc_valid(raw, len)) return false;

    memcpy(out->data, raw, len);
    out->len = len;

    // ICAO address is bytes 1-3 for DF17 (extended squitter)
    uint8_t df = (raw[0] >> 3) & 0x1F;
    if (df == 17 || df == 18) {
        out->icao = ((uint32_t)raw[1] << 16) | ((uint32_t)raw[2] << 8) | (uint32_t)raw[3];
    } else {
        out->icao = 0;
    }
    out->timestamp_ms = 0;
    return true;
}

uint8_t modes_df(const ModeSFrame* frame)
{
    return (frame->data[0] >> 3) & 0x1F;
}
uint8_t modes_tc(const ModeSFrame* frame)
{
    return (frame->data[4] >> 3) & 0x1F;
}
