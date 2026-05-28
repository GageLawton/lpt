#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>
#include <climits>
#include <string>
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
#include "renderer/map.h"
#include "renderer/aircraft.h"
#include "tracker/aircraft_table.h"

static const uint32_t ADSB_FREQ_HZ     = 1090000000U;
static const uint32_t ADSB_SAMPLE_RATE = 2000000U;
static const int      MAP_W            = 1024;
static const int      MAP_H            = 768;
static const double   DEFAULT_LAT      = 37.7749;
static const double   DEFAULT_LON      = -122.4194;

static double g_home_lat = DEFAULT_LAT;
static double g_home_lon = DEFAULT_LON;

static volatile sig_atomic_t g_got_signal = 0;

static RingBuffer        g_ring;
static std::atomic<bool> g_running{true};
static std::mutex        g_table_mutex;

static uint64_t now_ms()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

// DSP + decode thread: pops raw IQ, finds preambles, decodes frames,
// updates the aircraft table under g_table_mutex.
static void dsp_thread_fn()
{
    struct CprEntry {
        uint32_t lat_even{0}, lon_even{0};
        uint32_t lat_odd{0},  lon_odd{0};
        uint64_t t_even{0},   t_odd{0};
        bool has_even{false}, has_odd{false};
    };
    static std::unordered_map<uint32_t, CprEntry> cpr_cache;

    const uint32_t CHUNK   = 262144;
    const uint32_t MAG_LEN = CHUNK / 2;

    std::vector<uint8_t> raw(CHUNK);
    std::vector<float>   mag(MAG_LEN);
    uint8_t bits[112];

    while (g_running) {
        // Evict stale CPR cache entries every ~5 s to prevent unbounded growth
        {
            static uint64_t last_purge = 0;
            uint64_t now = now_ms();
            if (now - last_purge >= 5000) {
                for (auto it = cpr_cache.begin(); it != cpr_cache.end(); ) {
                    uint64_t age = std::max(it->second.t_even, it->second.t_odd);
                    it = (now - age > 60000) ? cpr_cache.erase(it) : std::next(it);
                }
                last_purge = now;
            }
        }

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

            uint64_t ts = now_ms();
            std::lock_guard<std::mutex> lk(g_table_mutex);
            Aircraft* ac = table_upsert(frame.icao, ts);

            if (tc >= 9 && tc <= 18) {
                // Airborne position (TC 9-18)
                // ME layout: [0]=TC|SS|NIC [1]=ALT[11:4] [2]=ALT[3:0]|T|F|LAT[16:15]
                //            [3]=LAT[14:7] [4]=LAT[6:0]|LON[16] [5]=LON[15:8] [6]=LON[7:0]
                int odd = (me[2] >> 2) & 1;
                uint32_t lat_cpr = ((uint32_t)(me[2] & 0x03) << 15)
                                 | ((uint32_t)me[3] << 7)
                                 |  (me[4] >> 1);
                uint32_t lon_cpr = ((uint32_t)(me[4] & 0x01) << 16)
                                 | ((uint32_t)me[5] << 8)
                                 |  me[6];

                // Update CPR cache for this ICAO
                CprEntry& entry = cpr_cache[frame.icao];
                if (odd) {
                    entry.lat_odd  = lat_cpr;
                    entry.lon_odd  = lon_cpr;
                    entry.t_odd    = ts;
                    entry.has_odd  = true;
                } else {
                    entry.lat_even = lat_cpr;
                    entry.lon_even = lon_cpr;
                    entry.t_even   = ts;
                    entry.has_even = true;
                }

                // Attempt global decode if both even and odd frames are available
                // and their timestamps are within 10 000 ms of each other
                double lat, lon;
                bool decoded = false;
                if (entry.has_even && entry.has_odd) {
                    uint64_t age_diff = entry.t_even > entry.t_odd
                                      ? entry.t_even - entry.t_odd
                                      : entry.t_odd  - entry.t_even;
                    if (age_diff <= 10000) {
                        int last_odd = odd; // the frame we just received
                        if (cpr_decode_global(entry.lat_even, entry.lon_even,
                                              entry.lat_odd,  entry.lon_odd,
                                              last_odd, &lat, &lon)) {
                            ac->lat = lat;
                            ac->lon = lon;
                            ac->position_valid = true;
                            decoded = true;
                        }
                    }
                }

                // Fall back to local decode when global is unavailable or fails
                if (!decoded) {
                    double ref_lat = ac->position_valid ? ac->lat : g_home_lat;
                    double ref_lon = ac->position_valid ? ac->lon : g_home_lon;
                    if (cpr_decode_local(lat_cpr, lon_cpr, odd,
                                         ref_lat, ref_lon, &lat, &lon)) {
                        ac->lat = lat;
                        ac->lon = lon;
                        ac->position_valid = true;
                        decoded = true;
                    }
                }

                // Record position in trail buffer whenever a decode succeeded
                if (decoded) {
                    ac->last_position_ms = ts;
                    ac->trail[ac->trail_head] = { ac->lat, ac->lon };
                    ac->trail_head = (ac->trail_head + 1) % TRAIL_MAX;
                    if (ac->trail_len < TRAIL_MAX) ac->trail_len++;
                }

                uint16_t alt_raw = ((uint16_t)(me[1] & 0xFF) << 4)
                                 |  (me[2] >> 4);
                int32_t alt = altitude_decode_gillham(alt_raw);
                if (alt != INT32_MIN) ac->altitude_ft = alt;

            } else if (tc == 19) {
                // Airborne velocity
                float spd, hdg;
                int32_t vr;
                if (velocity_decode(me, &spd, &hdg, &vr)) {
                    ac->groundspeed_kt = spd;
                    ac->heading_deg    = hdg;
                    ac->vert_rate_fpm  = vr;
                }
            } else if (tc >= 1 && tc <= 4) {
                // Aircraft identification — callsign
                callsign_decode(me, ac->callsign);
            }

            pos += (uint32_t)frame_len * 8 * 2;
        }
    }
}

