#include "velocity.h"

// payload[0..6] is the 7-byte ME field of a TC19 (airborne velocity) message
#include <cmath>

bool velocity_decode(const uint8_t* payload, float* speed_kt, float* heading_deg,
                     int32_t* vert_rate_fpm)
{
    uint8_t st = payload[0] & 0x07; // subtype
    if (st < 1 || st > 4) return false;

    // Vertical rate — same bit positions for all subtypes
    auto decode_vr = [&]() {
        int vr_sign = (payload[4] >> 2) & 1;
        int vr_val  = ((payload[4] & 0x03) << 7) | (payload[5] >> 1);
        if (vr_val == 0) {
            *vert_rate_fpm = 0;
            return;
        }
        vr_val--;
        *vert_rate_fpm = (vr_sign ? -1 : 1) * vr_val * 64;
    };

    if (st == 1 || st == 2) {
        // Ground speed (subtype 1 = subsonic, subtype 2 = supersonic, units ×4)
        int ew_dir = (payload[1] >> 2) & 1;
        int ew_vel = ((payload[1] & 0x03) << 8) | payload[2];
        int ns_dir = (payload[3] >> 7) & 1;
        int ns_vel = ((payload[3] & 0x7F) << 3) | (payload[4] >> 5);

        if (ew_vel == 0 || ns_vel == 0) return false; // 0 = not available
        ew_vel--;
        ns_vel--;
        if (st == 2) {
            ew_vel *= 4;
            ns_vel *= 4;
        } // supersonic: units are 4 kt
        if (ew_dir) ew_vel = -ew_vel;
        if (ns_dir) ns_vel = -ns_vel;

        float spd = sqrtf((float)(ew_vel * ew_vel + ns_vel * ns_vel));
        float hdg = atan2f((float)ew_vel, (float)ns_vel) * 180.0f / 3.14159265f;
        if (hdg < 0) hdg += 360.0f;

        *speed_kt    = spd;
        *heading_deg = hdg;
        decode_vr();
        return true;
    }

    // Subtype 3/4: magnetic heading + airspeed (IAS or TAS)
    int hdg_status = (payload[1] >> 2) & 1;
    if (hdg_status) {
        int hdg_raw  = ((payload[1] & 0x03) << 8) | payload[2];
        *heading_deg = hdg_raw * 360.0f / 1024.0f;
    } else {
        // Heading not available — signal sentinel so caller leaves ac->heading_deg alone.
        *heading_deg = NAN;
    }

    // as_type: 0 = IAS, 1 = TAS — both reported in kt via the same out-param
    int as_val = ((payload[3] & 0x7F) << 3) | (payload[4] >> 5);
    if (as_val == 0) return false; // not available
    as_val--;
    // subtype 4 is supersonic (×4) but realistically never seen; handle anyway
    *speed_kt = (st == 4) ? (float)(as_val * 4) : (float)as_val;
    decode_vr();
    return true;
}
