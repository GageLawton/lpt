/* scope-classic.jsx — Variant A: CRT phosphor radar scope.
   Round display, sweeping arm, scanlines, dim contour terrain. */

// Maximum age of a freshness "ping" before it fades out completely.
const FLASH_LIFETIME_MS = 600;

function ScopeClassic({ w = 1280, h = 800 }) {
  const sim = useLPT(15);
  const pal = usePalette();
  const knobs = useKnobs();

  // layout
  const HEADER_H = 40;
  const SCOPE_SIZE = h - HEADER_H - 20; // square
  const SIDE_W = w - SCOPE_SIZE - 20;

  // selected aircraft — null until first SSE batch arrives
  const [selectedIcao, setSelectedIcao] = React.useState(null);

  // Altitude filter (defaults are 0..60000 → no filtering)
  const altMin = knobs.altMinFt ?? 0;
  const altMax = knobs.altMaxFt ?? 60000;
  const passesAlt = (p) => {
    const alt = p.alt ?? 0;
    return alt >= altMin && alt <= altMax;
  };

  // Apply altitude filter once; downstream uses these.
  const allPlanes = (sim.planes ?? []).filter(passesAlt);
  const partialPlanes = (sim.partialPlanes ?? []).filter(passesAlt);

  // projection setup — center on receiver, or on selected aircraft when the
  // user has toggled "Center on selected". Range rings, labels, and the
  // receiver marker all stay correct relative to the chosen center because
  // they're derived from it.
  const cx = SCOPE_SIZE / 2, cy = SCOPE_SIZE / 2;
  const RING_NM = knobs.ringSpacingNm ?? 20;
  const RING_COUNT = 5;
  const maxNm = RING_NM * RING_COUNT;
  const nmPerPx = maxNm / (cx - 14);

  // selected aircraft (used by both projection & sidebar)
  const selected = allPlanes.find((p) => p.icao === selectedIcao) ?? allPlanes[0] ?? null;
  const centerOnSelected = !!knobs.centerOnSelected && selected != null;
  const centerLat = centerOnSelected ? selected.lat : sim.RECEIVER.lat;
  const centerLon = centerOnSelected ? selected.lon : sim.RECEIVER.lon;

  const project = makeProj({
    centerLat, centerLon, w: SCOPE_SIZE, h: SCOPE_SIZE, nmPerPx,
  });

  // terrain + coastlines — guarded: terrain.jsx is not loaded in web/, so
  // buildTerrainPaths/buildPolylinePath may be undefined and sim lacks PEAKS/COASTLINE/BAY.
  // Recompute when projection center changes so terrain follows the view.
  const terrain = React.useMemo(
    () => (sim.PEAKS && typeof buildTerrainPaths !== 'undefined')
      ? buildTerrainPaths({ peaks: sim.PEAKS, project, jitter: 1.1, levels: 5 })
      : [],
    [centerLat, centerLon, nmPerPx]
  );
  const coast = React.useMemo(
    () => (sim.COASTLINE && typeof buildPolylinePath !== 'undefined')
      ? buildPolylinePath(sim.COASTLINE, project)
      : '',
    [centerLat, centerLon, nmPerPx]
  );
  const bay = React.useMemo(
    () => (sim.BAY && typeof buildPolylinePath !== 'undefined')
      ? buildPolylinePath(sim.BAY, project)
      : '',
    [centerLat, centerLon, nmPerPx]
  );

  // "Connecting..." only before the first SSE event arrives.
  // Once uptimeSec > 0 the server is up; empty planes just means no aircraft in range.
  const isConnecting = allPlanes.length === 0 && partialPlanes.length === 0
                      && (!sim.stats || sim.stats.uptimeSec === 0);
  if (isConnecting) {
    return (
      <div style={{
        width: w, height: h, background: pal.bg, color: pal.dim,
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        fontFamily: 'JetBrains Mono, ui-monospace, monospace',
        fontSize: 13, letterSpacing: 3, textTransform: 'uppercase',
      }}>
        Connecting…
      </div>
    );
  }

  // sweep
  const tNow = performance.now();
  const sweepDeg = ((tNow / 1000) * (360 / knobs.sweepSpeed)) % 360 - 90;

  // planes inside the visible disk
  const inDisk = (s) => Math.hypot(s.x - cx, s.y - cy) < cx - 4;
  const visiblePlanes = allPlanes.filter((p) => inDisk(project(p.lat, p.lon)));

  // receiver pixel position in the (possibly off-center) projection
  const rxScreen = project(sim.RECEIVER.lat, sim.RECEIVER.lon);

  return (
    <div style={{
      width: w, height: h, background: pal.bg, color: pal.fg, position: 'relative',
      fontFamily: 'JetBrains Mono, ui-monospace, monospace', overflow: 'hidden',
    }}>
      {/* CRT vignette */}
      <div style={{
        position: 'absolute', inset: 0, pointerEvents: 'none',
        background: `radial-gradient(ellipse at center, transparent ${55 + (1 - knobs.vignette) * 30}%, rgba(0,0,0,${0.85 * knobs.vignette}) 100%)`,
        zIndex: 5,
      }} />
      {/* scanlines */}
      <div style={{
        position: 'absolute', inset: 0, pointerEvents: 'none',
        backgroundImage: `repeating-linear-gradient(0deg, rgba(0,0,0,${0.3 * knobs.scanlines}) 0px, rgba(0,0,0,${0.3 * knobs.scanlines}) 1px, transparent 1px, transparent 3px)`,
        zIndex: 6, mixBlendMode: 'multiply',
      }} />

      {/* HEADER */}
      <div style={{
        height: HEADER_H, display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        padding: '0 16px', borderBottom: `1px solid ${pal.faint}`, color: pal.fg,
        fontSize: 11, letterSpacing: 2, textTransform: 'uppercase',
      }}>
        <div style={{ display: 'flex', gap: 20, alignItems: 'center' }}>
          <span style={{ fontSize: 14, fontWeight: 700 }}>◉ LPT — {sim.RECEIVER?.label ?? 'HOME'}</span>
          <span style={{ color: pal.dim }}>ADS-B SCOPE · 1090.000 MHz · 2.0 MS/s</span>
        </div>
        <div style={{ display: 'flex', gap: 18, color: pal.dim }}>
          <span>LOCK <span style={{ color: pal.fg }}>●</span></span>
          <span>RX <span style={{ color: pal.fg }}>●</span></span>
          <span>UPTIME {fmt.hms(sim.stats.uptimeSec ?? 0)}</span>
          <span>{new Date().toISOString().slice(11, 19)}Z</span>
        </div>
      </div>

      {/* MAIN */}
      <div style={{ display: 'flex', height: h - HEADER_H }}>

        {/* SCOPE */}
        <div style={{ width: SCOPE_SIZE, height: SCOPE_SIZE, position: 'relative', padding: 10 }}>
          <svg width={SCOPE_SIZE - 20} height={SCOPE_SIZE - 20} viewBox={`0 0 ${SCOPE_SIZE - 20} ${SCOPE_SIZE - 20}`}
            style={{ filter: knobs.glow > 0 ? `drop-shadow(0 0 ${6 * knobs.glow}px ${pal.glow})` : 'none' }}>
            <defs>
              <clipPath id="scope-disk-cls">
                <circle cx={cx - 10} cy={cy - 10} r={cx - 14} />
              </clipPath>
              <radialGradient id="sweep-grad-cls" cx="0%" cy="50%" r="100%">
                <stop offset="0%" stopColor={pal.fg} stopOpacity={0.6} />
                <stop offset="100%" stopColor={pal.fg} stopOpacity={0} />
              </radialGradient>
            </defs>

            <g transform={`translate(-10 -10)`}>
              {/* outer disk */}
              <circle cx={cx} cy={cy} r={cx - 12} fill="#040b06" stroke={pal.dim} strokeWidth={1.4} />

              <g clipPath="url(#scope-disk-cls)">
                {/* terrain — only rendered when sim.PEAKS is available */}
                {terrain.length > 0 && <TerrainLayer paths={terrain} color={pal.fg} baseOpacity={0.13} mode="contour" />}
                {/* coast & bay — only rendered when data is available */}
                {coast && <CoastLayer d={coast} color={pal.fg} opacity={0.35} width={0.9} />}
                {bay   && <CoastLayer d={bay}   color={pal.fg} opacity={0.25} width={0.7} />}

                {/* radial spokes every 30° */}
                {Array.from({ length: 12 }).map((_, i) => {
                  const a = ((i * 30 - 90) * Math.PI) / 180;
                  return (
                    <line key={i} x1={cx} y1={cy}
                      x2={cx + Math.cos(a) * (cx - 14)} y2={cy + Math.sin(a) * (cx - 14)}
                      stroke={pal.faint} strokeWidth={0.5} strokeDasharray="2 5" />
                  );
                })}

                {/* range rings */}
                <RangeRings cx={cx} cy={cy} ringNm={RING_NM} ringCount={RING_COUNT}
                  nmPerPx={nmPerPx} color={pal.fg} dim={pal.dim} />

                {/* SWEEP wedge */}
                <g transform={`rotate(${sweepDeg} ${cx} ${cy})`}>
                  <path
                    d={`M ${cx} ${cy} L ${cx + (cx - 14)} ${cy} A ${cx - 14} ${cx - 14} 0 0 0 ${cx + (cx - 14) * Math.cos(-Math.PI/3)} ${cy + (cx - 14) * Math.sin(-Math.PI/3)} Z`}
                    fill="url(#sweep-grad-cls)" opacity={0.9} />
                  <line x1={cx} y1={cy} x2={cx + (cx - 14)} y2={cy} stroke={pal.fg} strokeWidth={1.4} opacity={0.95} />
                </g>

                {/* trails: dots that age out, brightness based on sweep angle */}
                {visiblePlanes.flatMap((p) =>
                  (p.trail ?? []).slice(-knobs.trailLength).map((t, i, arr) => {
                    const s = project(t.lat, t.lon);
                    const age = (arr.length - i) / arr.length;
                    return (
                      <circle key={p.icao + i} cx={s.x} cy={s.y} r={1.2}
                        fill={pal.fg} opacity={0.35 * (1 - age)} />
                    );
                  })
                )}

                {/* planes */}
                {visiblePlanes.map((p) => {
                  const s = project(p.lat, p.lon);
                  const isSel = selected !== null && p.icao === selected.icao;
                  // Ping ring on new ADS-B message — expands and fades over FLASH_LIFETIME_MS.
                  let pingAge = -1;
                  if (p.freshMs) {
                    const age = Date.now() - p.freshMs;
                    if (age >= 0 && age < FLASH_LIFETIME_MS) pingAge = age;
                  }
                  return (
                    <g key={p.icao} style={{ cursor: 'pointer' }} onClick={() => setSelectedIcao(p.icao)}>
                      {/* selection halo */}
                      {isSel && <circle cx={s.x} cy={s.y} r={11} fill="none" stroke={pal.sel} strokeWidth={1} strokeDasharray="2 2" />}
                      {/* fresh-ping flash */}
                      {pingAge >= 0 && (() => {
                        const t = pingAge / FLASH_LIFETIME_MS;
                        return <circle cx={s.x} cy={s.y} r={6 + t * 18}
                          fill="none" stroke={pal.fg} strokeWidth={1.1}
                          opacity={(1 - t) * 0.7} />;
                      })()}
                      {/* heading vector — 1 min projection */}
                      {(() => {
                        const distNm = (p.spd ?? 0) / 60;
                        const len = distNm / nmPerPx;
                        const a = (((p.hdg ?? 0) - 90) * Math.PI) / 180;
                        return (
                          <line x1={s.x} y1={s.y} x2={s.x + Math.cos(a) * len} y2={s.y + Math.sin(a) * len}
                            stroke={pal.fg} strokeWidth={0.8} opacity={0.7} />
                        );
                      })()}
                      <PlaneTriangle x={s.x} y={s.y} hdg={p.hdg} kind={p.kind} isSelected={isSel} color={pal.fg} />
                      <DataTagFull x={s.x} y={s.y} plane={p} color={pal.fg} dim={pal.dim} selected={isSel} />
                    </g>
                  );
                })}

                {/* receiver marker — drawn at its projected position so it
                    stays at the centre when in receiver-centred mode and slides
                    when the view is centred on a selected aircraft. */}
                {inDisk(rxScreen) && (
                  <g transform={`translate(${rxScreen.x} ${rxScreen.y})`}>
                    <circle r={6} fill="none" stroke={pal.fg} strokeWidth={1.2} />
                    <text x={10} y={-4} fill={pal.fg} fontFamily="JetBrains Mono, monospace" fontSize={9}>
                      RX · {sim.RECEIVER.label}
                    </text>
                  </g>
                )}

                {/* corner CRT labels */}
                <text x={14} y={20} fill={pal.dim} fontSize={9} fontFamily="JetBrains Mono, monospace">RNG {maxNm}NM</text>
                <text x={14} y={SCOPE_SIZE - 24} fill={pal.dim} fontSize={9} fontFamily="JetBrains Mono, monospace">
                  {centerLat.toFixed(4)}°{centerLat >= 0 ? 'N' : 'S'} {Math.abs(centerLon).toFixed(4)}°{centerLon >= 0 ? 'E' : 'W'}
                </text>
                <text x={SCOPE_SIZE - 70} y={20} fill={pal.dim} fontSize={9} fontFamily="JetBrains Mono, monospace">N↑</text>
                {centerOnSelected && (
                  <text x={SCOPE_SIZE - 130} y={SCOPE_SIZE - 24} fill={pal.warn} fontSize={9}
                    fontFamily="JetBrains Mono, monospace">
                    CTR · {selected.cs || fmt.icao(selected.icao)}
                  </text>
                )}
              </g>
            </g>
          </svg>
        </div>

        {/* SIDEBAR */}
        <ScopeClassicSidebar w={SIDE_W} h={h - HEADER_H} sim={sim} pal={pal}
          selected={selected} onSelect={setSelectedIcao} project={project}
          cx={cx} cy={cy} nmPerPx={nmPerPx}
          allPlanes={allPlanes} partialPlanes={partialPlanes} />
      </div>
    </div>
  );
}

