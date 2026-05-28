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
#include "decoder/surface.h"
#include "tracker/aircraft_table.h"
#include "web/server.h"

static const uint32_t ADSB_FREQ_HZ     = 1090000000U;
static const uint32_t ADSB_SAMPLE_RATE = 2000000U;

static RingBuffer        g_ring;
static std::atomic<bool> g_running{true};
static std::mutex        g_table_mutex;
static WebStats          g_stats;
static double            g_center_lat       = 37.7749;
static double            g_center_lon       = -122.4194;
static uint32_t          g_timeout_ms       = 60000;
static int               g_gain_tenth_db    = -1;  // -1 = auto AGC

static uint64_t now_ms()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

// Decode 13-bit Mode A identity field → 4-digit squawk (e.g. 7700).
// Bit layout (MSB first): C1 A1 C2 A2 C4 A4 B1 X B2 D2 B4 D4 SPI
static uint16_t decode_mode_a(uint16_t id)
{
    uint8_t A = (((id >> 7) & 1) << 2) | (((id >> 9) & 1) << 1) | ((id >> 11) & 1);
    uint8_t B = (((id >> 2) & 1) << 2) | (((id >> 4) & 1) << 1) | ((id >>  6) & 1);
    uint8_t C = (((id >> 8) & 1) << 2) | (((id >>10) & 1) << 1) | ((id >> 12) & 1);
    uint8_t D = (((id >> 1) & 1) << 2) | (((id >> 3) & 1) << 1);
    return (uint16_t)(A * 1000 + B * 100 + C * 10 + D);
}

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

    uint64_t sec_start         = now_ms();
    uint32_t msgs_this_sec     = 0;
    uint32_t crc_fail_this_sec = 0;

    while (g_running) {
        // Evict stale CPR cache entries every ~5 s
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

            // ── DF5 / DF21: surveillance replies carrying Mode A squawk ──
            // These frames use address-parity (AP = ICAO XOR CRC), so
            // modes_parse CRC check would reject them. Handle separately.
            uint8_t raw_df = (raw_bytes[0] >> 3) & 0x1F;
            if (raw_df == 5 || raw_df == 21) {
                // Recover ICAO: AP = ICAO XOR CRC(payload); ICAO = CRC XOR AP
                uint32_t crc_val = modes_crc(raw_bytes, frame_len - 3);
                uint32_t ap = ((uint32_t)raw_bytes[frame_len-3] << 16)
                            | ((uint32_t)raw_bytes[frame_len-2] <<  8)
                            |  (uint32_t)raw_bytes[frame_len-1];
                uint32_t icao = crc_val ^ ap;
                if (icao != 0) {
                    // ID field: raw_bytes[2] bits[4:0] | raw_bytes[3]
                    uint16_t id  = ((uint16_t)(raw_bytes[2] & 0x1F) << 8) | raw_bytes[3];
                    uint16_t sqk = decode_mode_a(id);
                    if (sqk != 0) {
                        std::lock_guard<std::mutex> lk(g_table_mutex);
                        Aircraft* ac = table_upsert(icao, now_ms());
                        ac->squawk = sqk;
                        msgs_this_sec++;
                    }
                }
                pos += (uint32_t)frame_len * 8 * 2;
                continue;
            }

            ModeSFrame frame{};
            if (!modes_parse(raw_bytes, frame_len, &frame)) {
                crc_fail_this_sec++;
                pos++;
                continue;
            }

            uint8_t tc        = modes_tc(&frame);
            const uint8_t* me = frame.data + 4;
            uint64_t ts       = now_ms();

            std::lock_guard<std::mutex> lk(g_table_mutex);
            Aircraft* ac = table_upsert(frame.icao, ts);
            msgs_this_sec++;

            if ((tc >= 9 && tc <= 18) || (tc >= 20 && tc <= 22)) {
                // ── Airborne position: TC 9-18 (baro alt), TC 20-22 (GNSS alt) ──
                int      odd     = (me[2] >> 2) & 1;
                uint32_t lat_cpr = ((uint32_t)(me[2] & 0x03) << 15)
                                 | ((uint32_t)me[3] << 7)
                                 |  (me[4] >> 1);
                uint32_t lon_cpr = ((uint32_t)(me[4] & 0x01) << 16)
                                 | ((uint32_t)me[5] << 8)
                                 |  me[6];

                // Update global CPR cache
                CprEntry& entry = cpr_cache[frame.icao];
                if (odd) {
                    entry.lat_odd = lat_cpr; entry.lon_odd = lon_cpr;
                    entry.t_odd = ts; entry.has_odd = true;
                } else {
                    entry.lat_even = lat_cpr; entry.lon_even = lon_cpr;
                    entry.t_even = ts; entry.has_even = true;
                }

                double lat, lon;
                bool decoded = false;

                // Try global decode first (requires recent even+odd pair)
                if (entry.has_even && entry.has_odd) {
                    uint64_t age_diff = entry.t_even > entry.t_odd
                                      ? entry.t_even - entry.t_odd
                                      : entry.t_odd  - entry.t_even;
                    if (age_diff <= 10000) {
                        if (cpr_decode_global(entry.lat_even, entry.lon_even,
                                             entry.lat_odd,  entry.lon_odd,
                                             odd, &lat, &lon)) {
                            ac->lat = lat; ac->lon = lon;
                            ac->position_valid = true;
                            decoded = true;
                        }
                    }
                }

                // Fall back to local decode
                if (!decoded) {
                    double ref_lat = ac->position_valid ? ac->lat : g_center_lat;
                    double ref_lon = ac->position_valid ? ac->lon : g_center_lon;
                    if (cpr_decode_local(lat_cpr, lon_cpr, odd,
                                        ref_lat, ref_lon, &lat, &lon)) {
                        ac->lat = lat; ac->lon = lon;
                        ac->position_valid = true;
                        decoded = true;
                    }
                }

                if (decoded) {
                    ac->last_position_ms = ts;
                    ac->trail[ac->trail_head] = { ac->lat, ac->lon };
                    ac->trail_head = (ac->trail_head + 1) % TRAIL_MAX;
                    if (ac->trail_len < TRAIL_MAX) ac->trail_len++;
                }

                // Altitude
                uint16_t alt_raw = ((uint16_t)me[1] << 4) | (me[2] >> 4);
                if (tc <= 18) {
                    // TC 9-18: Gillham/Gray-coded barometric altitude
                    int32_t alt = altitude_decode_gillham(alt_raw);
                    if (alt != INT32_MIN) ac->altitude_ft = alt;
                } else {
                    // TC 20-22: GNSS altitude in metres, plain integer
                    if (alt_raw != 0)
                        ac->altitude_ft = (int32_t)(alt_raw * 3.28084f);
                }

            } else if (tc >= 5 && tc <= 8) {
                // ── Surface position: taxiing aircraft ──
                double ref_lat = ac->position_valid ? ac->lat : g_center_lat;
                double ref_lon = ac->position_valid ? ac->lon : g_center_lon;
                SurfacePos sp;
                if (surface_decode(me, ref_lat, ref_lon, &sp)) {
                    ac->lat = sp.lat; ac->lon = sp.lon;
                    ac->groundspeed_kt = sp.speed_kt;
                    if (sp.heading_deg >= 0) ac->heading_deg = sp.heading_deg;
                    ac->altitude_ft     = 0;
                    ac->position_valid  = true;
                    ac->last_position_ms = ts;
                    ac->trail[ac->trail_head] = { ac->lat, ac->lon };
                    ac->trail_head = (ac->trail_head + 1) % TRAIL_MAX;
                    if (ac->trail_len < TRAIL_MAX) ac->trail_len++;
                }

            } else if (tc == 19) {
                // ── Airborne velocity ──
                float spd, hdg; int32_t vr;
                if (velocity_decode(me, &spd, &hdg, &vr)) {
                    ac->groundspeed_kt = spd;
                    ac->heading_deg    = hdg;
                    ac->vert_rate_fpm  = vr;
                }

            } else if (tc >= 1 && tc <= 4) {
                // ── Identification: callsign + emitter category ──
                callsign_decode(me, ac->callsign);
                // Encode category as (TC << 3) | CA, giving unique non-zero values 8-39
                ac->emitter_cat = (uint8_t)((tc << 3) | (me[0] & 0x07));

            } else if (tc == 28) {
                // ── Emergency / Priority Status (sub-type 1) ──
                uint8_t st = me[0] & 0x07;
                if (st == 1) {
                    ac->emergency_state = (me[1] >> 5) & 0x07;
                    // Mode A squawk embedded in TC 28 ST 1 (13 bits)
                    uint16_t id  = ((uint16_t)(me[1] & 0x1F) << 8) | me[2];
                    uint16_t sqk = decode_mode_a(id);
                    if (sqk != 0) ac->squawk = sqk;
                }
            }

            pos += (uint32_t)frame_len * 8 * 2;
        }

        uint64_t now = now_ms();
        if (now - sec_start >= 1000) {
            g_stats.msgs_last_sec.store(msgs_this_sec);
            g_stats.msgs_total.fetch_add(msgs_this_sec);
            g_stats.crc_fail_last_sec.store(crc_fail_this_sec);
            // Sample ring buffer fill level (percent)
            uint32_t avail = rb_available(&g_ring);
            g_stats.buf_fill_pct.store((uint32_t)(avail * 100u / RING_BUFFER_SIZE));
            msgs_this_sec     = 0;
            crc_fail_this_sec = 0;
            sec_start = now;

            // Expire stale aircraft entries
            std::lock_guard<std::mutex> lk(g_table_mutex);
            table_expire(now, g_timeout_ms);
        }
    }
}

