#pragma once
#include <cstdint>

// Decode globally using an even and odd CPR frame pair
// Returns true on success
bool cpr_decode_global(uint32_t lat_even, uint32_t lon_even,
                       uint32_t lat_odd,  uint32_t lon_odd,
                       int      last_odd, // 1 if odd frame arrived last
                       double*  lat_out,  double* lon_out);

// Decode locally using a single frame and a reference position.
// Set surface=true for TC 5-8 surface position messages (smaller CPR zone).
// Returns false if decoded position is implausibly far from reference (>3°).
bool cpr_decode_local(uint32_t lat_cpr, uint32_t lon_cpr, int odd,
                      double ref_lat, double ref_lon,
                      double* lat_out, double* lon_out,
                      bool surface = false);
