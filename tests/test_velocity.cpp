#include <cassert>
#include <cstdio>
#include <cmath>
#include "decoder/velocity.h"

static bool approx(float a, float b, float tol = 1.0f)
{
    return fabsf(a - b) < tol;
}

int main()
{
    float   spd, hdg;
    int32_t vr;

    // ── Subtype 1: ground speed ───────────────────────────────────────────

    // 100 kt due east → heading ≈ 90°
    {
        uint8_t me[] = {0x99, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00};
        assert(velocity_decode(me, &spd, &hdg, &vr));
        assert(approx(spd, 100.f));
        assert(approx(hdg, 90.f));
    }

    // 100 kt due north → heading ≈ 0°
    {
        uint8_t me[] = {0x99, 0x00, 0x01, 0x0C, 0xA0, 0x00, 0x00};
        assert(velocity_decode(me, &spd, &hdg, &vr));
        assert(approx(spd, 100.f));
        assert(approx(hdg, 0.f));
    }

    // 100 kt due west (ew_dir=1) → heading ≈ 270°
    {
        uint8_t me[] = {0x99, 0x04, 0x65, 0x00, 0x20, 0x00, 0x00};
        assert(velocity_decode(me, &spd, &hdg, &vr));
        assert(approx(spd, 100.f));
        assert(approx(hdg, 270.f));
    }

    // Zero EW velocity (raw 0 = not available) → false
    {
        uint8_t me[] = {0x99, 0x00, 0x00, 0x0C, 0xA0, 0x00, 0x00};
        assert(!velocity_decode(me, &spd, &hdg, &vr));
    }

    // Zero NS velocity (raw 0 = not available) → false
    {
        uint8_t me[] = {0x99, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00};
        assert(!velocity_decode(me, &spd, &hdg, &vr));
    }

    // Climbing 512 fpm
    {
        uint8_t me[] = {0x99, 0x00, 0x65, 0x00, 0x20, 0x12, 0x00};
        assert(velocity_decode(me, &spd, &hdg, &vr));
        assert(vr == 512);
    }

    // Descending 1024 fpm
    {
        uint8_t me[] = {0x99, 0x00, 0x65, 0x00, 0x24, 0x22, 0x00};
        assert(velocity_decode(me, &spd, &hdg, &vr));
        assert(vr == -1024);
    }

    // ── Subtype 2: supersonic (velocity units × 4) ───────────────────────

    // Same EW payload as "100 kt due east" but ST=2; expect 400 kt
    {
        uint8_t me[] = {0x9A, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00};
        assert(velocity_decode(me, &spd, &hdg, &vr));
        assert(approx(spd, 400.f, 2.f));
        assert(approx(hdg, 90.f));
    }

    // ── Subtype 3: magnetic heading + IAS ───────────────────────────────
    // heading_status=1, hdg_raw=512 (180°), IAS=250 kt
    // me[1]=0x06: bit2=hdg_status=1, bits1:0=hdg_raw[9:8]=2
    // me[2]=0x00: hdg_raw[7:0] → hdg_raw = (2<<8)|0 = 512 → 180°
    // me[3]=0x1F, me[4]=0x60: as_val = (31<<3)|3 = 251 → 250 kt after bias
    {
        uint8_t me[] = {0x9B, 0x06, 0x00, 0x1F, 0x60, 0x00, 0x00};
        assert(velocity_decode(me, &spd, &hdg, &vr));
        assert(approx(hdg, 180.f, 1.f));
        assert(approx(spd, 250.f, 1.f));
    }

    // Subtype 3: airspeed not available (raw as_val=0) → false
    {
        uint8_t me[] = {0x9B, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
        assert(!velocity_decode(me, &spd, &hdg, &vr));
    }

    // ── Invalid subtype 5 → false ─────────────────────────────────────────
    {
        uint8_t me[] = {0x9D, 0x00, 0x65, 0x00, 0x20, 0x00, 0x00};
        assert(!velocity_decode(me, &spd, &hdg, &vr));
    }

    printf("test_velocity: all tests passed\n");
    return 0;
}
