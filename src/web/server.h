#pragma once
#include <string>
#include <mutex>
#include <atomic>
#include <cstdint>

struct ServerConfig {
    int         port           = 8080;
    std::string web_dir        = "web";
    double      center_lat     = 0.0;
    double      center_lon     = 0.0;
    std::string receiver_label = "HOME";
};

struct WebStats {
    std::atomic<uint64_t> msgs_total{0};
    std::atomic<uint32_t> msgs_last_sec{0};
    std::atomic<uint64_t> start_ms{0};
};

// Start the HTTP server — blocks until server_stop() is called.
// table_mutex guards the aircraft table in aircraft_table.cpp.
void server_run(const ServerConfig& cfg, std::mutex& table_mutex, WebStats& stats);

// Signal server_run() to return cleanly.
void server_stop();
