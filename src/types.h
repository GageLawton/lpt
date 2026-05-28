#pragma once
#include <cstdint>

static const int TRAIL_MAX = 100;

struct LatLon { double lat; double lon; };

// Raw IQ sample from RTL-SDR
struct IQSample {
    int16_t i;
    int16_t q;
};

// Decoded Mode S frame
struct ModeSFrame {
    uint8_t  data[14];      // raw payload bytes
    uint8_t  len;           // 7 (short) or 14 (long squitter)
    uint32_t icao;          // 24-bit ICAO aircraft address
    uint64_t timestamp_ms;  // time of reception
};

// Tracked aircraft state
struct Aircraft {
    uint32_t icao;              // unique 24-bit identifier
    char     callsign[9];       // 8 chars + null terminator
    uint8_t  emitter_cat;       // ADS-B emitter category (0 = unknown)
    uint16_t squawk;            // Mode A squawk code as decimal (e.g. 7700); 0 = not set
    uint8_t  emergency_state;   // TC28 emergency: 0=none 1=general 2=medical 3=minfuel
                                //                 4=nordo 5=hijack 6=downed
    double   lat;               // decimal degrees
    double   lon;               // decimal degrees
    int32_t  altitude_ft;       // pressure altitude in feet
    float    groundspeed_kt;    // knots
    float    heading_deg;       // 0-360 degrees
    int32_t  vert_rate_fpm;     // vertical rate in feet per minute
    uint64_t first_seen_ms;     // timestamp of first received message
    uint64_t last_seen_ms;      // timestamp of last received message
    uint32_t msgs_rx;           // count of received messages
    bool     position_valid;    // true once CPR decode has succeeded
    LatLon   trail[TRAIL_MAX];  // position history (circular buffer)
    int      trail_head;        // next write index
    int      trail_len;         // valid entries (0..TRAIL_MAX)
};
