import { useState } from 'react';
import {
  getRomDirectory,
  setRomDirectory,
  setSaveDirectory,
} from '../lib/rom-settings';

const SAVE_DIR_KEY = 'retro-oasis-save-directory';
const DISPLAY_NAME_KEY = 'retro-oasis-display-name';

export function SettingsPage() {
  const [romDir, setRomDir] = useState(getRomDirectory);
  const [saveDir, setSaveDirState] = useState(
    () => localStorage.getItem(SAVE_DIR_KEY) ?? ''
  );
  const [displayName, setDisplayNameState] = useState(
    () => localStorage.getItem(DISPLAY_NAME_KEY) ?? ''
  );
  const [saved, setSaved] = useState(false);

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
    </div>
  );
}
