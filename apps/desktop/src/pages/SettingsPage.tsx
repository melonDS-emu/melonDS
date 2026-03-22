import { useState, useCallback, useRef } from 'react';
import {
  getRomDirectory,
  setRomDirectory,
  setSaveDirectory,
} from '../lib/rom-settings';
import { InputProfileManager } from '@retro-oasis/multiplayer-profiles';
import type { InputProfile, InputBinding } from '@retro-oasis/multiplayer-profiles';
import {
  getCustomProfile,
  saveCustomProfile,
  resetCustomProfile,
  mergeWithCustomProfiles,
} from '../lib/custom-profiles';
import {
  loadTouchCalibration,
  saveTouchCalibration,
  resetTouchCalibration,
  applyCalibration,
  DEFAULT_CALIBRATION,
} from '../lib/touch-calibration';
import type { DsTouchCalibration } from '../lib/touch-calibration';
import { tauriPickDirectory, isTauri } from '../lib/tauri-ipc';
import {
  EMULATOR_NAMES,
  EMULATOR_DEFAULT_PATHS,
  getEmulatorPath,
  setEmulatorPath,
  clearEmulatorPath,
} from '../lib/emulator-settings';
import {
  isCrashReportingEnabled,
  enableCrashReporting,
  disableCrashReporting,
  clearCrashReports,
  getCrashReports,
  exportCrashReports,
} from '../lib/crash-reporting';
import {
  getRACredentials,
  setRACredentials,
  clearRACredentials,
  isRAConnected,
} from '../lib/retro-achievements-settings';

const SAVE_DIR_KEY = 'retro-oasis-save-directory';
const DISPLAY_NAME_KEY = 'retro-oasis-display-name';

const profileManager = new InputProfileManager();
const DEFAULT_PROFILES = profileManager.listAll();

const SYSTEMS_WITH_PROFILES: string[] = [...new Set(DEFAULT_PROFILES.map((p) => p.system.toUpperCase()))];

