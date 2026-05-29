#pragma once
#include "../types.h"
#include <cstdint>

// Create or update an Aircraft entry keyed by ICAO address.
// Updates last_seen_ms and returns a pointer to the entry (stable until next expire).
Aircraft* table_upsert(uint32_t icao, uint64_t now_ms);

// Returns the existing entry for icao, or nullptr if not present.
// Used for AP-parity frames (DF5/DF21) where ICAO is recovered from CRC
// and must already be known — otherwise random noise produces phantom aircraft.
Aircraft* table_lookup(uint32_t icao);

// Remove entries whose last_seen_ms is older than timeout_ms ago.
void table_expire(uint64_t now_ms, uint64_t timeout_ms);

// Call cb(ac, ctx) for every live aircraft.
void table_for_each(void (*cb)(const Aircraft*, void*), void* ctx);
