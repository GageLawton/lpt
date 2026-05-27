#include <cstdio>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>
#include <climits>

#include "types.h"
#include "radio/rtlsdr.h"
#include "radio/ring_buffer.h"
#include "dsp/preamble.h"
#include "dsp/demod.h"
#include "decoder/modes.h"
#include "decoder/crc.h"
#include "decoder/cpr.h"
#include "decoder/altitude.h"
#include "decoder/velocity.h"
#include "renderer/map.h"
#include "renderer/aircraft.h"
#include "tracker/aircraft_table.h"

static const uint32_t ADSB_FREQ_HZ     = 1090000000U;
static const uint32_t ADSB_SAMPLE_RATE = 2000000U;
static const int      MAP_W            = 1024;
static const int      MAP_H            = 768;
// Default centre — user can override via argv in a future issue
static const double   HOME_LAT         = 37.7749;
static const double   HOME_LON         = -122.4194;

static RingBuffer        g_ring;
static std::atomic<bool> g_running{true};
static std::mutex        g_table_mutex;

// Inline IQ → magnitude (avoids adding mag.h dependency before issue #8 lands)
static void iq_to_mag(const uint8_t* iq, uint32_t iq_len, float* out)
{
    for (uint32_t i = 0; i < iq_len / 2; i++) {
        float I = (float)iq[2*i]   - 127.5f;
        float Q = (float)iq[2*i+1] - 127.5f;
        out[i] = hypotf(I, Q);
    }
}

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
    const uint32_t CHUNK   = 262144;
    const uint32_t MAG_LEN = CHUNK / 2;

    std::vector<uint8_t> raw(CHUNK);
    std::vector<float>   mag(MAG_LEN);
    uint8_t bits[112];

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
                double lat, lon;
                if (ac->position_valid) {
                    if (cpr_decode_local(lat_cpr, lon_cpr, odd,
                                         ac->lat, ac->lon, &lat, &lon)) {
                        ac->lat = lat;
                        ac->lon = lon;

                        ac->trail[ac->trail_head] = { ac->lat, ac->lon };
                        ac->trail_head = (ac->trail_head + 1) % TRAIL_MAX;
                        if (ac->trail_len < TRAIL_MAX) ac->trail_len++;
                    }
                } else {
                    // First position: use receiver location as reference
                    if (cpr_decode_local(lat_cpr, lon_cpr, odd,
                                         HOME_LAT, HOME_LON, &lat, &lon)) {
                        ac->lat = lat;
                        ac->lon = lon;
                        ac->position_valid = true;
                    }
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

int main()
{
    printf("lpt \xe2\x80\x94 ADS-B Plane Tracker\n");
    printf("Tuning to %.0f MHz...\n", ADSB_FREQ_HZ / 1e6);

    rb_init(&g_ring);

    if (map_init(MAP_W, MAP_H, HOME_LAT, HOME_LON) < 0) {
        fprintf(stderr, "SDL2 init failed\n");
        return 1;
    }

    if (rtlsdr_init(ADSB_FREQ_HZ, ADSB_SAMPLE_RATE) < 0) {
        map_close();
        return 1;
    }

    // Radio thread: rtlsdr_start blocks until rtlsdr_stop() is called.
    std::thread radio_thread([]() { rtlsdr_start(radio_cb); });
    std::thread dsp_thread(dsp_thread_fn);

    // Render loop on the main thread (~10 Hz).
    while (g_running) {
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

    rtlsdr_stop();
    radio_thread.join();
    dsp_thread.join();
    rtlsdr_close();
    map_close();
    return 0;
}
