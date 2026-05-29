/* app-classic.jsx — full-bleed standalone ScopeClassic with extended Tweaks. */

const PALETTES = window.LPT_PALETTES;
const PALETTE_HEX = {
  [PALETTES.phosphor.fg]: 'phosphor',
  [PALETTES.amber.fg]: 'amber',
  [PALETTES.white.fg]: 'white',
};
const HEX_FOR = (key) => PALETTES[key]?.fg || PALETTES.phosphor.fg;

function applyPalette(key) {
  const p = PALETTES[key] || PALETTES.phosphor;
  const r = document.documentElement.style;
  r.setProperty('--lpt-fg',    p.fg);
  r.setProperty('--lpt-dim',   p.dim);
  r.setProperty('--lpt-faint', p.faint);
  r.setProperty('--lpt-glow',  p.glow);
  r.setProperty('--lpt-bg',    p.bg);
  r.setProperty('--lpt-warn',  p.warn);
  r.setProperty('--lpt-sel',   p.sel);
  window.dispatchEvent(new Event('lpt-palette'));
}

const TWEAK_DEFAULTS = /*EDITMODE-BEGIN*/{
  "palette": "phosphor",
  "scanlines": 0.6,
  "sweepSpeed": 12,
  "glow": 1,
  "vignette": 0.85,
  "trailLength": 20,
  "ringSpacingNm": 20,
  "altMinFt": 0,
  "altMaxFt": 60000,
  "centerOnSelected": false
}/*EDITMODE-END*/;

// CSV export — one row per aircraft currently in the scope.
function exportFleetCsv(planes) {
  const header = 'icao,callsign,lat,lon,alt_ft,spd_kt,hdg_deg,vs_fpm,msgs_rx\n';
  const rows = planes.map((p) => [
    p.icao,
    (p.cs ?? '').replace(/[",\n]/g, ''),
    (p.lat ?? 0).toFixed(6),
    (p.lon ?? 0).toFixed(6),
    Math.round(p.alt ?? 0),
    Math.round(p.spd ?? 0),
    Math.round(p.hdg ?? 0),
    Math.round(p.vs ?? 0),
    p.msgsRx ?? 0,
  ].join(',')).join('\n');

  const blob = new Blob([header + rows + (rows ? '\n' : '')], { type: 'text/csv' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `lpt-export-${new Date().toISOString().slice(0, 19).replace(/[:T]/g, '-')}.csv`;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(a.href);
}

// ── viewport scaler — center 1280×800 on screen, letterbox black ──
function Stage({ children, w, h }) {
  const [scale, setScale] = React.useState(1);
  React.useEffect(() => {
    const compute = () => {
      const s = Math.min(window.innerWidth / w, window.innerHeight / h);
      setScale(s);
    };
    compute();
    window.addEventListener('resize', compute);
    return () => window.removeEventListener('resize', compute);
  }, [w, h]);
  return (
    <div style={{ position: 'fixed', inset: 0, background: '#000',
      display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
      <div style={{ width: w, height: h, transform: `scale(${scale})`, transformOrigin: 'center center' }}>
        {children}
      </div>
    </div>
  );
}

// global tweak knobs we expose on window so ScopeClassic can read them without
// having to thread props through (and so this file owns all tweaking surface).
window.LPT_KNOBS = {
  scanlines: 0.6, sweepSpeed: 12, glow: 1, vignette: 0.85, trailLength: 20,
  ringSpacingNm: 20, altMinFt: 0, altMaxFt: 60000, centerOnSelected: false,
};

function App() {
  React.useEffect(() => { applyPalette(TWEAK_DEFAULTS.palette); }, []);
  const t = useTweaks(TWEAK_DEFAULTS);

  React.useEffect(() => { applyPalette(t.palette); }, [t.palette]);
  React.useEffect(() => {
    window.LPT_KNOBS = {
      scanlines: t.scanlines, sweepSpeed: t.sweepSpeed, glow: t.glow,
      vignette: t.vignette, trailLength: t.trailLength,
      ringSpacingNm: t.ringSpacingNm,
      altMinFt: t.altMinFt, altMaxFt: t.altMaxFt,
      centerOnSelected: t.centerOnSelected,
    };
    window.dispatchEvent(new Event('lpt-knobs'));
  }, [
    t.scanlines, t.sweepSpeed, t.glow, t.vignette, t.trailLength,
    t.ringSpacingNm, t.altMinFt, t.altMaxFt, t.centerOnSelected,
  ]);

  return (
    <>
      <Stage w={1280} h={800}>
        <ScopeClassic w={1280} h={800} />
      </Stage>

      <TweaksPanel title="Tweaks">
        <TweakSection label="Palette">
          <TweakColor
            label="Palette"
            value={HEX_FOR(t.palette)}
            options={['#39ff14', '#ffb300', '#e8e8e8']}
            onChange={(c) => t.setTweak('palette', PALETTE_HEX[c] || 'phosphor')}
          />
          <TweakSlider label="Glow"     value={t.glow}     min={0} max={2}  step={0.05}
            onChange={(v) => t.setTweak('glow', v)} />
        </TweakSection>

        <TweakSection label="CRT">
          <TweakSlider label="Scanlines" value={t.scanlines} min={0} max={1}  step={0.05}
            onChange={(v) => t.setTweak('scanlines', v)} />
          <TweakSlider label="Vignette"  value={t.vignette}  min={0} max={1}  step={0.05}
            onChange={(v) => t.setTweak('vignette', v)} />
          <TweakSlider label="Sweep"     value={t.sweepSpeed} unit="s" min={4} max={30} step={1}
            onChange={(v) => t.setTweak('sweepSpeed', v)} />
        </TweakSection>

        <TweakSection label="Scope">
          <TweakSlider label="Ring NM"  value={t.ringSpacingNm} unit="NM" min={5} max={100} step={5}
            onChange={(v) => t.setTweak('ringSpacingNm', v)} />
          <TweakToggle label="Center on selected" value={t.centerOnSelected}
            onChange={(v) => t.setTweak('centerOnSelected', v)} />
        </TweakSection>

        <TweakSection label="Filters">
          <TweakSlider label="Alt min" value={t.altMinFt} unit="ft" min={0} max={60000} step={1000}
            onChange={(v) => t.setTweak('altMinFt', v)} />
          <TweakSlider label="Alt max" value={t.altMaxFt} unit="ft" min={0} max={60000} step={1000}
            onChange={(v) => t.setTweak('altMaxFt', v)} />
        </TweakSection>

        <TweakSection label="Tracks">
          <TweakSlider label="Trail pts" value={t.trailLength} min={5} max={100} step={5}
            onChange={(v) => t.setTweak('trailLength', v)} />
        </TweakSection>

        <TweakSection label="Export">
          <TweakButton label="Download CSV"
            onClick={() => exportFleetCsv(window.LPTSim?.planes ?? [])} />
        </TweakSection>
      </TweaksPanel>
    </>
  );
}

ReactDOM.createRoot(document.getElementById('root')).render(<App />);