function bindingLabel(b: InputBinding): string {
  if (b.key)    return b.key.replace(/^key\(/, '').replace(/\)$/, '').toUpperCase();
  if (b.button) return b.button.replace(/^button\(/, 'Btn ').replace(/^hat\(0 /, '').replace(/\)$/, '');
  if (b.axis)   return b.axis.replace(/^axis\(/, 'Axis ').replace(/\)$/, '');
  return '—';
}

/** Formats a raw user-typed binding string into the canonical internal format. */
function parseUserBinding(raw: string, originalBinding: InputBinding): InputBinding {
  const trimmed = raw.trim().toLowerCase();
  if (!trimmed) return originalBinding;

  // Detect axis notation  e.g. "axis(0+,0-)" or "axis(1-)"
  if (trimmed.startsWith('axis(')) {
    return { action: originalBinding.action, axis: trimmed };
  }
  // Detect hat/button notation  e.g. "hat(0 up)" or "button(4)"
  if (trimmed.startsWith('hat(') || trimmed.startsWith('button(') || trimmed.startsWith('mouse(')) {
    return { action: originalBinding.action, button: trimmed };
  }
  // Detect key notation  e.g. "key(space)"
  if (trimmed.startsWith('key(')) {
    return { action: originalBinding.action, key: trimmed };
  }
  // Plain key name like "space", "a", "return" → wrap in key()
  return { action: originalBinding.action, key: `key(${trimmed})` };
}

// ---------------------------------------------------------------------------
// BindingRow — single editable row within a profile card
// ---------------------------------------------------------------------------

interface BindingRowProps {
  binding: InputBinding;
  index: number;
  editing: boolean;
  onStartEdit: (index: number) => void;
  onCommit: (index: number, raw: string) => void;
  onCancel: () => void;
}

function BindingRow({ binding, index, editing, onStartEdit, onCommit, onCancel }: BindingRowProps) {
  const [draft, setDraft] = useState('');

  function handleKeyDown(e: React.KeyboardEvent<HTMLInputElement>) {
    if (e.key === 'Enter') {
      onCommit(index, draft);
    } else if (e.key === 'Escape') {
      onCancel();
    }
  }

  function handleStartEdit() {
    setDraft(bindingLabel(binding));
    onStartEdit(index);
  }

  return (
    <tr style={{ borderTop: index > 0 ? '1px solid rgba(255,255,255,0.05)' : undefined }}>
      <td className="py-1 pr-3 font-medium" style={{ color: 'var(--color-oasis-text)' }}>
        {binding.action}
      </td>
      <td className="py-1">
        {editing ? (
          <input
            autoFocus
            type="text"
            value={draft}
            onChange={(e) => setDraft(e.target.value)}
            onKeyDown={handleKeyDown}
            onBlur={() => onCommit(index, draft)}
            className="w-full px-2 py-0.5 rounded text-xs font-mono outline-none"
            style={{
              backgroundColor: 'var(--color-oasis-card)',
              color: 'var(--color-oasis-accent-light)',
              border: '1px solid var(--color-oasis-accent)',
            }}
            placeholder="e.g. key(x) or button(0)"
          />
        ) : (
          <button
            type="button"
            onClick={handleStartEdit}
            title="Click to remap"
            className="font-mono text-xs rounded px-1 py-0.5 transition-opacity hover:opacity-70"
            style={{ color: 'var(--color-oasis-accent-light)', background: 'transparent' }}
          >
            {bindingLabel(binding)}
          </button>
        )}
      </td>
    </tr>
  );
}

// ---------------------------------------------------------------------------
// ControllerProfileCard — expandable + editable profile
// ---------------------------------------------------------------------------

interface ProfileCardProps {
  defaultProfile: InputProfile;
  onProfileChange: () => void;
}

function ControllerProfileCard({ defaultProfile, onProfileChange }: ProfileCardProps) {
  const [expanded, setExpanded] = useState(false);
  const [editingIndex, setEditingIndex] = useState<number | null>(null);
  const [saved, setSaved] = useState(false);

  // Load current (possibly customized) profile
  const current = getCustomProfile(defaultProfile.id) ?? defaultProfile;
  const isCustomized = !!getCustomProfile(defaultProfile.id);

  const [bindings, setBindings] = useState<InputBinding[]>(current.bindings);

  const icon = current.controllerType === 'xbox' ? '🎮' : current.controllerType === 'playstation' ? '🕹️' : '⌨️';

  const handleCommit = useCallback(
    (index: number, raw: string) => {
      const updated = bindings.map((b, i) => (i === index ? parseUserBinding(raw, b) : b));
      setBindings(updated);
      setEditingIndex(null);
    },
    [bindings],
  );

  function handleSave() {
    const updated: InputProfile = { ...current, bindings };
    saveCustomProfile(updated);
    setSaved(true);
    setTimeout(() => setSaved(false), 1500);
    onProfileChange();
  }

  function handleReset() {
    resetCustomProfile(defaultProfile.id);
    setBindings(defaultProfile.bindings);
    onProfileChange();
  }

  return (
    <div className="rounded-xl overflow-hidden" style={{ backgroundColor: 'var(--color-oasis-surface)' }}>
      <button
        type="button"
        onClick={() => setExpanded((v) => !v)}
        className="w-full flex items-center justify-between px-4 py-3 text-left"
      >
        <div className="flex items-center gap-2">
          <span>{icon}</span>
          <span className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text)' }}>
            {current.name.replace(/ \(default\)$/, '')}
          </span>
          {isCustomized && (
            <span
              className="text-xs px-1.5 py-0.5 rounded-full"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white', fontSize: '10px' }}
            >
              custom
            </span>
          )}
        </div>
        <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {expanded ? '▲' : '▼'}
        </span>
      </button>

      {expanded && (
        <div className="px-4 pb-3 border-t border-white/10">
          <p className="text-xs mt-2 mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Click any binding to edit it. Type a key name, <code>button(N)</code>, <code>axis(…)</code>, or <code>hat(…)</code>.
          </p>
          <table className="w-full text-xs mt-1">
            <thead>
              <tr style={{ color: 'var(--color-oasis-text-muted)' }}>
                <th className="text-left pb-1 font-semibold">Action</th>
                <th className="text-left pb-1 font-semibold">Binding</th>
              </tr>
            </thead>
            <tbody>
              {bindings.map((b, i) => (
                <BindingRow
                  key={b.action}
                  binding={b}
                  index={i}
                  editing={editingIndex === i}
                  onStartEdit={(idx) => setEditingIndex(idx)}
                  onCommit={handleCommit}
                  onCancel={() => setEditingIndex(null)}
                />
              ))}
            </tbody>
          </table>
          <div className="flex gap-2 mt-3">
            <button
              type="button"
              onClick={handleSave}
              className="px-3 py-1 rounded-lg text-xs font-semibold transition-opacity hover:opacity-80"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
            >
              {saved ? '✓ Saved' : 'Save Bindings'}
            </button>
            {isCustomized && (
              <button
                type="button"
                onClick={handleReset}
                className="px-3 py-1 rounded-lg text-xs font-semibold transition-opacity hover:opacity-80"
                style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.1)' }}
              >
                Reset to Default
              </button>
            )}
          </div>
        </div>
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// DS Touch Input Calibration panel
// ---------------------------------------------------------------------------

// DS screen dimensions (256×192 per screen at native resolution)
const DS_SCREEN_W = 256;
const DS_SCREEN_H = 192;

function DsTouchCalibrationPanel() {
  const [cal, setCal] = useState<DsTouchCalibration>(loadTouchCalibration);
  const [saved, setSaved] = useState(false);
  // Visual test state: raw click position within the test area and its calibrated output
  const [testRaw, setTestRaw] = useState<{ x: number; y: number } | null>(null);
  const [testMapped, setTestMapped] = useState<{ x: number; y: number } | null>(null);
  const testAreaRef = useRef<HTMLDivElement>(null);

  function handleChange(field: keyof DsTouchCalibration, raw: string) {
    const value = parseFloat(raw);
    if (!Number.isFinite(value)) return;
    setCal((prev) => ({ ...prev, [field]: value }));
  }

  function handleSave() {
    saveTouchCalibration(cal);
    setSaved(true);
    setTimeout(() => setSaved(false), 1500);
  }

  function handleReset() {
    resetTouchCalibration();
    setCal({ ...DEFAULT_CALIBRATION });
    setTestRaw(null);
    setTestMapped(null);
  }

  /** Handle a click on the visual test area — maps click coords through current calibration. */
  function handleTestClick(e: React.MouseEvent<HTMLDivElement>) {
    if (!testAreaRef.current) return;
    const rect = testAreaRef.current.getBoundingClientRect();
    // Map click position to 0–DS_SCREEN_W / 0–DS_SCREEN_H coordinate space
    const rawX = ((e.clientX - rect.left) / rect.width) * DS_SCREEN_W;
    const rawY = ((e.clientY - rect.top) / rect.height) * DS_SCREEN_H;
    const [mappedX, mappedY] = applyCalibration(rawX, rawY, cal);
    setTestRaw({ x: rawX, y: rawY });
    setTestMapped({ x: mappedX, y: mappedY });
  }

  const isDefault =
    cal.offsetX === DEFAULT_CALIBRATION.offsetX &&
    cal.offsetY === DEFAULT_CALIBRATION.offsetY &&
    cal.scaleX === DEFAULT_CALIBRATION.scaleX &&
    cal.scaleY === DEFAULT_CALIBRATION.scaleY;

  // Convert DS coordinates (0–256, 0–192) back to percentage for dot placement
  const rawDotStyle = testRaw
    ? {
        left: `${(testRaw.x / DS_SCREEN_W) * 100}%`,
        top: `${(testRaw.y / DS_SCREEN_H) * 100}%`,
      }
    : null;
  const mappedDotStyle = testMapped
    ? {
        left: `${Math.min(Math.max((testMapped.x / DS_SCREEN_W) * 100, 0), 100)}%`,
        top: `${Math.min(Math.max((testMapped.y / DS_SCREEN_H) * 100, 0), 100)}%`,
      }
    : null;

  return (
    <section className="mt-8 rounded-2xl p-5" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
      <h2 className="text-base font-bold mb-1" style={{ color: 'var(--color-oasis-text)' }}>
        📱 DS Touch Input Calibration
      </h2>
      <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
        Fine-tune how mouse clicks map to the DS bottom screen. Adjust offset (pixel shift)
        and scale (stretch/shrink) if touch input feels misaligned in melonDS.
      </p>

      <div className="grid grid-cols-2 gap-3 mb-4">
        {(
          [
            { field: 'offsetX', label: 'Offset X (px)', step: '1', hint: 'Horizontal pixel shift' },
            { field: 'offsetY', label: 'Offset Y (px)', step: '1', hint: 'Vertical pixel shift' },
            { field: 'scaleX',  label: 'Scale X',       step: '0.01', hint: 'Horizontal stretch (1.0 = none)' },
            { field: 'scaleY',  label: 'Scale Y',       step: '0.01', hint: 'Vertical stretch (1.0 = none)' },
          ] as const
        ).map(({ field, label, step, hint }) => (
          <label key={field} className="block">
            <span className="text-xs font-semibold mb-1 block" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {label}
              <span className="ml-1 font-normal opacity-60">— {hint}</span>
            </span>
            <input
              type="number"
              step={step}
              value={cal[field]}
              onChange={(e) => handleChange(field, e.target.value)}
              className="w-full px-3 py-2 rounded-lg text-sm font-mono outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
                border: '1px solid rgba(255,255,255,0.1)',
              }}
            />
          </label>
        ))}
      </div>

      {/* Visual calibration test area */}
      <div className="mb-4">
        <p className="text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          🖱️ Click Test Area
          <span className="ml-1 font-normal opacity-60">— click anywhere below to preview the calibrated mapping</span>
        </p>
        <div
          ref={testAreaRef}
          onClick={handleTestClick}
          className="relative rounded-xl overflow-hidden cursor-crosshair select-none"
          style={{
            backgroundColor: 'var(--color-oasis-surface)',
            border: '1px solid rgba(255,255,255,0.1)',
            aspectRatio: `${DS_SCREEN_W} / ${DS_SCREEN_H}`,
            maxHeight: '120px',
          }}
        >
          {/* Grid lines for visual reference */}
          <div className="absolute inset-0 opacity-10" style={{
            backgroundImage: 'linear-gradient(rgba(255,255,255,0.3) 1px, transparent 1px), linear-gradient(90deg, rgba(255,255,255,0.3) 1px, transparent 1px)',
            backgroundSize: '25% 25%',
          }} />
          <span
            className="absolute inset-0 flex items-center justify-center text-[10px]"
            style={{ color: 'var(--color-oasis-text-muted)', pointerEvents: 'none' }}
          >
            {testRaw ? '' : 'Click to test calibration'}
          </span>
          {/* Raw click position (grey dot) */}
          {rawDotStyle && (
            <div
              className="absolute w-2.5 h-2.5 rounded-full -translate-x-1/2 -translate-y-1/2 border border-white/30"
              style={{ backgroundColor: 'rgba(148,163,184,0.8)', left: rawDotStyle.left, top: rawDotStyle.top, zIndex: 1 }}
              title="Raw click position"
            />
          )}
          {/* Calibrated (mapped) position (orange dot) */}
          {mappedDotStyle && (
            <div
              className="absolute w-2.5 h-2.5 rounded-full -translate-x-1/2 -translate-y-1/2 border border-white/30"
              style={{ backgroundColor: '#E87722', left: mappedDotStyle.left, top: mappedDotStyle.top, zIndex: 2 }}
              title="Calibrated position"
            />
          )}
        </div>
        {testRaw && testMapped && (
          <p className="text-[10px] mt-1 font-mono" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Raw: ({testRaw.x.toFixed(1)}, {testRaw.y.toFixed(1)}) →{' '}
            <span style={{ color: '#E87722' }}>
              Mapped: ({testMapped.x.toFixed(1)}, {testMapped.y.toFixed(1)})
            </span>
            {' '}(DS coords 0–{DS_SCREEN_W} × 0–{DS_SCREEN_H})
          </p>
        )}
      </div>

      <div className="flex gap-2 items-center">
        <button
          type="button"
          onClick={handleSave}
          className="px-4 py-2 rounded-xl text-sm font-bold transition-opacity hover:opacity-80"
          style={{ backgroundColor: '#E87722', color: 'white' }}
        >
          {saved ? '✓ Saved' : 'Save Calibration'}
        </button>
        {!isDefault && (
          <button
            type="button"
            onClick={handleReset}
            className="px-4 py-2 rounded-xl text-sm font-semibold transition-opacity hover:opacity-80"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.1)' }}
          >
            Reset to Defaults
          </button>
        )}
      </div>

      <p className="text-xs mt-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
        💡 melonDS tip: if the bottom screen region is offset in your window layout, try{' '}
        <code className="font-mono">offsetY = −192</code> (one DS screen height) to shift
        touch coordinates to the correct half.
      </p>
    </section>
  );
}