// RTL-SDR callback — runs on the radio thread, just pushes bytes.
static void radio_cb(const uint8_t* buf, uint32_t len)
{
    rb_push(&g_ring, buf, len);
}

static void print_usage(const char* prog)
{
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("  --lat DEG      Receiver latitude in decimal degrees (default: %.4f)\n", DEFAULT_LAT);
    printf("  --lon DEG      Receiver longitude in decimal degrees (default: %.4f)\n", DEFAULT_LON);
    printf("  --label STR    Receiver label shown on scope (default: HOME)\n");
    printf("  --replay FILE  Replay raw IQ capture file instead of live hardware\n");
    printf("  --help         Print this message and exit\n");
}

int main(int argc, char* argv[])
{
    std::string replay_path;
    std::string receiver_label = "HOME";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") { print_usage(argv[0]); return 0; }
        if (i + 1 < argc) {
            if      (arg == "--lat")    { g_home_lat      = std::atof(argv[++i]); continue; }
            else if (arg == "--lon")    { g_home_lon      = std::atof(argv[++i]); continue; }
            else if (arg == "--label")  { receiver_label  = argv[++i];            continue; }
            else if (arg == "--replay") { replay_path     = argv[++i];            continue; }
        }
        fprintf(stderr, "Unknown option: %s\n", arg.c_str());
        print_usage(argv[0]);
        return 1;
    }

    printf("lpt \xe2\x80\x94 ADS-B Plane Tracker\n");
    printf("Receiver : %.4f, %.4f (%s)\n", g_home_lat, g_home_lon, receiver_label.c_str());

    rb_init(&g_ring);

    if (map_init(MAP_W, MAP_H, g_home_lat, g_home_lon) < 0) {
        fprintf(stderr, "SDL2 init failed\n");
        return 1;
    }

    // Install signal handlers before spawning threads so no early signal is missed.
    {
        struct sigaction sa{};
        sa.sa_handler = [](int) { g_got_signal = 1; };
        sa.sa_flags   = SA_RESTART;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT,  &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
    }

    std::thread radio_thread;
    if (!replay_path.empty()) {
        printf("Replay   : %s\n", replay_path.c_str());
        radio_thread = std::thread([&replay_path]() {
            FILE* f = fopen(replay_path.c_str(), "rb");
            if (!f) { perror("replay: fopen"); g_running = false; return; }
            std::vector<uint8_t> buf(262144);
            while (g_running) {
                size_t n = fread(buf.data(), 1, buf.size(), f);
                if (n == 0) { rewind(f); continue; }
                rb_push(&g_ring, buf.data(), (uint32_t)n);
                std::this_thread::sleep_for(std::chrono::milliseconds(130));
            }
            fclose(f);
        });
    } else {
        if (rtlsdr_init(ADSB_FREQ_HZ, ADSB_SAMPLE_RATE) < 0) {
            map_close();
            return 1;
        }
        radio_thread = std::thread([]() { rtlsdr_start(radio_cb); });
    }

    std::thread dsp_thread(dsp_thread_fn);

    // Render loop on the main thread (~10 Hz).
    while (g_running && !g_got_signal) {
        { std::lock_guard<std::mutex> lk(g_table_mutex); table_expire(now_ms(), 60000); }
        map_draw_background();
        map_draw_range_rings(50.0f);
        {
            std::lock_guard<std::mutex> lk(g_table_mutex);
            table_for_each([](const Aircraft* ac, void*) {
                aircraft_draw(ac);
                aircraft_draw_vector(ac);
            }, nullptr);
        }
        if (!map_present()) g_running = false;
        SDL_Delay(100);
    }

    g_running = false;
    if (replay_path.empty()) {
        rtlsdr_stop();
        rtlsdr_close();
    }
    if (radio_thread.joinable()) radio_thread.join();
    dsp_thread.join();
    map_close();
    return 0;
}
