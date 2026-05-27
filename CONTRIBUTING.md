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
| `test_velocity` | TC 19 velocity / vertical rate decode |
| `test_callsign` | TC 1–4 callsign decode |
| `test_preamble` | ADS-B preamble detection |
| `test_mag` | IQ → magnitude conversion |
| `test_tracker` | Aircraft table upsert / expire |

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

## Opening a pull request

- Reference the related issue number in the PR description (e.g. `Closes #42`)
- Keep PRs focused — one issue per PR makes review faster
- Run `ctest --test-dir build` before opening and confirm all tests pass
- For frontend changes, test in the browser using `design-files/index.html`