export function SettingsPage() {
  const [romDir, setRomDir] = useState(getRomDirectory);
  const [saveDir, setSaveDirState] = useState(
    () => localStorage.getItem(SAVE_DIR_KEY) ?? ''
  );
  const [displayName, setDisplayNameState] = useState(
    () => localStorage.getItem(DISPLAY_NAME_KEY) ?? ''
  );
  const [saved, setSaved] = useState(false);
  const [profileSystem, setProfileSystem] = useState<string>(SYSTEMS_WITH_PROFILES[0] ?? 'N64');
  // Bump this to force re-render after a profile is saved/reset
  const [profileVersion, setProfileVersion] = useState(0);
  const handleProfileChange = useCallback(() => setProfileVersion((v) => v + 1), []);

  // Emulator path drafts — local state for the editable inputs, flushed on blur/save
  const [emulatorPathDrafts, setEmulatorPathDrafts] = useState<Record<string, string>>(
    () => {
      const result: Record<string, string> = {};
      for (const id of Object.keys(EMULATOR_NAMES)) {
        result[id] = getEmulatorPath(id) ?? '';
      }
      return result;
    },
  );
  const [emulatorPathsSaved, setEmulatorPathsSaved] = useState(false);

  function handleEmulatorPathChange(backendId: string, value: string) {
    setEmulatorPathDrafts((prev) => ({ ...prev, [backendId]: value }));
  }

  function persistEmulatorPath(backendId: string, value: string) {
    if (value.trim()) {
      setEmulatorPath(backendId, value);
    } else {
      clearEmulatorPath(backendId);
    }
  }

  function handleEmulatorPathBlur(backendId: string) {
    persistEmulatorPath(backendId, emulatorPathDrafts[backendId] ?? '');
  }

  function handleSaveEmulatorPaths() {
    for (const [id, val] of Object.entries(emulatorPathDrafts)) {
      persistEmulatorPath(id, val);
    }
    setEmulatorPathsSaved(true);
    setTimeout(() => setEmulatorPathsSaved(false), 3000);
  }

  function handleSave(e: React.FormEvent) {
    e.preventDefault();
    setRomDirectory(romDir);
    setSaveDirectory(saveDir);
    if (displayName.trim()) {
      localStorage.setItem(DISPLAY_NAME_KEY, displayName.trim());
    }
    setSaved(true);
    setTimeout(() => setSaved(false), 2000);
  }

  return (
    <div className="max-w-2xl">
      <h1 className="text-2xl font-bold mb-6" style={{ color: 'var(--color-oasis-accent-light)' }}>
        ⚙️ Settings
      </h1>

      <form onSubmit={handleSave} className="space-y-6">
        {/* Profile */}
        <section
          className="rounded-2xl p-5"
          style={{ backgroundColor: 'var(--color-oasis-card)' }}
        >
          <h2 className="text-base font-bold mb-4" style={{ color: 'var(--color-oasis-text)' }}>
            👤 Profile
          </h2>
          <label className="block">
            <span className="text-xs font-semibold mb-1 block" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Display Name
            </span>
            <input
              type="text"
              value={displayName}
              onChange={(e) => setDisplayNameState(e.target.value)}
              placeholder="Enter your display name"
              className="w-full px-3 py-2 rounded-lg text-sm"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
                border: '1px solid rgba(255,255,255,0.1)',
                outline: 'none',
              }}
            />
          </label>
        </section>

        {/* ROM Paths */}
        <section
          className="rounded-2xl p-5"
          style={{ backgroundColor: 'var(--color-oasis-card)' }}
        >
          <h2 className="text-base font-bold mb-1" style={{ color: 'var(--color-oasis-text)' }}>
            📁 ROM &amp; Save Paths
          </h2>
          <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
            These paths are used when auto-launching your emulator after a session starts.
            Enter the absolute path to the folder that contains your ROM files.
          </p>

          <div className="space-y-4">
            <label className="block">
              <span className="text-xs font-semibold mb-1 block" style={{ color: 'var(--color-oasis-text-muted)' }}>
                ROM Directory
              </span>
              <div className="flex gap-2">
                <input
                  type="text"
                  value={romDir}
                  onChange={(e) => setRomDir(e.target.value)}
                  placeholder="/home/user/roms  or  C:\ROMs"
                  className="flex-1 px-3 py-2 rounded-lg text-sm font-mono"
                  style={{
                    backgroundColor: 'var(--color-oasis-surface)',
                    color: 'var(--color-oasis-text)',
                    border: '1px solid rgba(255,255,255,0.1)',
                    outline: 'none',
                  }}
                />
                {isTauri() && (
                  <button
                    type="button"
                    onClick={async () => {
                      const result = await tauriPickDirectory();
                      if (result.path) setRomDir(result.path);
                    }}
                    className="px-3 py-2 rounded-lg text-xs font-semibold flex-shrink-0 transition-opacity hover:opacity-80"
                    style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.1)' }}
                    title="Open native directory picker"
                  >
                    📂 Browse
                  </button>
                )}
              </div>
            </label>

            <label className="block">
              <span className="text-xs font-semibold mb-1 block" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Save Directory{' '}
                <span style={{ color: 'var(--color-oasis-text-muted)', fontWeight: 400 }}>
                  (defaults to ROM Directory if empty)
                </span>
              </span>
              <div className="flex gap-2">
                <input
                  type="text"
                  value={saveDir}
                  onChange={(e) => setSaveDirState(e.target.value)}
                  placeholder="/home/user/saves  or  C:\Saves"
                  className="flex-1 px-3 py-2 rounded-lg text-sm font-mono"
                  style={{
                    backgroundColor: 'var(--color-oasis-surface)',
                    color: 'var(--color-oasis-text)',
                    border: '1px solid rgba(255,255,255,0.1)',
                    outline: 'none',
                  }}
                />
                {isTauri() && (
                  <button
                    type="button"
                    onClick={async () => {
                      const result = await tauriPickDirectory();
                      if (result.path) setSaveDirState(result.path);
                    }}
                    className="px-3 py-2 rounded-lg text-xs font-semibold flex-shrink-0 transition-opacity hover:opacity-80"
                    style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.1)' }}
                    title="Open native directory picker"
                  >
                    📂 Browse
                  </button>
                )}
              </div>
            </label>
          </div>
        </section>

        {/* Save button */}
        <div className="flex items-center gap-3">
          <button
            type="submit"
            className="px-6 py-2.5 rounded-xl text-sm font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
            style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
          >
            {saved ? '✓ Saved!' : 'Save Settings'}
          </button>
          {saved && (
            <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Settings saved to local storage.
            </span>
          )}
        </div>
      </form>

      {/* Controller Profiles — editable */}
      <section className="mt-8 rounded-2xl p-5" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
        <h2 className="text-base font-bold mb-1" style={{ color: 'var(--color-oasis-text)' }}>
          🕹️ Controller Profiles
        </h2>
        <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Input mappings used when launching a session. Expand a profile and click any binding to remap it.
          Custom bindings are saved to local storage; use "Reset to Default" to restore the original.
        </p>

        {/* System tabs */}
        <div className="flex gap-2 mb-4">
          {SYSTEMS_WITH_PROFILES.map((sys) => (
            <button
              key={sys}
              type="button"
              onClick={() => setProfileSystem(sys)}
              className="px-3 py-1 rounded-full text-xs font-semibold transition-colors"
              style={{
                backgroundColor: profileSystem === sys ? 'var(--color-oasis-accent)' : 'var(--color-oasis-surface)',
                color: profileSystem === sys ? 'white' : 'var(--color-oasis-text-muted)',
              }}
            >
              {sys}
            </button>
          ))}
        </div>

        <div key={profileVersion} className="space-y-2">
          {mergeWithCustomProfiles(profileManager.listForSystem(profileSystem.toLowerCase())).map((profile) => (
            <ControllerProfileCard
              key={profile.id}
              defaultProfile={DEFAULT_PROFILES.find((p) => p.id === profile.id) ?? profile}
              onProfileChange={handleProfileChange}
            />
          ))}
        </div>
      </section>

      {/* DS Touch Input Calibration */}
      <DsTouchCalibrationPanel />

      {/* Emulator Paths */}
      <section className="mt-8 rounded-2xl p-5" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
        <h2 className="text-base font-bold mb-1" style={{ color: 'var(--color-oasis-text)' }}>
          🎮 Emulator Paths
        </h2>
        <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Set the absolute path to each emulator executable. RetroOasis uses these paths when you click
          "▶ Play Locally" on a game detail page. Leave a field blank to disable local launch for that system.
        </p>

        <div className="space-y-3">
          {Object.entries(EMULATOR_NAMES).map(([backendId, displayName]) => (
            <label key={backendId} className="block">
              <span className="text-xs font-semibold mb-1 block" style={{ color: 'var(--color-oasis-text-muted)' }}>
                {displayName}
              </span>
              <input
                type="text"
                value={emulatorPathDrafts[backendId] ?? ''}
                onChange={(e) => handleEmulatorPathChange(backendId, e.target.value)}
                onBlur={() => handleEmulatorPathBlur(backendId)}
                placeholder={
                  (EMULATOR_DEFAULT_PATHS[backendId] ?? [])[0] ??
                  `/usr/bin/${backendId}`
                }
                className="w-full px-3 py-2 rounded-lg text-sm font-mono"
                style={{
                  backgroundColor: 'var(--color-oasis-surface)',
                  color: 'var(--color-oasis-text)',
                  border: '1px solid rgba(255,255,255,0.1)',
                  outline: 'none',
                }}
              />
              {(EMULATOR_DEFAULT_PATHS[backendId] ?? []).length > 0 && (
                <p className="text-[10px] mt-0.5" style={{ color: 'var(--color-oasis-text-muted)', opacity: 0.6 }}>
                  Common paths: {(EMULATOR_DEFAULT_PATHS[backendId] ?? []).join(' · ')}
                </p>
              )}
            </label>
          ))}
        </div>

        <div className="flex items-center gap-3 mt-4">
          <button
            type="button"
            onClick={handleSaveEmulatorPaths}
            className="px-6 py-2.5 rounded-xl text-sm font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
            style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
          >
            {emulatorPathsSaved ? '✓ Saved!' : 'Save Emulator Paths'}
          </button>
          {emulatorPathsSaved && (
            <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Paths saved to local storage.
            </span>
          )}
        </div>
      </section>

      {/* Crash Reporting */}
      <CrashReportingSection />

      {/* RetroAchievements */}
      <RetroAchievementsSection />
    </div>
  );
}

