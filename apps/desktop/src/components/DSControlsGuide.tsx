/**
 * DSControlsGuide — Inline reference card for Nintendo DS controls.
 *
 * Shows button mappings for Xbox, PlayStation, and keyboard, plus
 * a touchscreen explanation and screen layout overview.
 * Designed to be embedded in the lobby page or a help modal.
 */

interface DSControlsGuideProps {
  /** Compact mode hides the layout diagram and only shows the mapping table. */
  compact?: boolean;
  /** Show the DSi mode setup section (for DSiWare sessions). */
  showDsiSection?: boolean;
}

const DS_BUTTON_ROWS: Array<{ ds: string; xbox: string; ps: string; keyboard: string }> = [
  { ds: 'A',        xbox: 'A',          ps: 'Cross (×)',   keyboard: 'X' },
  { ds: 'B',        xbox: 'B',          ps: 'Circle (○)',  keyboard: 'Z' },
  { ds: 'X',        xbox: 'X',          ps: 'Square (□)',  keyboard: 'S' },
  { ds: 'Y',        xbox: 'Y',          ps: 'Triangle (△)', keyboard: 'A' },
  { ds: 'L',        xbox: 'LB',         ps: 'L1',          keyboard: 'Q' },
  { ds: 'R',        xbox: 'RB',         ps: 'R1',          keyboard: 'W' },
  { ds: 'Start',    xbox: 'Menu',       ps: 'Options',     keyboard: 'Enter' },
  { ds: 'Select',   xbox: 'View',       ps: 'Share/Create', keyboard: 'Right Shift' },
  { ds: 'D-Pad',    xbox: 'D-Pad / L-Stick', ps: 'D-Pad / L-Stick', keyboard: '↑↓←→' },
  { ds: 'Touch',    xbox: 'Mouse click', ps: 'Mouse click', keyboard: 'Mouse click' },
];

const SCREEN_LAYOUTS: Array<{ id: string; label: string; description: string }> = [
  {
    id: 'stacked',
    label: 'Stacked (default)',
    description: 'Top screen above, bottom screen below — mirrors the real DS hardware.',
  },
  {
    id: 'side-by-side',
    label: 'Side by Side',
    description: 'Top and bottom screens next to each other — useful on wide monitors.',
  },
  {
    id: 'top-focus',
    label: 'Top Focus',
    description: 'Top screen enlarged, bottom screen shrunk — best for action/platformer games.',
  },
  {
    id: 'bottom-focus',
    label: 'Bottom Focus',
    description: 'Bottom screen enlarged, top shrunk — ideal for touch-heavy games like Phantom Hourglass.',
  },
];

