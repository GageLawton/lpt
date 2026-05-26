#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>
#include <climits>
#include <string>
#include <csignal>
#include <unordered_map>

#include "types.h"
#include "radio/rtlsdr.h"
#include "radio/ring_buffer.h"
#include "dsp/mag.h"
#include "dsp/preamble.h"
#include "dsp/demod.h"
#include "decoder/modes.h"
#include "decoder/crc.h"
#include "decoder/cpr.h"
#include "decoder/altitude.h"
#include "decoder/velocity.h"
#include "decoder/callsign.h"
#include "tracker/aircraft_table.h"
#include "web/server.h"

static const uint32_t ADSB_FREQ_HZ     = 1090000000U;
static const uint32_t ADSB_SAMPLE_RATE = 2000000U;

static RingBuffer        g_ring;
static std::atomic<bool> g_running{true};
static std::mutex        g_table_mutex;
static WebStats          g_stats;

static uint64_t now_ms()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

static void dsp_thread_fn()
{
    const uint32_t CHUNK   = 262144;
    const uint32_t MAG_LEN = CHUNK / 2;

    std::vector<uint8_t> raw(CHUNK);
    std::vector<float>   mag(MAG_LEN);
    uint8_t bits[112];

    uint64_t sec_start    = now_ms();
    uint32_t msgs_this_sec = 0;

    while (g_running) {
        uint32_t got = rb_pop(&g_ring, raw.data(), CHUNK);
        if (got < 32) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        iq_to_mag(raw.data(), got, mag.data());
        uint32_t mag_len = got / 2;
        uint32_t pos = 0;

        while (pos + 16 + 56 * 4 <= mag_len) {
            int pre = preamble_search(mag.data() + pos, mag_len - pos);
            if (pre < 0) break;
            pos += (uint32_t)pre + 16;

            int nbits = demod_ook(mag.data() + pos, mag_len - pos, bits);
            if (nbits < 56) { pos++; continue; }

            uint8_t frame_len = (nbits >= 112) ? 14 : 7;
            uint8_t raw_bytes[14] = {};
            for (int i = 0; i < frame_len; i++)
                for (int b = 0; b < 8; b++)
                    raw_bytes[i] |= bits[i * 8 + b] << (7 - b);

            ModeSFrame frame{};
            if (!modes_parse(raw_bytes, frame_len, &frame)) { pos++; continue; }

            uint8_t tc = modes_tc(&frame);
            const uint8_t* me = frame.data + 4;

            std::lock_guard<std::mutex> lk(g_table_mutex);
            Aircraft* ac = table_upsert(frame.icao, now_ms());
            msgs_this_sec++;

            if (tc >= 9 && tc <= 18) {
                int odd = (me[2] >> 2) & 1;
                uint32_t lat_cpr = ((uint32_t)(me[2] & 0x03) << 15)
                                 | ((uint32_t)me[3] << 7)
                                 |  (me[4] >> 1);
                uint32_t lon_cpr = ((uint32_t)(me[4] & 0x01) << 16)
                                 | ((uint32_t)me[5] << 8)
                                 |  me[6];

                double ref_lat = ac->position_valid ? ac->lat : 37.7749;
                double ref_lon = ac->position_valid ? ac->lon : -122.4194;
                double lat, lon;
                if (cpr_decode_local(lat_cpr, lon_cpr, odd, ref_lat, ref_lon, &lat, &lon)) {
                    ac->lat = lat;
                    ac->lon = lon;
                    ac->position_valid = true;
                }

                uint16_t alt_raw = ((uint16_t)(me[1] & 0xFF) << 4) | (me[2] >> 4);
                int32_t alt = altitude_decode_gillham(alt_raw);
                if (alt != INT32_MIN) ac->altitude_ft = alt;

            } else if (tc == 19) {
                float spd, hdg;
                int32_t vr;
                if (velocity_decode(me, &spd, &hdg, &vr)) {
                    ac->groundspeed_kt = spd;
                    ac->heading_deg    = hdg;
                }
            } else if (tc >= 1 && tc <= 4) {
                callsign_decode(me, ac->callsign);
            }

            pos += (uint32_t)frame_len * 8 * 2;
        }

        uint64_t now = now_ms();
        if (now - sec_start >= 1000) {
            g_stats.msgs_last_sec.store(msgs_this_sec);
            g_stats.msgs_total.fetch_add(msgs_this_sec);
            msgs_this_sec = 0;
            sec_start = now;
        }
    }
}