// Replace fake sin/cos signal trace with a real msgs/sec sparkline. The
// backend doesn't expose per-frame SNR, so showing a live signal trace would
// be misleading; the message rate is the closest honest substitute.
function MsgRateTrace({ history, pal }) {
  const max = Math.max(1, ...history);
  const w = 400, h = 42, n = 80;
  const data = history.slice(-n);
  if (data.length === 0) {
    return (
      <div style={{
        height: h, display: 'flex', alignItems: 'center', justifyContent: 'center',
        fontSize: 9, color: pal.dim, letterSpacing: 1.5,
      }}>NO DATA</div>
    );
  }
  const barW = w / n - 1.6;
  return (
    <svg width="100%" height={h} viewBox={`0 0 ${w} ${h}`} preserveAspectRatio="none">
      {data.map((v, i) => {
        const bh = Math.max(1, (v / max) * (h - 2));
        const x = (n - data.length + i) * (w / n);
        return <rect key={i} x={x} y={h - bh} width={barW} height={bh}
          fill={pal.fg} opacity={0.55 + (i / data.length) * 0.4} />;
      })}
    </svg>
  );
}

function ScopeClassicSidebar({ w, h, sim, pal, selected, onSelect, project, cx, cy, nmPerPx,
                               allPlanes, partialPlanes }) {
  const sorted = [...allPlanes].sort((a, b) => {
    const sa = project(a.lat, a.lon), sb = project(b.lat, b.lon);
    const da = Math.hypot(sa.x - cx, sa.y - cy), db = Math.hypot(sb.x - cx, sb.y - cy);
    return da - db;
  });
  const partialSorted = [...partialPlanes].sort((a, b) => (b.lastSeenMs ?? 0) - (a.lastSeenMs ?? 0));

  const Stat = ({ label, value, sub }) => (
    <div style={{ flex: 1, padding: '10px 12px', borderRight: `1px solid ${pal.faint}` }}>
      <div style={{ fontSize: 9, color: pal.dim, letterSpacing: 1.5, textTransform: 'uppercase' }}>{label}</div>
      <div style={{ fontSize: 22, color: pal.fg, fontWeight: 600, marginTop: 4, textShadow: `0 0 6px ${pal.glow}` }}>{value}</div>
      {sub && <div style={{ fontSize: 9, color: pal.dim, marginTop: 2 }}>{sub}</div>}
    </div>
  );

  const rateMax = Math.max(1, ...(sim.stats.msgRateHistory ?? []));

  return (
    <div style={{ width: w, height: h, borderLeft: `1px solid ${pal.faint}`,
      display: 'flex', flexDirection: 'column', fontSize: 11 }}>

      {/* RX STATS */}
      <div style={{ display: 'flex', borderBottom: `1px solid ${pal.faint}` }}>
        <Stat label="MSGS/SEC" value={(sim.stats.msgsLastSec ?? 0).toString().padStart(3, '0')} />
        <Stat label="CRC FAIL" value={`${(((sim.stats.crcFailLastSec ?? 0) / Math.max(1, sim.stats.msgsLastSec ?? 1)) * 100).toFixed(1)}%`} />
        <Stat label="BUF FILL" value={`${sim.stats.bufFillPct ?? 0}%`} sub={`${sim.stats.bufOverflows ?? 0} overflows`} />
      </div>

      {/* message-rate trace (replaces the previous fake signal sparkline) */}
      <div style={{ padding: '10px 14px', borderBottom: `1px solid ${pal.faint}` }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 9, color: pal.dim, marginBottom: 4 }}>
          <span>MSG RATE</span>
          <span>0 ─ {rateMax} msg/s</span>
        </div>
        <MsgRateTrace history={sim.stats.msgRateHistory ?? []} pal={pal} />
      </div>

      {/* FLEET */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        <FleetListHeader pal={pal} label="IN RANGE" count={sorted.length} />
        <div style={{ overflow: 'auto', flex: 1 }}>
          {sorted.map((p) => (
            <FleetRow key={p.icao} p={p} pal={pal} selected={selected}
              onSelect={onSelect} project={project} cx={cx} cy={cy} nmPerPx={nmPerPx} />
          ))}
          {partialSorted.length > 0 && (
            <>
              <FleetListHeader pal={pal} label="CONTACT (no fix)" count={partialSorted.length} />
              {partialSorted.map((p) => (
                <FleetRow key={p.icao} p={p} pal={pal} selected={selected}
                  onSelect={onSelect} partial />
              ))}
            </>
          )}
        </div>
      </div>

      {/* SELECTED AIRCRAFT */}
      <SelectedPanel selected={selected} pal={pal} />
    </div>
  );
}

