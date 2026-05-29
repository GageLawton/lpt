#include <cassert>
#include <cstdio>
#include <cstring>
#include "decoder/modes.h"
#include "decoder/crc.h"
#include "types.h"

// Build a self-consistent 14-byte DF17 frame with correct CRC appended
static void make_df17_frame(uint8_t frame[14], uint32_t icao, const uint8_t me[7])
{
    frame[0] = (17 << 3);           // DF=17
    frame[1] = (icao >> 16) & 0xFF;
    frame[2] = (icao >>  8) & 0xFF;
    frame[3] =  icao        & 0xFF;
    memcpy(frame + 4, me, 7);
    uint32_t crc = modes_crc(frame, 11);
    frame[11] = (crc >> 16) & 0xFF;
    frame[12] = (crc >>  8) & 0xFF;
    frame[13] =  crc        & 0xFF;
}

int main()
{
    ModeSFrame f;
    const uint8_t me[7] = {0x08, 0x2C, 0xC3, 0x71, 0xC3, 0x2C, 0xE0}; // TC=1 ident

    // Test 1: valid 14-byte DF17 → parse returns true, ICAO correct
    {
        uint8_t frame[14];
        make_df17_frame(frame, 0x40621D, me);
        assert(modes_parse(frame, 14, &f));
        assert(f.icao == 0x40621Du);
        assert(modes_df(&f) == 17);
    }

    // Test 2: corrupted byte → CRC fails → returns false
    {
        uint8_t frame[14];
        make_df17_frame(frame, 0x40621D, me);
        frame[5] ^= 0xFF;
        assert(!modes_parse(frame, 14, &f));
    }

    // Test 3: valid 7-byte short frame (DF=11, no ICAO extraction)
    {
        uint8_t frame[7] = {(11 << 3), 0x40, 0x62, 0x1D, 0, 0, 0};
        uint32_t crc = modes_crc(frame, 4);
        frame[4] = (crc >> 16) & 0xFF;
        frame[5] = (crc >>  8) & 0xFF;
        frame[6] =  crc        & 0xFF;
        assert(modes_parse(frame, 7, &f));
        assert(modes_df(&f) == 11);
        assert(f.icao == 0u);  // non-DF17/18 → ICAO must not be extracted
    }

    // Test 4: invalid length (10 bytes) → returns false
    {
        uint8_t frame[14] = {};
        assert(!modes_parse(frame, 10, &f));
    }

    // Test 5: modes_df and modes_tc return correct values
    {
        uint8_t frame[14];
        make_df17_frame(frame, 0x40621D, me);
        assert(modes_parse(frame, 14, &f));
        assert(modes_df(&f) == 17);
        assert(modes_tc(&f) == 1);   // me[0]=0x08 → TC=(0x08>>3)&0x1F=1
    }

    // Test 6: DF18 frame — ICAO is extracted identically to DF17
    {
        uint8_t frame[14];
        make_df17_frame(frame, 0xABCDEF, me);
        frame[0] = (18 << 3);  // switch DF to 18
        uint32_t crc = modes_crc(frame, 11);
        frame[11] = (crc >> 16) & 0xFF;
        frame[12] = (crc >>  8) & 0xFF;
        frame[13] =  crc        & 0xFF;

        ModeSFrame f2;
        assert(modes_parse(frame, 14, &f2));
        assert(f2.icao == 0xABCDEFu);
    }

    // Test 7: 14-byte frame with DF != 17/18 — ICAO must not be extracted
    // from the body bytes. modes_parse may return true (CRC valid) or false;
    // either way ICAO must be zero.
    {
        uint8_t frame[14] = {};
        frame[0] = (21 << 3); // DF=21 (Comm-B identity reply)
        uint32_t crc = modes_crc(frame, 11);
        frame[11] = (crc >> 16) & 0xFF;
        frame[12] = (crc >>  8) & 0xFF;
        frame[13] =  crc        & 0xFF;

        ModeSFrame f3;
        bool ok = modes_parse(frame, 14, &f3);
        if (ok) assert(f3.icao == 0u);
    }

    printf("test_modes: all tests passed\n");
    return 0;
}
