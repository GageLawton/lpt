// End-to-end smoke test: feed a real DF17 hex frame through the full decode
// pipeline (modes_parse → modes_tc → cpr_decode_local / velocity_decode /
// callsign_decode / altitude_decode_gillham) and verify the decoded values
// match pyModeS ground truth.
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>

#include "decoder/modes.h"
#include "decoder/crc.h"
#include "decoder/cpr.h"
#include "decoder/altitude.h"
#include "decoder/velocity.h"
#include "decoder/callsign.h"

// Hex string → byte array; returns byte count
static int hex2bytes(const char* hex, uint8_t* out)
{
    int n = 0;
    while (hex[0] && hex[1]) {
        unsigned v = 0;
        sscanf(hex, "%02x", &v);
        out[n++] = (uint8_t)v;
        hex += 2;
    }
    return n;
}

static bool approx(double a, double b, double tol)
{
    return fabs(a - b) < tol;
}

int main()
{
    // ── TC 11: callsign / identification ──────────────────────────────────
    // Frame: 8D4840D6202CC371C32CE0576098 (from pyModeS test vectors)
    // ICAO: 4840D6, TC: 4, Callsign: "KLM1023 "
    {
        uint8_t raw[14];
        int     n = hex2bytes("8D4840D6202CC371C32CE0576098", raw);
        assert(n == 14);

        ModeSFrame frame {};
        assert(modes_parse(raw, 14, &frame));
        assert(frame.icao == 0x4840D6);

        uint8_t tc = modes_tc(&frame);
        assert(tc >= 1 && tc <= 4);

        const uint8_t* me    = frame.data + 4;
        char           cs[9] = {};
        assert(callsign_decode(me, cs));
        // Verify first 4 chars are "KLM1"
        assert(cs[0] == 'K' && cs[1] == 'L' && cs[2] == 'M' && cs[3] == '1');
    }

    // ── TC 17: airborne position ───────────────────────────────────────────
    // Frame: 8D40621D58C382D690C8AC2863A7 (from pyModeS test vectors)
    // ICAO: 40621D, TC: 11, odd=0, lat_cpr=93000, lon_cpr=51372
    // Using known reference position (52.258, 3.918) → ~52.258°N, 3.919°E
    {
        uint8_t raw[14];
        int     n = hex2bytes("8D40621D58C382D690C8AC2863A7", raw);
        assert(n == 14);

        ModeSFrame frame {};
        assert(modes_parse(raw, 14, &frame));
        assert(frame.icao == 0x40621D);

        uint8_t tc = modes_tc(&frame);
        assert(tc >= 9 && tc <= 18);

        const uint8_t* me  = frame.data + 4;
        int            odd = (me[2] >> 2) & 1;
        uint32_t lat_cpr = ((uint32_t)(me[2] & 0x03) << 15) | ((uint32_t)me[3] << 7) | (me[4] >> 1);
        uint32_t lon_cpr = ((uint32_t)(me[4] & 0x01) << 16) | ((uint32_t)me[5] << 8) | me[6];

        assert(odd == 0);
        assert(lat_cpr == 93000);
        assert(lon_cpr == 51372);

        // Local decode with reference near the Netherlands
        double lat, lon;
        assert(cpr_decode_local(lat_cpr, lon_cpr, odd, 52.258, 3.918, &lat, &lon));
        assert(approx(lat, 52.257, 0.05));
        assert(approx(lon, 3.919, 0.05));

        // Altitude
        uint16_t alt_raw = ((uint16_t)(me[1] & 0xFF) << 4) | (me[2] >> 4);
        int32_t  alt     = altitude_decode_gillham(alt_raw);
        // Should decode to a valid altitude (not INT32_MIN)
        assert(alt != INT32_MIN);
        // Expect roughly 38000 ft (FL380)
        assert(alt > 30000 && alt < 45000);
    }

    // ── TC 19: airborne velocity (subtype 1) ──────────────────────────────
    // 100 kt due east → heading ≈ 90°, verified against velocity.cpp unit test
    {
        uint8_t me[] = {0x99, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00};
        float   spd, hdg;
        int32_t vr;
        assert(velocity_decode(me, &spd, &hdg, &vr));
        assert(fabs(spd - 100.0f) < 1.0f);
        assert(fabs(hdg - 90.0f) < 1.0f);
        assert(vr == 0);
    }

    // ── CRC: corrupt frame rejected ───────────────────────────────────────
    {
        uint8_t raw[14];
        hex2bytes("8D40621D58C382D690C8AC2863A7", raw);
        raw[12] ^= 0xFF; // corrupt last bytes
        ModeSFrame frame {};
        assert(!modes_parse(raw, 14, &frame));
    }

    // ── cpr_decode_local: implausible position rejected ───────────────────
    {
        // CPR values that would decode far from the reference
        uint32_t lat_cpr = 0, lon_cpr = 0;
        double   lat, lon;
        // Reference in San Francisco; decoded position would be near equator
        bool ok = cpr_decode_local(lat_cpr, lon_cpr, 0, 37.7749, -122.4194, &lat, &lon);
        // If the decoded position is >3° from reference it must be rejected
        if (ok) {
            assert(fabs(lat - 37.7749) < 3.0);
            assert(fabs(lon - (-122.4194)) < 3.0);
        }
    }

    printf("test_e2e_decode: all tests passed\n");
    return 0;
}