static void radio_cb(const uint8_t* buf, uint32_t len)
{
    rb_push(&g_ring, buf, len);
}

static void print_usage(const char* prog)
{
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("  --port N       HTTP server port (default: 8080)\n");
    printf("  --lat DEG      Receiver latitude in decimal degrees (default: 37.7749)\n");
    printf("  --lon DEG      Receiver longitude in decimal degrees (default: -122.4194)\n");
    printf("  --label STR    Receiver label shown on scope (default: HOME)\n");
    printf("  --replay FILE  Replay raw IQ capture file instead of live hardware\n");
    printf("  --help         Print this message and exit\n");
}

int main(int argc, char* argv[])
{
    ServerConfig cfg;
    cfg.center_lat     = 37.7749;
    cfg.center_lon     = -122.4194;
    cfg.receiver_label = "HOME";
    std::string replay_path;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") { print_usage(argv[0]); return 0; }
        if (i + 1 < argc) {
            if      (arg == "--port")   { cfg.port           = std::atoi(argv[++i]); continue; }
            else if (arg == "--lat")    { cfg.center_lat     = std::atof(argv[++i]); continue; }
            else if (arg == "--lon")    { cfg.center_lon     = std::atof(argv[++i]); continue; }
            else if (arg == "--label")  { cfg.receiver_label = argv[++i];            continue; }
            else if (arg == "--replay") { replay_path        = argv[++i];            continue; }
        }
    }

    printf("lpt-web — ADS-B Browser Tracker\n");
    printf("Receiver : %.4f, %.4f (%s)\n",
           cfg.center_lat, cfg.center_lon, cfg.receiver_label.c_str());
    printf("Web UI   : http://localhost:%d\n", cfg.port);

    rb_init(&g_ring);
    g_stats.start_ms.store(now_ms());

    signal(SIGINT,  [](int){ server_stop(); g_running = false; });
    signal(SIGTERM, [](int){ server_stop(); g_running = false; });

    std::thread dsp_thread(dsp_thread_fn);
    std::thread radio_thread;

    if (!replay_path.empty()) {
        // Replay mode: feed a raw IQ capture file through the ring buffer
        printf("Replay   : %s\n", replay_path.c_str());
        radio_thread = std::thread([&replay_path]() {
            FILE* f = fopen(replay_path.c_str(), "rb");
            if (!f) {
                perror("replay: fopen");
                g_running = false;
                server_stop();
                return;
            }
            std::vector<uint8_t> buf(262144);
            while (g_running) {
                size_t n = fread(buf.data(), 1, buf.size(), f);
                if (n == 0) { rewind(f); continue; }
                rb_push(&g_ring, buf.data(), (uint32_t)n);
                // ~130 ms per 262144-byte chunk approximates 2 MSPS real-time
                std::this_thread::sleep_for(std::chrono::milliseconds(130));
            }
            fclose(f);
        });
    } else {
        if (rtlsdr_init(ADSB_FREQ_HZ, ADSB_SAMPLE_RATE) < 0) {
            fprintf(stderr, "rtlsdr_init failed — is an RTL-SDR dongle attached?\n");
            fprintf(stderr, "Tip: use --replay <file> to run without hardware.\n");
            g_running = false;
            dsp_thread.join();
            return 1;
        }
        radio_thread = std::thread([]() { rtlsdr_start(radio_cb); });
    }

    server_run(cfg, g_table_mutex, g_stats);   // blocks until SIGINT/SIGTERM

    g_running = false;
    if (replay_path.empty()) {
        rtlsdr_stop();
        rtlsdr_close();
    }
    if (radio_thread.joinable()) radio_thread.join();
    dsp_thread.join();
    return 0;
}
