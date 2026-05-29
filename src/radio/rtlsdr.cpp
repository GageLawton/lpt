#include "rtlsdr.h"
#include <rtl-sdr.h>
#include <cstdio>

static rtlsdr_dev_t*  s_dev = nullptr;
static SampleCallback s_cb;

static void rtlsdr_callback(unsigned char* buf, uint32_t len, void*)
{
    if (s_cb) s_cb(buf, len);
}

int rtlsdr_init(uint32_t freq_hz, uint32_t sample_rate_hz, int gain_tenth_db)
{
    if (rtlsdr_open(&s_dev, 0) < 0) {
        fprintf(stderr, "[rtlsdr] Failed to open device\n");
        return -1;
    }
    rtlsdr_set_sample_rate(s_dev, sample_rate_hz);
    rtlsdr_set_center_freq(s_dev, freq_hz);
    if (gain_tenth_db < 0) {
        rtlsdr_set_tuner_gain_mode(s_dev, 0); // auto AGC
        rtlsdr_set_agc_mode(s_dev, 1);
        printf("[rtlsdr] Gain: auto\n");
    } else {
        rtlsdr_set_tuner_gain_mode(s_dev, 1); // manual
        rtlsdr_set_agc_mode(s_dev, 0);
        rtlsdr_set_tuner_gain(s_dev, gain_tenth_db);
        printf("[rtlsdr] Gain: %.1f dB\n", gain_tenth_db / 10.0);
    }
    rtlsdr_reset_buffer(s_dev);
    printf("[rtlsdr] Tuned to %.1f MHz @ %.1f MSPS\n", freq_hz / 1e6, sample_rate_hz / 1e6);
    return 0;
}

void rtlsdr_start(SampleCallback cb)
{
    s_cb = cb;
    rtlsdr_read_async(s_dev, rtlsdr_callback, nullptr, 0, 0);
}

void rtlsdr_stop()
{
    if (s_dev) rtlsdr_cancel_async(s_dev);
}

void rtlsdr_close()
{
    if (s_dev) {
        rtlsdr_close(s_dev);
        s_dev = nullptr;
    }
}
