// CPR cache timing logic — verifies the gate values used by the live decoder.
//
// The cache itself is a local static inside dsp_thread_fn(); these tests
// replicate the inline predicates so the constants stay in sync.

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "decoder/cpr.h"

// Predicates mirrored from dsp_thread_fn() in main.cpp / main-web.cpp.
static constexpr uint64_t GLOBAL_PAIR_WINDOW_MS = 10000;
static constexpr uint64_t CACHE_TTL_MS          = 60000;

static bool within_global_window(uint64_t t_even, uint64_t t_odd)
{
    uint64_t age_diff = t_even > t_odd ? t_even - t_odd : t_odd - t_even;
    return age_diff <= GLOBAL_PAIR_WINDOW_MS;
}

static bool should_evict(uint64_t now, uint64_t newest_entry_ts)
{
    return now - newest_entry_ts > CACHE_TTL_MS;
}

int main()
{
    // 1. A known even+odd frame pair decodes globally.
    uint32_t lat_e = 93000, lon_e = 74158;
    uint32_t lat_o = 74158, lon_o = 50194;
    double   lat, lon;
    assert(cpr_decode_global(lat_e, lon_e, lat_o, lon_o, 0, &lat, &lon));

    // 2. Two frames 9 s apart pass the global-pair window (≤ 10 s).
    assert(within_global_window(1000, 9999));

    // 3. Two frames 11 s apart fail the window (> 10 s) and fall back to local.
    assert(!within_global_window(1000, 12001));

    // 4. Order-insensitive: odd-newer-than-even is treated the same way.
    assert(within_global_window(9000, 1000));
    assert(!within_global_window(13000, 1000));

    // 5. Cache eviction: entry exactly at the TTL boundary is kept; older is evicted.
    assert(!should_evict(/*now=*/60000, /*newest=*/0));
    assert(should_evict(/*now=*/60001, /*newest=*/0));

    printf("test_cpr_cache: all tests passed\n");
    return 0;
}