static void radio_cb(const uint8_t* buf, uint32_t len)
{
    if (!rb_push(&g_ring, buf, len))
        g_stats.buf_overflows.fetch_add(1, std::memory_order_relaxed);
}

static void print_usage(const char* prog)
{
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("  --port N       HTTP server port (default: 8080)\n");
    printf("  --lat DEG      Receiver latitude in decimal degrees (default: 37.7749)\n");
    printf("  --lon DEG      Receiver longitude in decimal degrees (default: -122.4194)\n");
    printf("  --label STR    Receiver label shown on scope (default: HOME)\n");
    printf("  --gain N       RTL-SDR gain in tenths of dB, e.g. 496 = 49.6 dB (default: auto)\n");
    printf("  --timeout MS   Aircraft expiry window in milliseconds (default: 60000)\n");
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
            if      (arg == "--port")    { cfg.port           = std::atoi(argv[++i]); continue; }
            else if (arg == "--lat")     { cfg.center_lat     = std::atof(argv[++i]); continue; }
            else if (arg == "--lon")     { cfg.center_lon     = std::atof(argv[++i]); continue; }
            else if (arg == "--label")   { cfg.receiver_label = argv[++i];            continue; }
            else if (arg == "--gain")    { g_gain_tenth_db    = std::atoi(argv[++i]); continue; }
            else if (arg == "--timeout") { g_timeout_ms       = (uint32_t)std::atoi(argv[++i]); continue; }
            else if (arg == "--replay")  { replay_path        = argv[++i];            continue; }
        }
        fprintf(stderr, "Unknown option: %s\n", arg.c_str());
        print_usage(argv[0]);
        return 1;
    }

    g_center_lat = cfg.center_lat;
    g_center_lon = cfg.center_lon;

    printf("lpt-web — ADS-B Browser Tracker\n");
    printf("Receiver : %.4f, %.4f (%s)\n",
           cfg.center_lat, cfg.center_lon, cfg.receiver_label.c_str());
    printf("Web UI   : http://localhost:%d\n", cfg.port);
    printf("Timeout  : %u ms\n", g_timeout_ms);

    rb_init(&g_ring);
    g_stats.start_ms.store(now_ms());

    static volatile sig_atomic_t g_got_signal = 0;
    struct sigaction sa{};
    sa.sa_handler = [](int) { g_got_signal = 1; };
    sa.sa_flags   = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    std::thread dsp_thread(dsp_thread_fn);
    std::thread radio_thread;

    std::thread watcher([]() {
        while (!g_got_signal && g_running)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        g_running = false;
        server_stop();
    });

    if (!replay_path.empty()) {
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
                std::this_thread::sleep_for(std::chrono::milliseconds(130));
            }
            fclose(f);
        });
    } else {
        if (rtlsdr_init(ADSB_FREQ_HZ, ADSB_SAMPLE_RATE, g_gain_tenth_db) < 0) {
            fprintf(stderr, "rtlsdr_init failed — is an RTL-SDR dongle attached?\n");
            fprintf(stderr, "Tip: use --replay <file> to run without hardware.\n");
            g_running = false;
            dsp_thread.join();
            watcher.join();
            return 1;
        }
        radio_thread = std::thread([]() { rtlsdr_start(radio_cb); });
    }

    server_run(cfg, g_table_mutex, g_stats);

    g_running = false;
    watcher.join();
    if (replay_path.empty()) {
        rtlsdr_stop();
        rtlsdr_close();
    }
    if (radio_thread.joinable()) radio_thread.join();
    dsp_thread.join();
    return 0;
}
