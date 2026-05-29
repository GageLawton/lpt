#include <cstdio>
#include <cassert>
#include <cstdint>
#include <cstring>
#include "decoder/crc.h"

int main()
{
    // Self-consistency: CRC of (payload || CRC) must be zero.
    // Use an arbitrary 11-byte payload, compute its 3-byte CRC, build a 14-byte
    // frame, and verify the whole-frame CRC is 0.
    uint8_t frame[14]
        = {0x8D, 0x40, 0x62, 0x1D, 0x58, 0xC3, 0x82, 0xD6, 0x90, 0xC8, 0xAC, 0x00, 0x00, 0x00};

    uint32_t crc = modes_crc(frame, 11);
    frame[11]    = (crc >> 16) & 0xFF;
    frame[12]    = (crc >> 8) & 0xFF;
    frame[13]    = (crc >> 0) & 0xFF;

    assert(modes_crc(frame, 14) == 0);
    assert(modes_crc_valid(frame, 14));

    // Flip a payload byte — CRC must fail.
    uint8_t bad[14];
    memcpy(bad, frame, 14);
    bad[5] ^= 0x01;
    assert(!modes_crc_valid(bad, 14));

    // Identical-payload short frame (7 bytes).
    uint8_t  short_frame[7] = {0x5D, 0x3C, 0x4B, 0x5E, 0x00, 0x00, 0x00};
    uint32_t crc2           = modes_crc(short_frame, 4);
    short_frame[4]          = (crc2 >> 16) & 0xFF;
    short_frame[5]          = (crc2 >> 8) & 0xFF;
    short_frame[6]          = (crc2 >> 0) & 0xFF;
    assert(modes_crc_valid(short_frame, 7));

    printf("test_crc: all tests passed\n");
    return 0;
}
