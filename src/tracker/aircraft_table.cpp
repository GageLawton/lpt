#include "aircraft_table.h"
#include <unordered_map>
#include <cstring>

static std::unordered_map<uint32_t, Aircraft> s_table;

Aircraft* table_upsert(uint32_t icao, uint64_t now_ms)
{
    auto [it, inserted] = s_table.emplace(icao, Aircraft {});
    Aircraft& ac        = it->second;
    if (inserted) {
        ac.icao          = icao;
        ac.first_seen_ms = now_ms;
        ac.trail_len     = 0;
        ac.trail_head    = 0;
    }
    ac.last_seen_ms = now_ms;
    ac.msgs_rx++;
    return &ac;
}

Aircraft* table_lookup(uint32_t icao)
{
    auto it = s_table.find(icao);
    return it == s_table.end() ? nullptr : &it->second;
}

void table_expire(uint64_t now_ms, uint64_t timeout_ms)
{
    for (auto it = s_table.begin(); it != s_table.end();) {
        if (now_ms - it->second.last_seen_ms > timeout_ms) {
            it = s_table.erase(it);
        } else {
            ++it;
        }
    }
}

void table_for_each(void (*cb)(const Aircraft*, void*), void* ctx)
{
    for (const auto& kv : s_table) { cb(&kv.second, ctx); }
}
