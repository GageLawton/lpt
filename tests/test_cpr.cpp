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

    // Global decode (last frame was odd, so last_odd=1)
    {
        double lat_odd_decoded, lon_odd_decoded;
        ok = cpr_decode_global(lat_e, lon_e, lat_o, lon_o, 1, &lat_odd_decoded, &lon_odd_decoded);
        assert(ok);
        assert(approx(lat_odd_decoded, 52.2572, 0.05));
    }

    // Local decode using odd frame with reference close to the expected position
    {
        double lat_odd_local, lon_odd_local;
        ok = cpr_decode_local(lat_o, lon_o, 1, 52.0, 3.5, &lat_odd_local, &lon_odd_local);
        assert(ok);
        assert(approx(lat_odd_local, 52.2572, 0.1));
    }

    // Far reference should still decode or return false gracefully without crashing.
    {
        double lat_far = 0.0;
        double lon_far = 0.0;
        ok = cpr_decode_local(lat_e, lon_e, 0, -33.8, 151.2, &lat_far, &lon_far);
        (void)ok;
    }

    printf("test_cpr: all tests passed\n");
    return 0;
}
