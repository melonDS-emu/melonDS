/**
 * Setup Page — Phase 29 (Desktop Productization)
 *
 * Multi-step first-run wizard that guides a new user through:
 *   Step 1: Choose a display name
 *   Step 2: Configure emulator executable paths
 *   Step 3: Set the ROM directory
 *   Step 4: Done!
 *
 * Accessible at /setup. After completion, redirects to /.
 */

import { useMemo, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import {
  markFirstRunComplete,
  completeStep,
  type SetupStep,
} from '../lib/first-run-service';
import {
  EMULATOR_NAMES,
  SYSTEM_BACKEND_MAP,
  EMULATOR_DEFAULT_PATHS,
  setEmulatorPath,
  getEmulatorPath,
} from '../lib/emulator-settings';
import {
  isCrashReportingEnabled,
  enableCrashReporting,
  disableCrashReporting,
} from '../lib/crash-reporting';
import { isWindowsPlatform } from '../lib/platform';
import { tauriPickDirectory, isTauri } from '../lib/tauri-ipc';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const STEP_LABELS: SetupStep[] = ['display-name', 'emulator-paths', 'rom-directory', 'done'];
const STEP_TITLES: Record<SetupStep, string> = {
  'display-name': 'Welcome to RetroOasis',
  'emulator-paths': 'Configure Emulators',
  'rom-directory': 'Set ROM Directory',
  'done': 'You\'re All Set!',
};
const STEP_SUBTITLES: Record<SetupStep, string> = {
  'display-name': 'Choose how other players see you in lobbies.',
  'emulator-paths': 'Point RetroOasis to your installed emulators.',
  'rom-directory': 'Tell RetroOasis where your ROM files live.',
  'done': 'RetroOasis is ready. Happy gaming!',
};

function ProgressDots({ current }: { current: number }) {
  return (
    <div className="flex items-center justify-center gap-2 mb-8">
      {STEP_LABELS.map((_, i) => (
        <span
          key={i}
          className="rounded-full transition-all"
          style={{
            width: i === current ? 24 : 8,
            height: 8,
            background: i <= current ? 'var(--color-oasis-accent)' : 'var(--color-oasis-border)',
          }}
        />
      ))}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Step components
// ---------------------------------------------------------------------------

function Step1DisplayName({
  onNext,
}: {
  onNext: (name: string) => void;
}) {
  const [name, setName] = useState('');
  const trimmed = name.trim();
  const valid = trimmed.length >= 2 && trimmed.length <= 24 && /^[a-zA-Z0-9 _\-]+$/.test(trimmed);

  return (
    <div className="flex flex-col gap-4">
      <div>
        <label
          className="block text-sm font-medium mb-1"
          style={{ color: 'var(--color-oasis-text)' }}
          htmlFor="display-name"
        >
          Display Name
        </label>
        <input
          id="display-name"
          type="text"
          placeholder="e.g. RetroFan64"
          value={name}
          onChange={(e) => setName(e.target.value)}
          maxLength={24}
          className="w-full px-4 py-2 rounded-lg text-sm"
          style={{
            background: 'var(--color-oasis-card)',
            border: `1px solid ${valid || !trimmed ? 'var(--color-oasis-border)' : '#ef4444'}`,
            color: 'var(--color-oasis-text)',
            outline: 'none',
          }}
        />
        {trimmed && !valid && (
          <p className="text-xs mt-1" style={{ color: '#ef4444' }}>
            2–24 characters; letters, numbers, spaces, _ and - only.
          </p>
        )}
        <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          You can change this later in Settings.
        </p>
      </div>

      <button
        onClick={() => onNext(trimmed)}
        disabled={!valid}
        className="w-full py-2 rounded-lg font-semibold transition-opacity"
        style={{
          background: 'var(--color-oasis-accent)',
          color: '#fff',
          opacity: valid ? 1 : 0.4,
        }}
      >
        Continue →
      </button>
      <button
        onClick={() => onNext('')}
        className="text-sm text-center"
        style={{ color: 'var(--color-oasis-text-muted)' }}
      >
        Skip for now
      </button>
    </div>
  );
}

function Step2EmulatorPaths({ onNext }: { onNext: () => void }) {
  const isWindows = useMemo(() => isWindowsPlatform(), []);
  // Get unique backends from the system map
  const backends = [...new Set(Object.values(SYSTEM_BACKEND_MAP))].sort();
  const [paths, setPaths] = useState<Record<string, string>>(() => {
    const init: Record<string, string> = {};
    for (const b of backends) {
      init[b] = getEmulatorPath(b) ?? '';
    }
    return init;
  });

  function handleSave() {
    for (const [backend, path] of Object.entries(paths)) {
      if (path.trim()) setEmulatorPath(backend, path.trim());
    }
    onNext();
  }

  return (
    <div className="flex flex-col gap-4">
      <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
        Enter the path to each emulator's executable. You can skip any you haven't installed.
      </p>
      <div className="flex flex-col gap-3 max-h-64 overflow-y-auto pr-1">
        {backends.map((backend) => {
          const suggestedPaths = EMULATOR_DEFAULT_PATHS[backend] ?? [];
          const preferredPlaceholder = isWindows
            ? suggestedPaths.find((path) => path.includes('\\')) ?? suggestedPaths[0] ?? 'path/to/executable'
            : suggestedPaths.find((path) => !path.includes('\\')) ?? suggestedPaths[0] ?? 'path/to/executable';

          return (
            <div key={backend}>
              <label
              className="block text-xs font-semibold mb-1 uppercase tracking-wide"
              style={{ color: 'var(--color-oasis-text-muted)' }}
            >
              {EMULATOR_NAMES[backend] ?? backend}
            </label>
            <input
              type="text"
              placeholder={preferredPlaceholder}
              value={paths[backend] ?? ''}
              onChange={(e) => setPaths((p) => ({ ...p, [backend]: e.target.value }))}
              className="w-full px-3 py-1.5 rounded-lg text-sm"
              style={{
                background: 'var(--color-oasis-card)',
                border: '1px solid var(--color-oasis-border)',
                color: 'var(--color-oasis-text)',
                outline: 'none',
                fontFamily: 'monospace',
              }}
            />
            {suggestedPaths.length > 0 && (
              <p className="text-[10px] mt-1 leading-relaxed" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Suggested locations: {suggestedPaths.join(' · ')}
              </p>
            )}
            </div>
          );
        })}
      </div>
      <button
        onClick={handleSave}
        className="w-full py-2 rounded-lg font-semibold"
        style={{ background: 'var(--color-oasis-accent)', color: '#fff' }}
      >
        Save & Continue →
      </button>
      <button
        onClick={onNext}
        className="text-sm text-center"
        style={{ color: 'var(--color-oasis-text-muted)' }}
      >
        Skip for now
      </button>
    </div>
  );
}

function Step3RomDirectory({ onNext }: { onNext: (dir: string) => void }) {
  const isWindows = useMemo(() => isWindowsPlatform(), []);
  const [dir, setDir] = useState('');

  const placeholder = isWindows ? 'C:\\RetroOasis\\ROMs' : '/home/user/ROMs';
  const helperText = isWindows
    ? 'Tip: folders under Documents or a dedicated ROMs library tend to avoid Windows permission prompts.'
    : 'Tip: point this at the top-level folder that contains your ROM collection.';

  return (
    <div className="flex flex-col gap-4">
      <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
        RetroOasis will scan this directory for ROM files and populate your library.
      </p>
      <div>
        <label
          className="block text-sm font-medium mb-1"
          style={{ color: 'var(--color-oasis-text)' }}
        >
          ROM Directory Path
        </label>
        <div className="flex gap-2">
          <input
            type="text"
            placeholder={placeholder}
            value={dir}
            onChange={(e) => setDir(e.target.value)}
            className="w-full px-4 py-2 rounded-lg text-sm"
            style={{
              background: 'var(--color-oasis-card)',
              border: '1px solid var(--color-oasis-border)',
              color: 'var(--color-oasis-text)',
              outline: 'none',
              fontFamily: 'monospace',
            }}
          />
          {isTauri() && (
            <button
              type="button"
              onClick={async () => {
                const result = await tauriPickDirectory();
                if (result.path) setDir(result.path);
              }}
              className="px-3 py-2 rounded-lg text-xs font-semibold flex-shrink-0 transition-opacity hover:opacity-80"
              style={{
                background: 'var(--color-oasis-card)',
                color: 'var(--color-oasis-text-muted)',
                border: '1px solid var(--color-oasis-border)',
              }}
            >
              📂 Browse
            </button>
          )}
        </div>
        <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Subdirectories are scanned recursively. Supported file types include .nes, .sfc, .gba, .n64, .nds, .iso, and more.
        </p>
        <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {helperText}
        </p>
      </div>

      {/* Crash reporting opt-in */}
      <div
        className="flex items-start gap-3 p-3 rounded-lg"
        style={{ background: 'var(--color-oasis-card)', border: '1px solid var(--color-oasis-border)' }}
      >
        <input
          id="crash-opt-in"
          type="checkbox"
          defaultChecked={isCrashReportingEnabled()}
          onChange={(e) => e.target.checked ? enableCrashReporting() : disableCrashReporting()}
          className="mt-0.5"
        />
        <label htmlFor="crash-opt-in" className="text-sm cursor-pointer" style={{ color: 'var(--color-oasis-text)' }}>
          <span className="font-medium">Enable crash reporting (optional)</span>
          <br />
          <span style={{ color: 'var(--color-oasis-text-muted)' }}>
            Stores crash details locally so you can share them when reporting bugs.
            Nothing is sent to any server.
          </span>
        </label>
      </div>

      <button
        onClick={() => onNext(dir.trim())}
        className="w-full py-2 rounded-lg font-semibold"
        style={{ background: 'var(--color-oasis-accent)', color: '#fff' }}
      >
        Continue →
      </button>
      <button
        onClick={() => onNext('')}
        className="text-sm text-center"
        style={{ color: 'var(--color-oasis-text-muted)' }}
      >
        Skip for now
      </button>
    </div>
  );
}

function StepDone({ onFinish }: { onFinish: () => void }) {
  return (
    <div className="flex flex-col items-center gap-6 text-center">
      <p className="text-7xl">🎮</p>
      <div>
        <p className="text-xl font-bold" style={{ color: 'var(--color-oasis-text)' }}>
          RetroOasis is ready!
        </p>
        <p className="text-sm mt-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Head to the Library to browse games, or jump straight into a lobby.
          You can revisit these settings any time from the Settings page.
        </p>
      </div>
      <div className="flex flex-col gap-2 w-full">
        <button
          onClick={onFinish}
          className="w-full py-2 rounded-lg font-semibold"
          style={{ background: 'var(--color-oasis-accent)', color: '#fff' }}
        >
          Go to Home →
        </button>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Main wizard
// ---------------------------------------------------------------------------

export default function SetupPage() {
  const navigate = useNavigate();
  const [stepIdx, setStepIdx] = useState(0);
  const currentStep = STEP_LABELS[stepIdx];

  function advance() {
    if (stepIdx < STEP_LABELS.length - 1) {
      setStepIdx((i) => i + 1);
    }
  }

  function handleStep1(name: string) {
    completeStep('display-name', { displayName: name });
    // Persist display name to localStorage for LobbyContext to pick up
    if (name) {
      localStorage.setItem('retro-oasis-display-name', name);
    }
    advance();
  }

  function handleStep3(dir: string) {
    completeStep('rom-directory', { romDirectory: dir });
    if (dir) {
      localStorage.setItem('retro-oasis-rom-directory', dir);
    }
    advance();
  }

  function handleStep2() {
    completeStep('emulator-paths');
    advance();
  }

  function handleFinish() {
    completeStep('done');
    markFirstRunComplete();
    navigate('/');
  }

  return (
    <div
      className="min-h-screen flex items-center justify-center p-4"
      style={{ background: 'var(--color-oasis-bg)' }}
    >
      <div
        className="w-full max-w-md rounded-2xl p-8"
        style={{ background: 'var(--color-oasis-surface)', border: '1px solid var(--color-oasis-border)' }}
      >
        {/* Logo */}
        <div className="text-center mb-6">
          <p className="text-3xl font-black tracking-tight" style={{ color: 'var(--color-oasis-accent)' }}>
            RetroOasis
          </p>
          <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Curated retro multiplayer
          </p>
        </div>

        {/* Progress dots */}
        <ProgressDots current={stepIdx} />

        {/* Step title */}
        <div className="mb-6">
          <h2 className="text-lg font-bold" style={{ color: 'var(--color-oasis-text)' }}>
            {STEP_TITLES[currentStep]}
          </h2>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {STEP_SUBTITLES[currentStep]}
          </p>
        </div>

        {/* Step content */}
        {currentStep === 'display-name'   && <Step1DisplayName onNext={handleStep1} />}
        {currentStep === 'emulator-paths' && <Step2EmulatorPaths onNext={handleStep2} />}
        {currentStep === 'rom-directory'  && <Step3RomDirectory onNext={handleStep3} />}
        {currentStep === 'done'           && <StepDone onFinish={handleFinish} />}
      </div>
    </div>
  );
}
