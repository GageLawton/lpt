#pragma once
#include <cstdint>
#include <functional>

// Callback type: called with a buffer of raw IQ bytes from the device
using SampleCallback = std::function<void(const uint8_t* buf, uint32_t len)>;

// Open and configure the RTL-SDR device.
// gain_tenth_db: -1 = automatic AGC (default); otherwise manual gain in tenths
// of a dB (e.g. 496 = 49.6 dB). Use rtlsdr_get_tuner_gains() to query valid values.
// Returns 0 on success, negative on error.
int  rtlsdr_init(uint32_t freq_hz, uint32_t sample_rate_hz, int gain_tenth_db = -1);

// Begin async sample streaming — blocks until rtlsdr_stop() is called
void rtlsdr_start(SampleCallback cb);

// Signal the streaming loop to stop
void rtlsdr_stop();

// Release the device
void rtlsdr_close();