export function DSControlsGuide({ compact = false, showDsiSection = false }: DSControlsGuideProps) {
  return (
    <div className="space-y-4">
      {/* Button mapping table */}
      <div>
        <p className="text-xs font-semibold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
          🎮 Button Mapping
        </p>
        <div
          className="rounded-xl overflow-hidden text-xs"
          style={{ backgroundColor: 'var(--color-oasis-surface)' }}
        >
          {/* Header */}
          <div
            className="grid grid-cols-4 px-3 py-2 font-semibold"
            style={{ backgroundColor: 'rgba(255,255,255,0.05)', color: 'var(--color-oasis-text-muted)' }}
          >
            <span>DS Button</span>
            <span>Xbox</span>
            <span>PlayStation</span>
            <span>Keyboard</span>
          </div>
          {DS_BUTTON_ROWS.map((row, i) => (
            <div
              key={row.ds}
              className="grid grid-cols-4 px-3 py-1.5"
              style={{
                borderTop: i > 0 ? '1px solid rgba(255,255,255,0.04)' : undefined,
                color: 'var(--color-oasis-text)',
              }}
            >
              <span className="font-semibold" style={{ color: '#E87722' }}>{row.ds}</span>
              <span>{row.xbox}</span>
              <span>{row.ps}</span>
              <span className="font-mono">{row.keyboard}</span>
            </div>
          ))}
        </div>
      </div>

      {/* Touchscreen explanation */}
      <div
        className="rounded-xl px-4 py-3 text-xs"
        style={{ backgroundColor: 'var(--color-oasis-surface)' }}
      >
        <p className="font-semibold mb-1" style={{ color: 'var(--color-oasis-text)' }}>
          📱 Touchscreen Controls
        </p>
        <p style={{ color: 'var(--color-oasis-text-muted)' }}>
          The DS bottom screen is touch-operated. In melonDS, <strong>move your mouse to the bottom
          screen area and click</strong> to simulate touch input. This works for map taps in
          Phantom Hourglass, GTS and Union Room navigation in Pokémon, menu interactions, and
          any other touch-only actions.
        </p>
        <p className="mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Tip: Press <kbd className="text-[10px] px-1 py-0.5 rounded border border-white/20">F11</kbd> to
          swap the top and bottom screens at any time.
        </p>
      </div>

      {/* Screen layout selector (full mode only) */}
      {!compact && (
        <div>
          <p className="text-xs font-semibold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            🖥️ Screen Layout Options
          </p>
          <div className="grid grid-cols-2 gap-2">
            {SCREEN_LAYOUTS.map((layout) => (
              <div
                key={layout.id}
                className="rounded-xl px-3 py-2 text-xs"
                style={{ backgroundColor: 'var(--color-oasis-surface)' }}
              >
                <p className="font-semibold mb-0.5" style={{ color: 'var(--color-oasis-text)' }}>
                  {layout.label}
                </p>
                <p style={{ color: 'var(--color-oasis-text-muted)' }}>{layout.description}</p>
              </div>
            ))}
          </div>
          <p className="text-[10px] mt-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Layout is set when hosting a room. The host&apos;s chosen layout is used for the session.
          </p>
        </div>
      )}

      {/* WFC info */}
      <div
        className="rounded-xl px-4 py-3 text-xs"
        style={{ backgroundColor: 'rgba(37,99,235,0.12)' }}
      >
        <p className="font-semibold mb-1" style={{ color: '#93c5fd' }}>
          🌐 Wi-Fi Connection (WFC)
        </p>
        <p style={{ color: 'var(--color-oasis-text-muted)' }}>
          Games like Pokémon Diamond/Pearl/Platinum, Mario Kart DS, and Metroid Prime Hunters
          originally used Nintendo&apos;s Wi-Fi Connection service (now offline). RetroOasis automatically
          routes these games through <strong>Wiimmfi</strong> — a community-run replacement server
          that re-enables all original online features including the GTS, Battle Tower, Union Room,
          and friend battles.
        </p>
        <p className="mt-1 font-mono text-[10px]" style={{ color: '#93c5fd' }}>
          DNS: 178.62.43.212 (configured automatically)
        </p>
      </div>

      {/* DSi Mode section */}
      {showDsiSection && (
        <div
          className="rounded-xl px-4 py-3 text-xs"
          style={{ backgroundColor: 'rgba(3,105,161,0.12)' }}
        >
          <p className="font-semibold mb-1" style={{ color: '#7dd3fc' }}>
            🟦 DSi Mode (DSiWare)
          </p>
          <p style={{ color: 'var(--color-oasis-text-muted)' }}>
            DSiWare titles run on the Nintendo DSi hardware revision and require additional BIOS files.
            RetroOasis automatically launches melonDS with <code className="font-mono">--dsi-mode</code> for
            DSiWare sessions.
          </p>
          <p className="mt-2 font-semibold text-[10px]" style={{ color: '#7dd3fc' }}>
            Required files in <code className="font-mono">~/.config/melonDS/</code>:
          </p>
          <ul className="mt-1 space-y-0.5 font-mono text-[10px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
            <li>• bios7i.bin — DSi ARM7 BIOS</li>
            <li>• bios9i.bin — DSi ARM9 BIOS</li>
            <li>• nand.bin — DSi NAND (system memory)</li>
          </ul>
          <p className="mt-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            These files can be dumped from a real DSi using the <strong>dsibinhax</strong> tool.
            Standard DS BIOS files (bios7.bin, bios9.bin, firmware.bin) are also required.
          </p>
        </div>
      )}
    </div>
  );
}
