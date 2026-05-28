#include "server.h"
#include "../tracker/aircraft_table.h"
#include "httplib.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>

static httplib::Server* g_svr = nullptr;

static std::string mime_for(const std::string& path)
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

static std::string read_file(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return ""; }
    rewind(f);
    std::string buf((size_t)sz, '\0');
    if (fread(&buf[0], 1, (size_t)sz, f) != (size_t)sz) buf.clear();
    fclose(f);
    return buf;
}

static std::string json_escape(const std::string& s)
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

static std::string aircraft_to_json(const Aircraft* ac, uint64_t now_ms)
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

void server_run(const ServerConfig& cfg, std::mutex& table_mutex, WebStats& stats)
{
    httplib::Server svr;
    g_svr = &svr;

    // Root — serve index.html
    svr.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        std::string body = read_file(cfg.web_dir + "/index.html");
        if (body.empty()) { res.status = 404; res.set_content("Not found", "text/plain"); return; }
        res.set_content(body, "text/html");
    });

    // SSE stream — push aircraft state ~1/sec
    svr.Get("/events", [&](const httplib::Request&, httplib::Response& res) {
        res.set_header("Cache-Control", "no-cache");
        res.set_header("X-Accel-Buffering", "no");
        res.set_chunked_content_provider("text/event-stream",
            [&](size_t, httplib::DataSink& sink) -> bool {
                using namespace std::chrono;
                uint64_t now_ms = (uint64_t)duration_cast<milliseconds>(
                    steady_clock::now().time_since_epoch()).count();

                struct AcCtx { std::string* out; uint64_t now_ms; };
                std::string planes_json;
                AcCtx ac_ctx = { &planes_json, now_ms };
                {
                    std::lock_guard<std::mutex> lk(table_mutex);
                    table_for_each([](const Aircraft* ac, void* ctx) {
                        // Emit aircraft with a position OR with notable metadata
                        // (squawk, emergency, callsign) — TC28/DF5/DF21 frames can
                        // populate those fields before a position lock is acquired.
                        bool interesting = ac->position_valid
                                        || ac->squawk != 0
                                        || ac->emergency_state != 0
                                        || ac->callsign[0] != 0;
                        if (!interesting) return;
                        auto* c = static_cast<AcCtx*>(ctx);
                        if (!c->out->empty() && c->out->back() != '[') *c->out += ",";
                        *c->out += aircraft_to_json(ac, c->now_ms);
                    }, &ac_ctx);
                }

                uint64_t t0 = stats.start_ms.load();
                uint64_t uptime_ms = (t0 > 0 && now_ms > t0) ? now_ms - t0 : 0;

                std::string label_esc = json_escape(cfg.receiver_label);
                char hdr[512];
                snprintf(hdr, sizeof(hdr),
                    "{\"receiver\":{\"lat\":%.6f,\"lon\":%.6f,\"label\":\"%s\"},"
                    "\"stats\":{\"msgsTotal\":%llu,\"msgsLastSec\":%u,\"crcFailLastSec\":%u,"
                    "\"uptimeSec\":%.1f,\"bufOverflows\":%u,\"bufFillPct\":%u},"
                    "\"planes\":[",
                    cfg.center_lat, cfg.center_lon, label_esc.c_str(),
                    (unsigned long long)stats.msgs_total.load(),
                    (unsigned int)stats.msgs_last_sec.load(),
                    (unsigned int)stats.crc_fail_last_sec.load(),
                    uptime_ms / 1000.0,
                    (unsigned int)stats.buf_overflows.load(),
                    (unsigned int)stats.buf_fill_pct.load());

                std::string event = "data: ";
                event += hdr;
                event += planes_json;
                event += "]}\n\n";

                if (!sink.write(event.c_str(), event.size())) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                return true;
            });
    });

    // Aircraft selection — client POSTs when user clicks a blip
    svr.Post(R"(/select/([0-9A-Fa-f]+))",
        [](const httplib::Request&, httplib::Response& res) {
            res.set_content("ok", "text/plain");
        });

    // Static file handler (catch-all, must be last)
    svr.Get(R"(/(.+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string path = cfg.web_dir + "/" + req.matches[1].str();
        // Prevent path traversal
        if (path.find("..") != std::string::npos) {
            res.status = 400; return;
        }
        std::string body = read_file(path);
        if (body.empty()) { res.status = 404; res.set_content("Not found", "text/plain"); return; }
        res.set_content(body, mime_for(path));
    });

    svr.listen("0.0.0.0", cfg.port);
    g_svr = nullptr;
}

void server_stop()
{
    if (g_svr) g_svr->stop();
}
