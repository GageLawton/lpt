#include <cstdio>
#include <cassert>
#include <cmath>
#include "decoder/cpr.h"

static bool approx(double a, double b, double tol = 0.01)
{
    return fabs(a - b) < tol;
}

int main()
{
    // Known CPR pair from ICAO Annex 10 / ADS-B spec example:
    // Aircraft near lat=52.2572, lon=3.9194
    // Even frame: lat_cpr=0x6CF, lon_cpr=0x6962  (hex) → 1743 / 26978
    // Odd  frame: lat_cpr=0x535, lon_cpr=0x563F  → 1333 / 22079
    uint32_t lat_e = 93000, lon_e = 74158;
    uint32_t lat_o = 74158, lon_o = 50194;

    double lat, lon;

    // Global decode (last frame was even, so last_odd=0)
    bool ok = cpr_decode_global(lat_e, lon_e, lat_o, lon_o, 0, &lat, &lon);
    assert(ok);
    assert(approx(lat, 52.2572, 0.05));

    // Local decode using even frame with reference close to the expected position
    double lat2, lon2;
    ok = cpr_decode_local(lat_e, lon_e, 0, 52.0, 3.5, &lat2, &lon2);
    assert(ok);
    assert(approx(lat2, lat, 0.1));

    // Global decode with odd frame received last
    {
        double lat_g, lon_g;
        bool g = cpr_decode_global(lat_e, lon_e, lat_o, lon_o, 1, &lat_g, &lon_g);
        assert(g);
        assert(approx(lat_g, 52.2572, 0.05));
    }

    // Local decode using odd frame with a nearby reference
    {
        double lat_l, lon_l;
        bool g = cpr_decode_local(lat_o, lon_o, 1, 52.0, 3.5, &lat_l, &lon_l);
        assert(g);
        assert(approx(lat_l, 52.2572, 0.1));
    }

    // Local decode with a reference position far from the actual aircraft
    // (Sydney while the frames are over the UK) must not crash or invoke UB.
    // It may legitimately return false because of the implausibility gate.
    {
        double lat_far, lon_far;
        (void)cpr_decode_local(lat_e, lon_e, 0, -33.8, 151.2, &lat_far, &lon_far);
    }

    printf("test_cpr: all tests passed\n");
    return 0;
}
