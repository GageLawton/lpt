#pragma once
#include <cstdint>

struct SurfacePos {
    double lat;
    double lon;
    float  heading_deg;  // -1 if heading status bit not set
    float  speed_kt;
};

// Decode a TC 5-8 surface position ME field.
// ref_lat/ref_lon is the receiver position used as a CPR reference.
// Returns true on success, false if CPR decode fails plausibility check.
bool surface_decode(const uint8_t* me, double ref_lat, double ref_lon,
                    SurfacePos* out);
