/* scope-shared.jsx — shared building blocks for the three scope artboards.
   Exports plane glyphs, data tags, range rings, compass rose, terrain layer. */

// Plane glyph styles ─────────────────────────────────────────────────────────

function PlaneTriangle({ x, y, hdg, kind, isSelected, color }) {
  // small filled triangle pointing in heading direction (north = up = -y)
  const size = isSelected ? 7 : 5.5;
  return (
    <g transform={`translate(${x} ${y}) rotate(${hdg})`}>
      <path
        d={`M 0 ${-size} L ${size * 0.85} ${size * 0.85} L 0 ${size * 0.35} L ${-size * 0.85} ${size * 0.85} Z`}
        fill={color}
        stroke="none"
      />
    </g>
  );
}

function PlaneSilhouette({ x, y, hdg, kind, isSelected, color }) {
  // top-down aircraft silhouette: fuselage + wings + tail
  const s = isSelected ? 1.2 : 1.0;
  return (
    <g transform={`translate(${x} ${y}) rotate(${hdg})`} fill={color}>
      {/* wings */}
      <path d={`M ${-9 * s} ${1 * s} L ${9 * s} ${1 * s} L ${8 * s} ${2.5 * s} L ${-8 * s} ${2.5 * s} Z`} />
      {/* fuselage */}
      <path d={`M ${-1.2 * s} ${-7 * s} L ${1.2 * s} ${-7 * s} L ${1.6 * s} ${5 * s} L ${-1.6 * s} ${5 * s} Z`} />
      {/* tail */}
      <path d={`M ${-3 * s} ${4 * s} L ${3 * s} ${4 * s} L ${2.4 * s} ${5.5 * s} L ${-2.4 * s} ${5.5 * s} Z`} />
    </g>
  );
}

function PlaneChevron({ x, y, hdg, kind, isSelected, color }) {
  const s = isSelected ? 1.3 : 1.0;
  return (
    <g transform={`translate(${x} ${y}) rotate(${hdg})`}>
      <path
        d={`M 0 ${-7 * s} L ${5 * s} ${5 * s} L 0 ${2.5 * s} L ${-5 * s} ${5 * s} Z`}
        fill="none"
        stroke={color}
        strokeWidth={isSelected ? 1.6 : 1.1}
        strokeLinejoin="round"
      />
    </g>
  );
}

// Data tag styles ───────────────────────────────────────────────────────────

function DataTagFull({ x, y, plane, color, dim, selected }) {
  // FlightRadar-style stacked block, anchored with a leader line offset up-right
  const ox = 14, oy = -28;
  const lines = [plane.cs, `${Math.round(plane.alt / 100) * 100}`.padStart(5, ' '), `${Math.round(plane.spd)}kt`];
  const w = 78, h = 38, lineH = 11;
  return (
    <g transform={`translate(${x} ${y})`}>
      <line x1={2} y1={-2} x2={ox} y2={oy + h - 4} stroke={dim} strokeWidth={0.6} />
      <rect x={ox} y={oy} width={w} height={h}
        fill="rgba(0,0,0,0.55)"
        stroke={selected ? color : dim}
        strokeWidth={selected ? 1.1 : 0.6} />
      {lines.map((t, i) => (
        <text key={i} x={ox + 5} y={oy + 11 + i * lineH}
          fill={color} fontFamily="JetBrains Mono, monospace" fontSize={9.5}
          style={{ letterSpacing: 0.5 }}>{t}</text>
      ))}
    </g>
  );
}

function DataTagCompact({ x, y, plane, color, dim, selected }) {
  const ox = 12, oy = -22;
  const cs = plane.cs;
  const sub = `${Math.round(plane.alt / 100) * 100} · ${Math.round(plane.spd)}`;
  const w = Math.max(cs.length, sub.length) * 6.2 + 10;
  return (
    <g transform={`translate(${x} ${y})`}>
      <line x1={2} y1={-2} x2={ox + 2} y2={oy + 22} stroke={dim} strokeWidth={0.5} />
      <text x={ox + 4} y={oy + 9} fill={color} fontFamily="JetBrains Mono, monospace" fontSize={9.5} fontWeight={500}>{cs}</text>
      <text x={ox + 4} y={oy + 20} fill={dim} fontFamily="JetBrains Mono, monospace" fontSize={8.5}>{sub}</text>
    </g>
  );
}

