/* app-classic.jsx — full-bleed standalone ScopeClassic with extended Tweaks. */

const PALETTES = {
  phosphor: { name: 'PHOSPHOR', fg: '#33ff77', dim: '#1f8a46', faint: '#0a3318', glow: 'rgba(51,255,119,0.45)', bg: '#020a05', warn: '#ffb84d', sel: '#e6fff0' },
  amber:    { name: 'AMBER',    fg: '#ffb240', dim: '#a06a18', faint: '#3a2607', glow: 'rgba(255,178,64,0.5)',  bg: '#0a0602', warn: '#ff5a3b', sel: '#fff1d6' },
  cyan:     { name: 'CYAN',     fg: '#39d8ff', dim: '#1a7693', faint: '#072a35', glow: 'rgba(57,216,255,0.45)', bg: '#02080b', warn: '#ff7a4d', sel: '#e0f7ff' },
  ice:      { name: 'ICE',      fg: '#e2e9ef', dim: '#65737f', faint: '#1a2128', glow: 'rgba(226,233,239,0.35)',bg: '#05080b', warn: '#ff8a4d', sel: '#ffffff' },
};
const PALETTE_HEX = { '#33ff77': 'phosphor', '#ffb240': 'amber', '#39d8ff': 'cyan', '#e2e9ef': 'ice' };
const HEX_FOR = (key) => Object.entries(PALETTE_HEX).find(([h, k]) => k === key)?.[0] || '#33ff77';

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
  "trailLength": 20
}/*EDITMODE-END*/;

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
window.LPT_KNOBS = { scanlines: 0.6, sweepSpeed: 12, glow: 1, vignette: 0.85, trailLength: 20 };

function App() {
  React.useEffect(() => { applyPalette(TWEAK_DEFAULTS.palette); }, []);
  const t = useTweaks(TWEAK_DEFAULTS);

  React.useEffect(() => { applyPalette(t.palette); }, [t.palette]);
  React.useEffect(() => {
    window.LPT_KNOBS = { scanlines: t.scanlines, sweepSpeed: t.sweepSpeed, glow: t.glow, vignette: t.vignette, trailLength: t.trailLength };
    window.dispatchEvent(new Event('lpt-knobs'));
  }, [t.scanlines, t.sweepSpeed, t.glow, t.vignette, t.trailLength]);

  return (
    <>
      <Stage w={1280} h={800}>
        <ScopeClassic w={1280} h={800} />
      </Stage>

      <TweaksPanel title="Tweaks">
        <TweakSection label="Phosphor">
          <TweakColor
            label="Palette"
            value={HEX_FOR(t.palette)}
            options={['#33ff77', '#ffb240', '#39d8ff', '#e2e9ef']}
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

        <TweakSection label="Tracks">
          <TweakSlider label="Trail pts" value={t.trailLength} min={5} max={100} step={5}
            onChange={(v) => t.setTweak('trailLength', v)} />
        </TweakSection>
      </TweaksPanel>
    </>
  );
}

ReactDOM.createRoot(document.getElementById('root')).render(<App />);