// ---------------------------------------------------------------------------
// Crash Reporting Section — Phase 29
// ---------------------------------------------------------------------------

function CrashReportingSection() {
  const [enabled, setEnabled] = useState(isCrashReportingEnabled);
  const [reportCount, setReportCount] = useState(() => getCrashReports().length);
  const [cleared, setCleared] = useState(false);

  function handleToggle(on: boolean) {
    if (on) {
      enableCrashReporting();
    } else {
      disableCrashReporting();
    }
    setEnabled(on);
    setReportCount(getCrashReports().length);
  }

  function handleClear() {
    clearCrashReports();
    setReportCount(0);
    setCleared(true);
    setTimeout(() => setCleared(false), 2000);
  }

  function handleExport() {
    const data = exportCrashReports();
    const blob = new Blob([data], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `retro-oasis-crash-reports-${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
  }

  return (
    <section className="mt-8 rounded-2xl p-5" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
      <h2 className="text-base font-bold mb-1" style={{ color: 'var(--color-oasis-text)' }}>
        🐛 Crash Reporting
      </h2>
      <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
        When enabled, crash details are stored locally so you can include them when filing bug reports.
        Nothing is sent to any remote server.
      </p>

      <label className="flex items-center gap-3 cursor-pointer select-none mb-4">
        <input
          type="checkbox"
          checked={enabled}
          onChange={(e) => handleToggle(e.target.checked)}
          className="w-4 h-4"
        />
        <span className="text-sm font-medium" style={{ color: 'var(--color-oasis-text)' }}>
          Enable local crash reporting
        </span>
      </label>

      {enabled && (
        <div className="flex items-center gap-3 flex-wrap">
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {reportCount} report{reportCount !== 1 ? 's' : ''} stored
          </span>
          {reportCount > 0 && (
            <>
              <button
                type="button"
                onClick={handleExport}
                className="px-3 py-1.5 rounded-lg text-xs font-semibold"
                style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
              >
                Export Reports
              </button>
              <button
                type="button"
                onClick={handleClear}
                className="px-3 py-1.5 rounded-lg text-xs font-semibold"
                style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text)' }}
              >
                {cleared ? '✓ Cleared' : 'Clear Reports'}
              </button>
            </>
          )}
        </div>
      )}
    </section>
  );
}

// ---------------------------------------------------------------------------
// RetroAchievements Section — Phase 34
// ---------------------------------------------------------------------------

function RetroAchievementsSection() {
  const initial = getRACredentials();
  const [username, setUsername] = useState(initial.username);
  const [token, setToken] = useState(initial.token);
  const [hardcoreMode, setHardcoreMode] = useState(initial.hardcoreMode);
  const [connected, setConnected] = useState(isRAConnected());
  const [saved, setSaved] = useState(false);

  function handleSave() {
    setRACredentials({ username, token, hardcoreMode });
    setConnected(isRAConnected());
    setSaved(true);
    setTimeout(() => setSaved(false), 2000);
  }

  function handleDisconnect() {
    clearRACredentials();
    setUsername('');
    setToken('');
    setHardcoreMode(false);
    setConnected(false);
  }

  return (
    <section className="mt-8 rounded-2xl p-5" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
      <h2 className="text-base font-bold mb-1" style={{ color: 'var(--color-oasis-text)' }}>
        🏅 RetroAchievements Account
      </h2>
      <p className="text-xs mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
        Connect your RetroAchievements.org account to track achievements across supported systems.
        Visit{' '}
        <a
          href="https://retroachievements.org"
          target="_blank"
          rel="noopener noreferrer"
          className="underline"
          style={{ color: 'var(--color-oasis-accent-light)' }}
        >
          retroachievements.org
        </a>{' '}
        to get your API key.
      </p>

      {/* Status badge */}
      <div className="mb-4 mt-2">
        <span
          className="inline-block text-[11px] font-bold px-2.5 py-1 rounded-full"
          style={
            connected
              ? { backgroundColor: 'rgba(22,163,74,0.15)', color: '#4ade80' }
              : { backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }
          }
        >
          {connected ? '● Connected' : '○ Not configured'}
        </span>
      </div>

      <div className="space-y-3">
        <label className="block">
          <span className="text-xs font-semibold mb-1 block" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Username
          </span>
          <input
            type="text"
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            placeholder="Your RetroAchievements username"
            className="w-full px-3 py-2 rounded-lg text-sm"
            style={{
              backgroundColor: 'var(--color-oasis-surface)',
              color: 'var(--color-oasis-text)',
              border: '1px solid rgba(255,255,255,0.1)',
              outline: 'none',
            }}
          />
        </label>

        <label className="block">
          <span className="text-xs font-semibold mb-1 block" style={{ color: 'var(--color-oasis-text-muted)' }}>
            API Token
          </span>
          <input
            type="password"
            value={token}
            onChange={(e) => setToken(e.target.value)}
            placeholder="Your API key or web API token"
            className="w-full px-3 py-2 rounded-lg text-sm font-mono"
            style={{
              backgroundColor: 'var(--color-oasis-surface)',
              color: 'var(--color-oasis-text)',
              border: '1px solid rgba(255,255,255,0.1)',
              outline: 'none',
            }}
          />
        </label>

        <label className="flex items-center gap-3 cursor-pointer select-none">
          <input
            type="checkbox"
            checked={hardcoreMode}
            onChange={(e) => setHardcoreMode(e.target.checked)}
            className="w-4 h-4"
          />
          <span className="text-sm font-medium" style={{ color: 'var(--color-oasis-text)' }}>
            Hardcore Mode
          </span>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            (no save states or rewind — achievements worth double points)
          </span>
        </label>
      </div>

      <div className="flex items-center gap-3 mt-4 flex-wrap">
        <button
          type="button"
          onClick={handleSave}
          className="px-5 py-2 rounded-xl text-sm font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
        >
          {saved ? '✓ Saved!' : 'Save'}
        </button>
        {connected && (
          <button
            type="button"
            onClick={handleDisconnect}
            className="px-5 py-2 rounded-xl text-sm font-bold transition-opacity hover:opacity-80"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: '#f87171', border: '1px solid rgba(230,0,18,0.3)' }}
          >
            Disconnect
          </button>
        )}
        {saved && (
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Credentials saved to local storage.
          </span>
        )}
      </div>
    </section>
  );
}
