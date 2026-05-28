#include "cpr.h"
#include <cmath>

static const double kPi = 3.14159265358979323846;

// Number of longitude zones for a given latitude
static int nl(double lat)
{
    if (lat < 0) lat = -lat;
    if (lat >= 87.0) return 1;
    if (lat >= 86.536) return 2;
    double a = 1.0 - cos(kPi / (2.0 * 15.0));
    double b = cos(kPi / 180.0 * lat);
    return (int)floor(2.0 * kPi / acos(1.0 - a / (b * b)));
}

static double mod(double a, double b)
{
    return a - b * floor(a / b);
}

bool cpr_decode_global(uint32_t lat_even, uint32_t lon_even,
                       uint32_t lat_odd,  uint32_t lon_odd,
                       int last_odd, double* lat_out, double* lon_out)
{
    const double dLat_even = 360.0 / 60.0;
    const double dLat_odd  = 360.0 / 59.0;
    const double scale     = 1.0 / 131072.0; // 2^17

    double rlat_e = (double)lat_even * scale;
    double rlat_o = (double)lat_odd  * scale;

    int j = (int)floor(59.0 * rlat_e - 60.0 * rlat_o + 0.5);

    double lat_e = dLat_even * (mod(j, 60) + rlat_e);
    double lat_o = dLat_odd  * (mod(j, 59) + rlat_o);

    if (lat_e >= 270.0) lat_e -= 360.0;
    if (lat_o >= 270.0) lat_o -= 360.0;

    if (nl(lat_e) != nl(lat_o))
        return false;

    double lat = last_odd ? lat_o : lat_e;
    *lat_out = lat;

    int nl_val = nl(lat);
    int ni_e = nl_val     > 1 ? nl_val     : 1;
    int ni_o = nl_val - 1 > 1 ? nl_val - 1 : 1;

    double rlon_e = (double)lon_even * scale;
    double rlon_o = (double)lon_odd  * scale;
    double dLon_e = 360.0 / ni_e;
    double dLon_o = 360.0 / ni_o;

    int m = (int)floor(rlon_e * (nl_val - 1) - rlon_o * nl_val + 0.5);

    double lon;
    if (last_odd)
        lon = dLon_o * (mod(m, ni_o) + rlon_o);
    else
        lon = dLon_e * (mod(m, ni_e) + rlon_e);

    if (lon >= 180.0) lon -= 360.0;
    *lon_out = lon;
    return true;
}

bool cpr_decode_local(uint32_t lat_cpr, uint32_t lon_cpr, int odd,
                      double ref_lat, double ref_lon,
                      double* lat_out, double* lon_out,
                      bool surface)
{
    const double scale = 1.0 / 131072.0;
    // Surface CPR uses 90° zone range; airborne uses 360°
    const double zone = surface ? 90.0 : 360.0;
    double dLat = odd ? zone / 59.0 : zone / 60.0;

    double rlat = (double)lat_cpr * scale;
    int j = (int)floor(ref_lat / dLat) + (int)floor(mod(ref_lat, dLat) / dLat - rlat + 0.5);
    double lat = dLat * (j + rlat);

    int nl_val = nl(lat);
    int ni = (nl_val - odd) > 1 ? (nl_val - odd) : 1;
    double dLon = 360.0 / ni;

    double rlon = (double)lon_cpr * scale;
    int m = (int)floor(ref_lon / dLon) + (int)floor(mod(ref_lon, dLon) / dLon - rlon + 0.5);
    double lon = dLon * (m + rlon);

    if (lat >= 270.0) lat -= 360.0;
    if (lon >= 180.0) lon -= 360.0;

    // Plausibility: reject if decoded position is >3° (~180 NM) from reference
    double dlat = lat - ref_lat;
    double dlon = lon - ref_lon;
    if (dlon >  180.0) dlon -= 360.0;
    if (dlon < -180.0) dlon += 360.0;
    if (dlat > 3.0 || dlat < -3.0 || dlon > 3.0 || dlon < -3.0) return false;

    *lat_out = lat;
    *lon_out = lon;
    return true;
}
