# lpt — ADS-B Plane Tracker

Real-time aircraft tracking in C++ using an RTL-SDR dongle. Decodes ADS-B Mode S transmissions at 1090 MHz from scratch — no GNU Radio, no dump1090.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/GageLawton/lpt/actions/workflows/ci.yml/badge.svg)](https://github.com/GageLawton/lpt/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/GageLawton/lpt/branch/main/graph/badge.svg)](https://codecov.io/gh/GageLawton/lpt)

---

## Overview

lpt listens on 1090 MHz with a cheap RTL-SDR USB dongle and builds a live map of aircraft overhead. Every stage of the receive chain is implemented from scratch in C++17:

```
RTL-SDR hardware
      │  raw IQ bytes (uint8, 2 MSPS)
      ▼
 IQ → magnitude        (dsp/mag)
      │  float amplitude samples
      ▼
 Preamble detector      (dsp/preamble)
      │  frame start offset
      ▼
 OOK demodulator        (dsp/demod)
      │  bit stream (112 bits / frame)
      ▼
 Mode S parser + CRC    (decoder/modes, decoder/crc)
      │  ModeSFrame  (ICAO, raw payload)
      ▼
 Payload decoders
  ├─ CPR → lat/lon      (decoder/cpr)
  ├─ Altitude           (decoder/altitude)
  ├─ Velocity           (decoder/velocity)
  └─ Callsign           (decoder/callsign)
      │  Aircraft state
      ▼
 Aircraft state table   (tracker/aircraft_table)
      │  live fleet
      ▼
 SDL2 map renderer      (renderer/map, renderer/aircraft)
```

---

## Hardware setup

### RTL-SDR dongle

Any RTL2832U-based USB dongle works. Well-tested options:

- **RTL-SDR Blog V3 / V4** — best sensitivity, 1090 MHz SMA connector
- **Nooelec NESDR Smart** — good budget option
- **Generic DVB-T sticks** — work but are noisier

Pair the dongle with a **1090 MHz bandpass filter + antenna** for best range. A simple quarter-wave monopole (~6.9 cm wire) on a ground plane works, but a dedicated ADS-B antenna significantly improves coverage.

### Linux driver install

```bash
sudo apt-get install librtlsdr-dev rtl-sdr
```

### udev rules (run without sudo)

Create `/etc/udev/rules.d/rtl-sdr.rules`:

```
SUBSYSTEM=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2832", GROUP="plugdev", MODE="0664"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2838", GROUP="plugdev", MODE="0664"
```

Then reload and add your user to `plugdev`:

```bash
sudo udevadm control --reload-rules
sudo usermod -aG plugdev $USER
# Log out and back in for the group change to take effect
```

### Blacklist the DVB-T kernel module

The generic DVB-T driver claims the device and prevents rtl-sdr from opening it:

```bash
echo 'blacklist dvb_usb_rtl28xxu' | sudo tee /etc/modprobe.d/blacklist-rtl.conf
sudo modprobe -r dvb_usb_rtl28xxu
```

### Verify the dongle is detected

```bash
rtl_test -t
# Should print: Found 1 device(s): ...
```

---

## Dependencies

- librtlsdr
- SDL2
- CMake 3.16+


## Web display (lpt-web)

The project also includes a browser-based aircraft scope using `lpt-web`.

### Build

```bash
# Requires: cmake 3.16+, librtlsdr, g++
cmake -B build
cmake --build build --target lpt-web
```

### Run

```bash
./build/lpt-web [OPTIONS]

  --port N       HTTP server port                      (default: 8080)
  --lat DEG      Receiver latitude in decimal degrees  (default: 37.7749)
  --lon DEG      Receiver longitude in decimal degrees (default: -122.4194)
  --label STR    Receiver label shown on scope header  (default: HOME)
  --replay FILE  Replay a raw IQ capture file instead of live hardware
  --help         Print this message and exit

# Open http://localhost:<port> in your browser
```

### Architecture

`lpt-web` streams decoded aircraft data from the C++ ADS-B processing pipeline to a browser-based CRT scope using Server-Sent Events (SSE). The pipeline includes RTL-SDR signal capture, DSP processing, Mode S decoding, aircraft tracking, and live browser rendering.

### SSE event format

`lpt-web` pushes one JSON object per second to the `/events` endpoint:

```json
{
  "receiver": {
    "lat": 37.7749,
    "lon": -122.4194,
    "label": "HOME"
  },
  "stats": {
    "msgsTotal": 14523,
    "msgsLastSec": 42,
    "crcFailLastSec": 3,
    "uptimeSec": 347.0
  },
  "planes": [
    {
      "icao": "A1B2C3",
      "cs": "UAL123",
      "lat": 37.6213,
      "lon": -122.3790,
      "alt": 8500,
      "spd": 210.5,
      "hdg": 275.0,
      "vs": -640,
      "msgsRx": 17,
      "firstSeenMs": 1700000100000,
      "lastSeenMs": 1700000347000,
      "trail": [
        { "lat": 37.6200, "lon": -122.3750 }
      ]
    }
  ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `receiver.lat/lon` | float | Configured receiver position (degrees) |
| `stats.msgsLastSec` | int | Mode S frames decoded in the last second |
| `stats.crcFailLastSec` | int | Frames that failed CRC in the last second |
| `stats.uptimeSec` | float | Seconds since `lpt-web` started |
| `planes[].alt` | int | Altitude in feet (Gillham-decoded) |
| `planes[].spd` | float | Ground speed in knots |
| `planes[].hdg` | float | Heading in degrees (0–360, true north) |
| `planes[].vs` | int | Vertical speed in feet/minute (positive = climbing) |
| `planes[].trail` | array | Recent position fixes, oldest first |

---

## Native display (lpt)

The SDL2 build renders a live map directly on the desktop.

```bash
./build/lpt [OPTIONS]

  --lat DEG      Receiver latitude in decimal degrees  (default: 37.7749)
  --lon DEG      Receiver longitude in decimal degrees (default: -122.4194)
  --label STR    Receiver label                        (default: HOME)
  --replay FILE  Replay a raw IQ capture file instead of live hardware
  --help         Print this message and exit
```

## Screenshots

Add a screenshot of the CRT scope running with live data.

## License

MIT

