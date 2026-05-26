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

static std::string aircraft_to_json(const Aircraft* ac)
{
    char icao[7];
    snprintf(icao, sizeof(icao), "%06X", ac->icao);

    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"icao\":\"%s\",\"cs\":\"%s\","
        "\"lat\":%.6f,\"lon\":%.6f,"
        "\"alt\":%d,\"spd\":%.1f,\"hdg\":%.1f,"
        "\"vs\":0,\"msgsRx\":0,"
        "\"firstSeenMs\":%llu,\"lastSeenMs\":%llu,"
        "\"trail\":[]}",
        icao,
        ac->callsign[0] ? ac->callsign : "",
        ac->lat, ac->lon,
        (int)ac->altitude_ft,
        (double)ac->groundspeed_kt,
        (double)ac->heading_deg,
        (unsigned long long)ac->last_seen_ms,
        (unsigned long long)ac->last_seen_ms);
    return buf;
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
                std::string planes_json;
                {
                    std::lock_guard<std::mutex> lk(table_mutex);
                    bool first = true;
                    table_for_each([](const Aircraft* ac, void* ctx) {
                        if (!ac->position_valid) return;
                        std::string* out = static_cast<std::string*>(ctx);
                        if (!out->empty() && out->back() != '[') *out += ",";
                        *out += aircraft_to_json(ac);
                    }, &planes_json);
                }

                uint64_t uptime_ms = 0;
                uint64_t t0 = stats.start_ms.load();
                if (t0 > 0) {
                    using namespace std::chrono;
                    uint64_t now = (uint64_t)duration_cast<milliseconds>(
                        steady_clock::now().time_since_epoch()).count();
                    uptime_ms = now > t0 ? now - t0 : 0;
                }

                char hdr[512];
                snprintf(hdr, sizeof(hdr),
                    "{\"receiver\":{\"lat\":%.6f,\"lon\":%.6f,\"label\":\"%s\"},"
                    "\"stats\":{\"msgsTotal\":%llu,\"msgsLastSec\":%u,\"uptimeSec\":%.1f},"
                    "\"planes\":[",
                    cfg.center_lat, cfg.center_lon, cfg.receiver_label.c_str(),
                    (unsigned long long)stats.msgs_total.load(),
                    (unsigned int)stats.msgs_last_sec.load(),
                    uptime_ms / 1000.0);

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