function DataTagPill({ x, y, plane, color, dim, selected }) {
  const ox = 11, oy = -8;
  const txt = `${plane.cs}·${Math.round(plane.alt / 100)}`;
  const w = txt.length * 6.0 + 12;
  return (
    <g transform={`translate(${x} ${y})`}>
      <rect x={ox} y={oy - 7} width={w} height={14} rx={7} ry={7}
        fill="rgba(0,0,0,0.7)" stroke={selected ? color : dim} strokeWidth={selected ? 1.1 : 0.6} />
      <text x={ox + 6} y={oy + 2.5} fill={color} fontFamily="JetBrains Mono, monospace" fontSize={9}>{txt}</text>
    </g>
  );
}

// Range rings ───────────────────────────────────────────────────────────────

function RangeRings({ cx, cy, ringNm, ringCount, nmPerPx, color, dim, labelEvery = 1, withLabels = true }) {
  const rings = [];
  for (let i = 1; i <= ringCount; i++) {
    const r = (i * ringNm) / nmPerPx;
    rings.push(
      <circle key={i} cx={cx} cy={cy} r={r}
        fill="none" stroke={dim} strokeWidth={0.7}
        strokeDasharray={i === ringCount ? 'none' : '3 4'} />
    );
    if (withLabels && i % labelEvery === 0) {
      rings.push(
        <text key={`l${i}`} x={cx + r + 4} y={cy + 3}
          fill={dim} fontFamily="JetBrains Mono, monospace" fontSize={9}>
          {i * ringNm}NM
        </text>
      );
    }
  }
  // crosshair
  rings.push(
    <line key="hx" x1={cx - 8} y1={cy} x2={cx + 8} y2={cy} stroke={color} strokeWidth={1} />,
    <line key="hy" x1={cx} y1={cy - 8} x2={cx} y2={cy + 8} stroke={color} strokeWidth={1} />,
    <circle key="ctr" cx={cx} cy={cy} r={2.5} fill={color} />
  );
  return <g>{rings}</g>;
}

// Compass rose ──────────────────────────────────────────────────────────────

function CompassRose({ cx, cy, r, color, dim, dense = true }) {
  const ticks = [];
  for (let deg = 0; deg < 360; deg += dense ? 5 : 10) {
    const a = ((deg - 90) * Math.PI) / 180;
    const isMajor = deg % 30 === 0;
    const isMid = deg % 10 === 0;
    const len = isMajor ? 10 : isMid ? 6 : 3;
    const x1 = cx + Math.cos(a) * (r - len);
    const y1 = cy + Math.sin(a) * (r - len);
    const x2 = cx + Math.cos(a) * r;
    const y2 = cy + Math.sin(a) * r;
    ticks.push(<line key={deg} x1={x1} y1={y1} x2={x2} y2={y2} stroke={isMajor ? color : dim} strokeWidth={isMajor ? 0.9 : 0.5} />);
    if (isMajor) {
      const tx = cx + Math.cos(a) * (r - 22);
      const ty = cy + Math.sin(a) * (r - 22);
      const label = deg === 0 ? 'N' : deg === 90 ? 'E' : deg === 180 ? 'S' : deg === 270 ? 'W' : deg.toString().padStart(3, '0');
      ticks.push(
        <text key={`t${deg}`} x={tx} y={ty + 4} fill={color}
          fontFamily="JetBrains Mono, monospace" fontSize={deg % 90 === 0 ? 12 : 9}
          textAnchor="middle" fontWeight={deg % 90 === 0 ? 600 : 400}>{label}</text>
      );
    }
  }
  return <g>{ticks}</g>;
}

// Terrain layer ─────────────────────────────────────────────────────────────