function FleetListHeader({ pal, label, count }) {
  return (
    <div style={{
      padding: '8px 14px', display: 'grid', gridTemplateColumns: '70px 50px 50px 44px 1fr',
      fontSize: 9, color: pal.dim, letterSpacing: 1.2, borderBottom: `1px solid ${pal.faint}`,
      background: pal.faint, textTransform: 'uppercase',
    }}>
      <span style={{ gridColumn: '1 / 5', fontWeight: 600 }}>{label} · {count}</span>
      <span style={{ textAlign: 'right' }}>RNG</span>
    </div>
  );
}

function FleetRow({ p, pal, selected, onSelect, project, cx, cy, nmPerPx, partial = false }) {
  const isSel = selected !== null && p.icao === selected.icao;
  const dash = '—';
  let rng = dash;
  if (!partial && project) {
    const s = project(p.lat, p.lon);
    rng = (Math.hypot(s.x - cx, s.y - cy) * nmPerPx).toFixed(1) + 'NM';
  }
  return (
    <div onClick={() => onSelect(p.icao)}
      style={{
        padding: '6px 14px', display: 'grid', gridTemplateColumns: '70px 50px 50px 44px 1fr',
        fontSize: 10.5,
        color: isSel ? pal.bg : (partial ? pal.dim : pal.fg),
        background: isSel ? pal.fg : 'transparent',
        borderBottom: `1px solid ${pal.faint}`, cursor: 'pointer',
      }}>
      <span style={{ fontWeight: 600 }}>{p.cs || fmt.icao(p.icao)}</span>
      <span>{p.alt != null ? Math.round(p.alt / 100) * 100 : dash}</span>
      <span>{p.spd != null ? Math.round(p.spd) : dash}</span>
      <span>{p.hdg != null ? fmt.hdg(p.hdg) : dash}</span>
      <span style={{ textAlign: 'right' }}>{rng}</span>
    </div>
  );
}

