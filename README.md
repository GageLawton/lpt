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

