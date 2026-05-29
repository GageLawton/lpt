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
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    out += esc;
                } else {
                    out += (char)c;
                }
        }
    }
    return out;
}

inline std::string aircraft_to_json(const Aircraft* ac, uint64_t now_ms)
{
    char icao[7];
    snprintf(icao, sizeof(icao), "%06X", ac->icao);

    std::string cs = json_escape(ac->callsign[0] ? ac->callsign : "");
    // posAgeMs reflects only position freshness; resets on each successful CPR decode.
    // 0 if we have never decoded a position for this aircraft.
    uint64_t pos_age_ms = (ac->last_position_ms > 0 && now_ms > ac->last_position_ms)
                        ? now_ms - ac->last_position_ms : 0;

    char hdr[512];
    snprintf(hdr, sizeof(hdr),
        "{\"icao\":\"%s\",\"cs\":\"%s\","
        "\"lat\":%.6f,\"lon\":%.6f,\"posValid\":%s,"
        "\"alt\":%d,\"spd\":%.1f,\"hdg\":%.1f,"
        "\"vs\":%d,\"msgsRx\":%u,"
        "\"firstSeenMs\":%llu,\"lastSeenMs\":%llu,"
        "\"posAgeMs\":%llu,",
        icao, cs.c_str(),
        ac->lat, ac->lon, ac->position_valid ? "true" : "false",
        (int)ac->altitude_ft,
        (double)ac->groundspeed_kt,
        (double)ac->heading_deg,
        (int)ac->vert_rate_fpm,
        (unsigned)ac->msgs_rx,
        (unsigned long long)ac->first_seen_ms,
        (unsigned long long)ac->last_seen_ms,
        (unsigned long long)pos_age_ms);

    std::string out = hdr;

    // Optional fields — only emitted when set
    if (ac->emitter_cat != 0) {
        char tmp[32]; snprintf(tmp, sizeof(tmp), "\"cat\":%u,", ac->emitter_cat);
        out += tmp;
    }
    if (ac->squawk != 0) {
        char tmp[32]; snprintf(tmp, sizeof(tmp), "\"sqk\":\"%04u\",", ac->squawk);
        out += tmp;
    }
    if (ac->emergency_state != 0) {
        char tmp[32]; snprintf(tmp, sizeof(tmp), "\"emrg\":%u,", ac->emergency_state);
        out += tmp;
    }

    // Serialize trail circular buffer oldest-to-newest.
    // trail_head is the next-write slot; oldest entry is at
    // (trail_head + TRAIL_MAX - trail_len) % TRAIL_MAX.
    out += "\"trail\":[";
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