function TerrainLayer({ paths, color, baseOpacity = 0.18, mode = 'contour' }) {
  // mode: 'contour' = stroked rings; 'banded' = filled with low alpha; 'detail' = many thin strokes
  return (
    <g>
      {paths.map((c, i) => {
        const t = c.maxLevel ? c.level / c.maxLevel : 0; // 0=outer, 1=inner peak
        if (mode === 'banded') {
          return (
            <path key={i} d={c.d} fill={color} fillOpacity={baseOpacity * (0.15 + t * 0.55)}
              stroke={color} strokeOpacity={baseOpacity * 1.2} strokeWidth={0.4} />
          );
        }
        return (
          <path key={i} d={c.d} fill="none" stroke={color}
            strokeOpacity={baseOpacity * (0.4 + t * 0.7)}
            strokeWidth={mode === 'detail' ? 0.45 : 0.7} />
        );
      })}
    </g>
  );
}

function CoastLayer({ d, color, opacity = 0.4, width = 1 }) {
  return <path d={d} fill="none" stroke={color} strokeOpacity={opacity} strokeWidth={width} />;
}

// Palette definitions ───────────────────────────────────────────────────────

const LPT_PALETTES = {
  phosphor: {
    name: 'PHOSPHOR',
    fg: '#39ff14',
    dim: '#1c7f35',
    faint: '#082716',
    glow: 'rgba(57,255,20,0.45)',
    bg: '#040b06',
    warn: '#ffb84d',
    sel: '#e6fff0',
  },
  amber: {
    name: 'AMBER',
    fg: '#ffb300',
    dim: '#8a5d09',
    faint: '#2f1e00',
    glow: 'rgba(255,179,0,0.45)',
    bg: '#0a0600',
    warn: '#ff5a3b',
    sel: '#fff1d6',
  },
  white: {
    name: 'WHITE',
    fg: '#e8e8e8',
    dim: '#6f767d',
    faint: '#1b1b1b',
    glow: 'rgba(232,232,232,0.35)',
    bg: '#050505',
    warn: '#ff8a4d',
    sel: '#ffffff',
  },
};

Object.assign(window, { LPT_PALETTES });

// Palette hook ──────────────────────────────────────────────────────────────

function usePalette() {
  const [pal, setPal] = React.useState(() => readPalette());
  React.useEffect(() => {
    const handler = () => setPal(readPalette());
    window.addEventListener('lpt-palette', handler);
    return () => window.removeEventListener('lpt-palette', handler);
  }, []);
  return pal;
}

function readPalette() {
  const fallback = LPT_PALETTES.phosphor;
  const r = getComputedStyle(document.documentElement);
  return {
    fg:     r.getPropertyValue('--lpt-fg').trim()     || fallback.fg,
    dim:    r.getPropertyValue('--lpt-dim').trim()    || fallback.dim,
    faint:  r.getPropertyValue('--lpt-faint').trim()  || fallback.faint,
    glow:   r.getPropertyValue('--lpt-glow').trim()   || fallback.glow,
    bg:     r.getPropertyValue('--lpt-bg').trim()     || fallback.bg,
    warn:   r.getPropertyValue('--lpt-warn').trim()   || fallback.warn,
    sel:    r.getPropertyValue('--lpt-sel').trim()    || fallback.sel,
  };
}

// Knobs hook — reads window.LPT_KNOBS, re-renders on 'lpt-knobs' event.
function useKnobs() {
  const [k, setK] = React.useState(() => window.LPT_KNOBS || { scanlines: 0.6, sweepSpeed: 12, glow: 1, vignette: 0.85 });
  React.useEffect(() => {
    const h = () => setK({ ...(window.LPT_KNOBS || {}) });
    window.addEventListener('lpt-knobs', h);
    return () => window.removeEventListener('lpt-knobs', h);
  }, []);
  return k;
}

Object.assign(window, {
  PlaneTriangle, PlaneSilhouette, PlaneChevron,
  DataTagFull, DataTagCompact, DataTagPill,
  RangeRings, CompassRose, TerrainLayer, CoastLayer,
  usePalette, readPalette, useKnobs,
});
