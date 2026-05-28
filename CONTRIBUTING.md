# Contributing to lpt

## Getting started

```bash
git clone https://github.com/GageLawton/lpt.git
cd lpt
cmake -B build
cmake --build build
ctest --test-dir build
```

All unit tests should pass before you open a pull request.

| Test binary | What it covers |
|---|---|
| `test_crc` | CRC-24 Mode S checksum |
| `test_cpr` | CPR position decode (global + local) |
| `test_demod` | OOK bit demodulation |
| `test_modes` | Mode S frame parsing |
| `test_altitude` | Gillham altitude decode |
| `test_velocity` | TC 19 velocity (subtypes 1-4) + vertical rate |
| `test_callsign` | TC 1–4 callsign decode |
| `test_preamble` | ADS-B preamble detection |
| `test_mag` | IQ → magnitude conversion |
| `test_tracker` | Aircraft table upsert / expire |
| `test_ring_buffer` | SPSC ring buffer push/pop/overflow |
| `test_e2e_decode` | Full pipeline: hex frame → decoded aircraft state |

## Hardware-free development

You don't need an RTL-SDR dongle to contribute.

### Unit tests

Every decoder and DSP module has a test binary. Run them all with:

```bash
ctest --test-dir build
```

This covers CRC, CPR position decode, preamble detection, OOK demodulation, altitude, velocity, callsign, and the aircraft tracker — no hardware required.

### Browser UI with sim data

The full CRT scope UI runs entirely in the browser using simulated aircraft:

1. Open `design-files/index.html` directly in your browser (no server needed).
2. The page loads `design-files/sim.jsx`, which generates a simulated fleet of 17 aircraft with realistic trajectories.
3. You can edit `web/scope-classic.jsx` and `web/` files and reload to see changes immediately.

This is the fastest way to iterate on the frontend without any C++ compilation or hardware.

## Project structure

```
src/
  radio/      — RTL-SDR wrapper + SPSC ring buffer
  dsp/        — IQ magnitude, preamble detection, OOK demodulation
  decoder/    — Mode S parsing, CRC-24, CPR, altitude, velocity, callsign
  tracker/    — Aircraft state table (upsert / expire)
  renderer/   — SDL2 map + aircraft drawing (native lpt build only)
  web/        — SSE HTTP server for browser mode (lpt-web)
web/          — Browser UI (React/JSX, no build step required)
design-files/ — UI design mockups and browser prototypes with sim data
tests/        — Unit tests for all decoder and DSP modules
vendor/       — Vendored dependencies (httplib.h single-header HTTP server)
```

## Build flags (lpt-web)

| Flag | Default | Description |
|------|---------|-------------|
| `--port N` | 8080 | HTTP server port |
| `--lat DEG` | 37.7749 | Receiver latitude |
| `--lon DEG` | -122.4194 | Receiver longitude |
| `--label STR` | HOME | Label shown on scope |
| `--gain N` | auto | RTL-SDR gain in tenths of dB (e.g. `496` = 49.6 dB) |
| `--timeout MS` | 60000 | Aircraft expiry window in milliseconds |
| `--replay FILE` | — | Replay a raw IQ capture instead of live hardware |

## ADS-B Type Code coverage

| TC | Content | Handler | Status |
|----|---------|---------|--------|
| 1–4 | Aircraft identification + emitter category | `decoder/callsign.cpp` | ✅ |
| 5–8 | Surface position (taxiing aircraft) | `decoder/surface.cpp` | ✅ |
| 9–18 | Airborne position, barometric altitude | `decoder/cpr.cpp` | ✅ global + local |
| 19 | Airborne velocity | `decoder/velocity.cpp` | ✅ subtypes 1–4 |
| 20–22 | Airborne position, GNSS altitude | `decoder/cpr.cpp` | ✅ |
| 28 | Emergency / priority status + squawk | `main-web.cpp` | ✅ sub-type 1 |
| 29 | Target state and status | — | ⬜ not implemented |
| 31 | Aircraft operational status | — | ⬜ not implemented |

Mode S Downlink Formats handled:

| DF | Content | Handler |
|----|---------|---------|
| 5 | Surveillance alt reply (Mode A squawk) | `main-web.cpp` |
| 17 | ADS-B extended squitter | `decoder/modes.cpp` |
| 18 | TIS-B extended squitter | `decoder/modes.cpp` |
| 21 | Comm-B alt reply (Mode A squawk) | `main-web.cpp` |

## Code style

- C++17; no exceptions, no RTTI.
- `static` for all translation-unit-local functions and globals.
- No heap allocation in hot paths (DSP thread); use stack arrays and pre-allocated vectors.
- Comments explain *why*, not *what*. One line maximum; omit if obvious from the code.
- Four-space indentation, no trailing whitespace.

## Opening a pull request

- Reference the related issue number in the PR description (e.g. `Closes #42`)
- Keep PRs focused — one issue per PR makes review faster
- Run `ctest --test-dir build` before opening and confirm all tests pass
- For frontend changes, test in the browser using `design-files/index.html`