function SelectedPanel({ selected, pal }) {
  if (selected === null) {
    return (
      <div style={{ borderTop: `1px solid ${pal.faint}`, padding: '12px 14px', minHeight: 160 }}>
        <div style={{ fontSize: 9, color: pal.dim, letterSpacing: 1.5, paddingTop: 8 }}>
          NO AIRCRAFT IN RANGE
        </div>
      </div>
    );
  }
  return (
    <div style={{ borderTop: `1px solid ${pal.faint}`, padding: '12px 14px', minHeight: 160 }}>
      <div style={{ fontSize: 9, color: pal.dim, letterSpacing: 1.5, marginBottom: 6 }}>
        SELECTED · ICAO {fmt.icao(selected.icao)}
      </div>
      <div style={{ fontSize: 28, fontWeight: 700, color: pal.fg, textShadow: `0 0 8px ${pal.glow}`, letterSpacing: 1 }}>
        {selected.cs || fmt.icao(selected.icao)}
      </div>
      <div style={{ fontSize: 10.5, color: pal.dim, marginTop: 2, display: 'flex', gap: 12 }}>
        {selected.model && <span>{selected.model}</span>}
        {selected.model && <span>·</span>}
        {selected.kind  && <span>{selected.kind.toUpperCase()}</span>}
        {selected.kind  && <span>·</span>}
        {selected.sqk    && <span>SQK {selected.sqk}</span>}
      </div>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 6, marginTop: 12, fontSize: 11 }}>
        {[
          ['ALT', `${Math.round(selected.alt ?? 0).toLocaleString()}ft`],
          ['SPD', `${Math.round(selected.spd ?? 0)}kt`],
          ['HDG', `${fmt.hdg(selected.hdg ?? 0)}°`],
          ['V/S', `${(selected.vs ?? 0) > 0 ? '+' : ''}${Math.round((selected.vs ?? 0) / 100) * 100}fpm`],
          ['LAT', (selected.lat ?? 0).toFixed(4)],
          ['LON', (selected.lon ?? 0).toFixed(4)],
        ].map(([k, v]) => (
          <div key={k} style={{ padding: '6px 8px', border: `1px solid ${pal.faint}` }}>
            <div style={{ fontSize: 8.5, color: pal.dim, letterSpacing: 1 }}>{k}</div>
            <div style={{ color: pal.fg, marginTop: 2 }}>{v}</div>
          </div>
        ))}
      </div>
      <div style={{ marginTop: 8, fontSize: 9.5, color: pal.dim }}>
        MSGS {selected.msgsRx ?? 0} · LAST SEEN {(selected.lastSeenMs ? ((Date.now() - selected.lastSeenMs) / 1000).toFixed(1) : '—')}s
      </div>
    </div>
  );
}

window.ScopeClassic = ScopeClassic;
