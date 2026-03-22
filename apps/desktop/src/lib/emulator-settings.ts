/**
 * Emulator settings — per-backend executable path configuration.
 *
 * Each emulator backend has an optional user-configured executable path stored
 * in localStorage. When a path is not configured, the SettingsPage shows a
 * list of common installation locations so the user knows where to look.
 *
 * `getEmulatorPathForSystem()` is the primary entry point used by the launch
 * flow: given a system shortname it resolves the backend, then returns the
 * configured path (or null if none is set).
 */

const EMULATOR_PATHS_KEY = 'retro-oasis-emulator-paths';

// ---------------------------------------------------------------------------
// Emulator display names (backend ID → human-readable name)
// ---------------------------------------------------------------------------

export const EMULATOR_NAMES: Record<string, string> = {
  mgba: 'mGBA',
  sameboy: 'SameBoy',
  fceux: 'FCEUX',
  snes9x: 'Snes9x',
  mupen64plus: 'Mupen64Plus',
  melonds: 'melonDS',
  dolphin: 'Dolphin',
  citra: 'Citra / Lime3DS',
  retroarch: 'RetroArch',
  ppsspp: 'PPSSPP',
  pcsx2: 'PCSX2',
  duckstation: 'DuckStation',
  flycast: 'Flycast',
  cemu: 'Cemu',
};

// ---------------------------------------------------------------------------
// System → default backend map
// ---------------------------------------------------------------------------

export const SYSTEM_BACKEND_MAP: Record<string, string> = {
  gba: 'mgba',
  gb: 'sameboy',
  gbc: 'sameboy',
  nes: 'fceux',
  snes: 'snes9x',
  n64: 'mupen64plus',
  nds: 'melonds',
  gc: 'dolphin',
  '3ds': 'citra',
  wii: 'dolphin',
  wiiu: 'cemu',
  genesis: 'retroarch',
  sms: 'retroarch',
  psx: 'duckstation',
  ps2: 'pcsx2',
  psp: 'ppsspp',
  dreamcast: 'flycast',
};

// ---------------------------------------------------------------------------
// Suggested default paths per backend (for the Settings UI hint list)
// ---------------------------------------------------------------------------

export const EMULATOR_DEFAULT_PATHS: Record<string, string[]> = {
  mgba: [
    '/usr/bin/mgba',
    '/usr/local/bin/mgba',
    '/Applications/mGBA.app/Contents/MacOS/mGBA',
    'C:\\Program Files\\mGBA\\mGBA.exe',
  ],
  sameboy: [
    '/usr/bin/sameboy',
    '/usr/local/bin/sameboy',
    '/Applications/SameBoy.app/Contents/MacOS/SameBoy',
  ],
  fceux: [
    '/usr/bin/fceux',
    '/usr/local/bin/fceux',
    'C:\\Program Files\\FCEUX\\fceux.exe',
  ],
  snes9x: [
    '/usr/bin/snes9x-gtk',
    '/usr/local/bin/snes9x',
    '/Applications/Snes9x.app/Contents/MacOS/Snes9x',
    'C:\\Program Files\\Snes9x\\snes9x.exe',
  ],
  mupen64plus: [
    '/usr/bin/mupen64plus',
    '/usr/local/bin/mupen64plus',
  ],
  melonds: [
    '/usr/bin/melonds',
    '/usr/local/bin/melonds',
    '/Applications/melonDS.app/Contents/MacOS/melonDS',
    'C:\\Program Files\\melonDS\\melonDS.exe',
  ],
  dolphin: [
    '/usr/bin/dolphin-emu',
    '/usr/local/bin/dolphin-emu',
    '/Applications/Dolphin.app/Contents/MacOS/Dolphin',
    'C:\\Program Files\\Dolphin\\Dolphin.exe',
  ],
  citra: [
    '/usr/bin/citra-qt',
    '/usr/local/bin/citra-qt',
    '/Applications/Citra.app/Contents/MacOS/citra-qt',
    'C:\\Program Files\\Citra\\citra-qt.exe',
  ],
  retroarch: [
    '/usr/bin/retroarch',
    '/usr/local/bin/retroarch',
    '/Applications/RetroArch.app/Contents/MacOS/RetroArch',
    'C:\\Program Files\\RetroArch\\retroarch.exe',
  ],
  ppsspp: [
    '/usr/bin/ppsspp',
    '/Applications/PPSSPP.app/Contents/MacOS/PPSSPP',
    'C:\\Program Files\\PPSSPP\\PPSSPPWindows64.exe',
  ],
  pcsx2: [
    '/usr/bin/pcsx2',
    '/Applications/PCSX2.app/Contents/MacOS/PCSX2',
    'C:\\Program Files\\PCSX2\\pcsx2.exe',
  ],
  duckstation: [
    '/usr/bin/duckstation-qt',
    '/Applications/DuckStation.app/Contents/MacOS/DuckStation',
    'C:\\Program Files\\DuckStation\\duckstation-qt-x64-ReleaseLTCG.exe',
  ],
  flycast: [
    '/usr/bin/flycast',
    '/Applications/Flycast.app/Contents/MacOS/Flycast',
    'C:\\Program Files\\Flycast\\flycast.exe',
  ],
  cemu: [
    '/usr/bin/Cemu',
    '/Applications/Cemu.app/Contents/MacOS/Cemu',
    'C:\\Program Files\\Cemu\\Cemu.exe',
  ],
};

// ---------------------------------------------------------------------------
// Persistence helpers
// ---------------------------------------------------------------------------

function loadPaths(): Record<string, string> {
  try {
    const raw = localStorage.getItem(EMULATOR_PATHS_KEY);
    return raw ? (JSON.parse(raw) as Record<string, string>) : {};
  } catch {
    return {};
  }
}

function savePaths(paths: Record<string, string>): void {
  localStorage.setItem(EMULATOR_PATHS_KEY, JSON.stringify(paths));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/** Get the user-configured executable path for an emulator backend, or null. */
export function getEmulatorPath(backendId: string): string | null {
  return loadPaths()[backendId] ?? null;
}

/** Persist the executable path for an emulator backend. */
export function setEmulatorPath(backendId: string, exePath: string): void {
  const paths = loadPaths();
  paths[backendId] = exePath.trim();
  savePaths(paths);
}

/** Remove the configured path for an emulator backend. */
export function clearEmulatorPath(backendId: string): void {
  const paths = loadPaths();
  delete paths[backendId];
  savePaths(paths);
}

/** Return all configured emulator paths as a plain object. */
export function getAllEmulatorPaths(): Record<string, string> {
  return { ...loadPaths() };
}

/**
 * Get the configured executable path for the default backend of a system.
 * Returns null when no path is configured or the system is unrecognised.
 */
export function getEmulatorPathForSystem(system: string): string | null {
  const backendId = SYSTEM_BACKEND_MAP[system.toLowerCase()];
  if (!backendId) return null;
  return getEmulatorPath(backendId);
}

/**
 * Get the backend ID for a system, or null if unrecognised.
 */
export function getBackendIdForSystem(system: string): string | null {
  return SYSTEM_BACKEND_MAP[system.toLowerCase()] ?? null;
}
