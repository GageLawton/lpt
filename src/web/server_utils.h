#pragma once

#include "../types.h"
#include <cstdio>
#include <cstring>
#include <string>

inline std::string mime_for(const std::string& path)
{
    auto ends = [&](const char* s) {
        size_t pl = path.size(), sl = strlen(s);
        return pl >= sl && path.compare(pl - sl, sl, s) == 0;
    };
    if (ends(".html")) return "text/html";
    if (ends(".css"))  return "text/css";
    if (ends(".js"))   return "application/javascript";
    if (ends(".jsx"))  return "application/javascript";
    return "application/octet-stream";
}

inline std::string json_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else                out += c;
    }
    return out;
}

inline std::string aircraft_to_json(const Aircraft* ac)
{
    char icao[7];
    snprintf(icao, sizeof(icao), "%06X", ac->icao);

    std::string cs = json_escape(ac->callsign[0] ? ac->callsign : "");
    char hdr[384];
    snprintf(hdr, sizeof(hdr),
        "{\"icao\":\"%s\",\"cs\":\"%s\","
        "\"lat\":%.6f,\"lon\":%.6f,"
        "\"alt\":%d,\"spd\":%.1f,\"hdg\":%.1f,"
        "\"vs\":%d,\"msgsRx\":%u,"
        "\"firstSeenMs\":%llu,\"lastSeenMs\":%llu,"
        "\"trail\":[",
        icao, cs.c_str(),
        ac->lat, ac->lon,
        (int)ac->altitude_ft,
        (double)ac->groundspeed_kt,
        (double)ac->heading_deg,
        (int)ac->vert_rate_fpm,
        (unsigned)ac->msgs_rx,
        (unsigned long long)ac->first_seen_ms,
        (unsigned long long)ac->last_seen_ms);

    std::string out = hdr;
    // Oldest entry is at (trail_head + TRAIL_MAX - trail_len) % TRAIL_MAX.
    int start = (ac->trail_head + TRAIL_MAX - ac->trail_len) % TRAIL_MAX;
    for (int i = 0; i < ac->trail_len; i++) {
        int idx = (start + i) % TRAIL_MAX;
        char pt[64];
        snprintf(pt, sizeof(pt), "%s{\"lat\":%.6f,\"lon\":%.6f}",
                 i > 0 ? "," : "", ac->trail[idx].lat, ac->trail[idx].lon);
        out += pt;
    }
    out += "]}";
    return out;
}
