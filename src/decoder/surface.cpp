#include "surface.h"
#include "cpr.h"

// ME bit layout for TC 5-8 (surface position), 56 bits total:
//   bits  1- 5: TC (5-8)
//   bits  6-12: MOV (7-bit movement / speed code)
//   bit  13: S  (ground track / heading status)
//   bit  14: T/H (0=ground track, 1=magnetic heading)
//   bits 15-21: HDG (7-bit value, 128 steps → 360°)
//   bit  22: F  (CPR odd/even flag)
//   bits 23-39: LAT-CPR (17 bits)
//   bits 40-56: LON-CPR (17 bits)
//
// In bytes (me[0..6], MSB-first):
//   me[0]: TC[4:0] | MOV[6:4]
//   me[1]: MOV[3:0] | S | T/H | HDG[6:5]
//   me[2]: HDG[4:0] | F | LAT[16:15]
//   me[3]: LAT[15:8]
//   me[4]: LAT[7:0]   (LAT-CPR low 8 bits, but note: LAT is 17 bits total)
//   me[5]: LON[16:9]
//   me[6]: LON[8:1]   + LON[0] in me[4] bit 0 / me[5] msb arrangement
//
// Verified against pyModeS surface_position() bit indices (0-indexed MSB-first):
//   F    = mb[21]        → me[2] bit 2 (counting from LSB) = (me[2] >> 2) & 1
//   LAT  = mb[22:39]     → same formula as TC 9-18
//   LON  = mb[39:56]     → same formula as TC 9-18

// Movement code → approximate ground speed in knots
static float mov_to_kt(uint8_t mov)
{
    if (mov == 0)              return 0.0f;   // no info
    if (mov == 1)              return 0.0f;   // stopped
    if (mov <= 8)              return 0.125f * (mov - 1);
    if (mov <= 12)             return 1.0f  + 0.25f  * (mov -  9);
    if (mov <= 38)             return 2.0f  + 0.5f   * (mov - 13);
    if (mov <= 93)             return 15.0f + 1.0f   * (mov - 39);
    if (mov <= 108)            return 70.0f + 2.0f   * (mov - 94);
    if (mov <= 123)            return 100.0f + 5.0f  * (mov - 109);
    return 175.0f;
}

bool surface_decode(const uint8_t* me, double ref_lat, double ref_lon,
                    SurfacePos* out)
{
    uint8_t mov = ((me[0] & 0x07) << 4) | (me[1] >> 4);
    uint8_t hdg_status = (me[1] >> 3) & 1;
    uint8_t hdg_raw    = ((me[1] & 0x03) << 5) | (me[2] >> 3);

    int      odd     = (me[2] >> 2) & 1;
    uint32_t lat_cpr = ((uint32_t)(me[2] & 0x03) << 15)
                     | ((uint32_t)me[3] << 7)
                     |  (me[4] >> 1);
    uint32_t lon_cpr = ((uint32_t)(me[4] & 0x01) << 16)
                     | ((uint32_t)me[5] << 8)
                     |  me[6];

    double lat, lon;
    if (!cpr_decode_local(lat_cpr, lon_cpr, odd, ref_lat, ref_lon,
                          &lat, &lon, /*surface=*/true))
        return false;

    out->lat         = lat;
    out->lon         = lon;
    out->speed_kt    = mov_to_kt(mov);
    out->heading_deg = hdg_status ? (hdg_raw * 360.0f / 128.0f) : -1.0f;
    return true;
}
