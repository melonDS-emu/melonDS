import { useState } from 'react';
import {
  getRomDirectory,
  setRomDirectory,
  setSaveDirectory,
} from '../lib/rom-settings';
import { InputProfileManager } from '@retro-oasis/multiplayer-profiles';
import type { InputProfile, InputBinding } from '@retro-oasis/multiplayer-profiles';

const SAVE_DIR_KEY = 'retro-oasis-save-directory';
const DISPLAY_NAME_KEY = 'retro-oasis-display-name';

const profileManager = new InputProfileManager();
const ALL_PROFILES = profileManager.listAll();

const SYSTEMS_WITH_PROFILES: string[] = [...new Set(ALL_PROFILES.map((p) => p.system.toUpperCase()))];

function bindingLabel(b: InputBinding): string {
  if (b.key)    return b.key.replace(/^key\(/, '').replace(/\)$/, '').toUpperCase();
  if (b.button) return b.button.replace(/^button\(/, 'Btn ').replace(/^hat\(0 /, '').replace(/\)$/, '');
  if (b.axis)   return b.axis.replace(/^axis\(/, 'Axis ').replace(/\)$/, '');
  return '—';
}

function ControllerProfileCard({ profile }: { profile: InputProfile }) {
  const [expanded, setExpanded] = useState(false);
  const icon = profile.controllerType === 'xbox' ? '🎮' : profile.controllerType === 'playstation' ? '🕹️' : '⌨️';
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
            {profile.name.replace(/ \(default\)$/, '')}
          </span>
        </div>
        <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {expanded ? '▲' : '▼'}
        </span>
      </button>
      {expanded && (
        <div className="px-4 pb-3 border-t border-white/10">
          <table className="w-full text-xs mt-2">
            <thead>
              <tr style={{ color: 'var(--color-oasis-text-muted)' }}>
                <th className="text-left pb-1 font-semibold">Action</th>
                <th className="text-left pb-1 font-semibold">Binding</th>
              </tr>
            </thead>
            <tbody>
              {profile.bindings.map((b, i) => (
                <tr key={i} style={{ borderTop: i > 0 ? '1px solid rgba(255,255,255,0.05)' : undefined }}>
                  <td className="py-1 pr-3 font-medium" style={{ color: 'var(--color-oasis-text)' }}>{b.action}</td>
                  <td className="py-1 font-mono" style={{ color: 'var(--color-oasis-accent-light)' }}>{bindingLabel(b)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
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
              <input
                type="text"
                value={romDir}
                onChange={(e) => setRomDir(e.target.value)}
                placeholder="/home/user/roms  or  C:\ROMs"
                className="w-full px-3 py-2 rounded-lg text-sm font-mono"
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
                Save Directory{' '}
                <span style={{ color: 'var(--color-oasis-text-muted)', fontWeight: 400 }}>
                  (defaults to ROM Directory if empty)
                </span>
              </span>
              <input
                type="text"
                value={saveDir}
                onChange={(e) => setSaveDirState(e.target.value)}
                placeholder="/home/user/saves  or  C:\Saves"
                className="w-full px-3 py-2 rounded-lg text-sm font-mono"
                style={{
                  backgroundColor: 'var(--color-oasis-surface)',
                  color: 'var(--color-oasis-text)',
                  border: '1px solid rgba(255,255,255,0.1)',
                  outline: 'none',
                }}
              />
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

      {/* Controller Profiles — read-only reference (not part of the save form) */}
      <section className="mt-8 rounded-2xl p-5" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
        <h2 className="text-base font-bold mb-1" style={{ color: 'var(--color-oasis-text)' }}>
          🕹️ Controller Profiles
        </h2>
        <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Default input mappings for each system. These are used by the emulator when launching a session.
          Expand a profile to see its button bindings.
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

        <div className="space-y-2">
          {profileManager
            .listForSystem(profileSystem.toLowerCase())
            .map((profile) => (
              <ControllerProfileCard key={profile.id} profile={profile} />
            ))}
        </div>
      </section>
    </div>
  );
}
