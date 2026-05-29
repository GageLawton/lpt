/* live.jsx — SSE client; exposes the same window.LPTSim API as sim.jsx */

const LPTSim = (() => {
  let planes = [], partialPlanes = [], stats = {}, selectedIcao = null;
  let RECEIVER = { lat: 0, lon: 0, label: "HOME" };
  const subs = new Set();
  const prevMsgsRx = new Map();        // icao → last msgsRx seen
  const FLASH_KEEP_MS = 1000;          // keep freshMs around so scope can fade
  const RATE_HISTORY = 80;             // sparkline width in samples
  const msgRateHistory = [];           // rolling msgsLastSec history

  function notify() { subs.forEach(fn => fn()); }

  // Tag aircraft with freshMs when their msgsRx counter advances, so the scope
  // can briefly brighten the blip on each new ping. Old freshMs values older
  // than FLASH_KEEP_MS are stripped to keep the object small.
  function applyFreshness(arr, now) {
    return arr.map(p => {
      const prev = prevMsgsRx.get(p.icao);
      const advanced = prev != null && p.msgsRx > prev;
      const isNew = prev == null;
      prevMsgsRx.set(p.icao, p.msgsRx ?? 0);
      const freshMs = (advanced || isNew) ? now : (p._freshMs ?? 0);
      const keep = freshMs > 0 && (now - freshMs) < FLASH_KEEP_MS ? freshMs : 0;
      return { ...p, freshMs: keep, selected: p.icao === selectedIcao };
    });
  }

  function connect() {
    const es = new EventSource("/events");
    es.onmessage = (e) => {
      const d = JSON.parse(e.data);
      const now = Date.now();
      RECEIVER = d.receiver;

      msgRateHistory.push(d.stats.msgsLastSec ?? 0);
      if (msgRateHistory.length > RATE_HISTORY) msgRateHistory.shift();

      stats = {
        ...d.stats,
        crcFailLastSec: d.stats.crcFailLastSec ?? 0,
        msgRateHistory: msgRateHistory.slice(),
      };
      planes        = applyFreshness(d.planes ?? [], now);
      partialPlanes = applyFreshness(d.partialPlanes ?? [], now);

      // ICAOs no longer present: drop their freshness state so the map doesn't grow.
      const live = new Set([...planes, ...partialPlanes].map(p => p.icao));
      for (const icao of prevMsgsRx.keys()) {
        if (!live.has(icao)) prevMsgsRx.delete(icao);
      }
      notify();
    };
    es.onerror = () => { es.close(); setTimeout(connect, 3000); };
  }
  connect();

  return {
    get planes()         { return planes;        },
    get partialPlanes()  { return partialPlanes; },
    get stats()          { return stats;         },
    get RECEIVER()       { return RECEIVER;      },
    subscribe(fn)       { subs.add(fn); return () => subs.delete(fn); },
    setSelected(icao)   { selectedIcao = icao; if (icao) fetch(`/select/${icao}`, { method: "POST" }); notify(); },
    getSelected()       { return planes.find(p => p.icao === selectedIcao) ?? null; },
  };
})();

window.LPTSim       = LPTSim;
window.LPT_RECEIVER = LPTSim.RECEIVER;

// Copy useLPT, makeProj, and fmt verbatim from design-files/sim.jsx below this line

const NM_PER_DEG_LAT = 60;
const nmPerDegLon = (lat) => 60 * Math.cos((lat * Math.PI) / 180);

// React hook — re-render at a throttled rate so we don't repaint every plane on every tick.
function useLPT(fps = 12) {
  const [, force] = React.useReducer((x) => (x + 1) & 0xfffffff, 0);
  React.useEffect(() => {
    let last = 0;
    return LPTSim.subscribe(() => {
      const now = performance.now();
      if (now - last >= 1000 / fps) { last = now; force(); }
    });
  }, [fps]);
  return LPTSim;
}

// projection helper — build per-render
function makeProj({ centerLat, centerLon, w, h, nmPerPx }) {
  return function project(lat, lon) {
    const dyNm = (lat - centerLat) * NM_PER_DEG_LAT;
    const dxNm = (lon - centerLon) * nmPerDegLon(centerLat);
    return { x: w / 2 + dxNm / nmPerPx, y: h / 2 - dyNm / nmPerPx };
  };
}

// utility: format helpers
const fmt = {
  alt: (a) => 'FL' + Math.round(a / 100).toString().padStart(3, '0'),
  altFt: (a) => Math.round(a).toString().padStart(5, ' ') + 'ft',
  spd: (s) => Math.round(s).toString().padStart(3, ' ') + 'kt',
  hdg: (h) => Math.round(((h % 360) + 360) % 360).toString().padStart(3, '0'),
  vs:  (v) => (v > 50 ? '↑' : v < -50 ? '↓' : '·') + Math.abs(Math.round(v / 100) * 100).toString().padStart(4, ' '),
  icao:(c) => c.toUpperCase(),
  pad: (s, n) => s.toString().padEnd(n, ' '),
  hms: (s) => {
    const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = Math.floor(s % 60);
    return [h, m, sec].map((x) => x.toString().padStart(2, '0')).join(':');
  },
};

Object.assign(window, { useLPT, makeProj, fmt });
