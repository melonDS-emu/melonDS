/**
 * Tauri IPC — typed command surface for native desktop capabilities.
 *
 * RetroOasis targets both a browser/Vite dev build and a native Tauri desktop
 * app. This module wraps all Tauri-specific IPC commands and provides
 * transparent HTTP-API fallbacks so the same UI code runs correctly in both
 * environments.
 *
 * ## Implementation status
 * The `src-tauri/` package and the corresponding Rust command handlers are not
 * yet part of the repository. This module defines the **intended command
 * surface** that the Tauri integration will expose and serves as the
 * specification for the Rust-side implementation.
 *
 * When Tauri is available (`window.__TAURI__` is truthy), calls are routed via
 * `window.__TAURI__.invoke`. When it is not, the functions fall back to the
 * lobby-server HTTP API so the app remains functional in dev / browser mode.
 *
 * ## Carry-over blocker
 * Full Tauri integration is tracked as an explicit Phase 1 carry-over blocker.
 * The remaining work is:
 *   1. Scaffold `src-tauri/` with `tauri.conf.json`, `Cargo.toml`, and
 *      `src/main.rs`.
 *   2. Implement Rust handlers for each command listed below.
 *   3. Wire the Tauri build into the npm workspace (`npm run build:tauri`).
 *   4. Replace HTTP fallbacks with proper error messages once native features
 *      are confirmed present at runtime.
 */

/** Base URL of the local HTTP API (used as fallback outside Tauri). */
const API_BASE =
  (import.meta.env?.VITE_LAUNCH_API_URL as string | undefined) ??
  'http://localhost:8080';

// ---------------------------------------------------------------------------
// Runtime detection
// ---------------------------------------------------------------------------

/** Returns true when the app is running inside a Tauri native shell. */
export function isTauri(): boolean {
  return typeof window !== 'undefined' && '__TAURI__' in window;
}

/**
 * Thin wrapper around `window.__TAURI__.invoke` that keeps TypeScript happy
 * without scattering `(window as any)` casts throughout the file.
 * Only call this after confirming `isTauri()` returns true.
 */
function tauriInvoke<T>(command: string, args?: Record<string, unknown>): Promise<T> {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  return (window as any).__TAURI__.invoke(command, args) as Promise<T>;
}

// ---------------------------------------------------------------------------
// Type definitions  (mirrors the Rust command signatures)
// ---------------------------------------------------------------------------

export interface TauriRomScanResult {
  filePath: string;
  fileName: string;
  system: string;
  inferredTitle: string;
  fileSizeBytes: number;
  lastModified: string;
}

export interface TauriFilePickResult {
  /** Absolute path chosen by the user, or null if cancelled. */
  path: string | null;
}

export interface TauriDirectoryPickResult {
  /** Absolute directory path chosen by the user, or null if cancelled. */
  path: string | null;
}

export interface TauriLaunchResult {
  success: boolean;
  pid?: number;
  error?: string;
}

// ---------------------------------------------------------------------------
// Command: scan_rom_directory
// Tauri Rust signature: `#[tauri::command] fn scan_rom_directory(dir: String, recursive: bool)`
// ---------------------------------------------------------------------------

/**
 * Scan `dirPath` for ROM files.
 *
 * In Tauri: invokes the native `scan_rom_directory` command.
 * Fallback: calls `GET /api/roms/scan?dir=<path>&recursive=<bool>`.
 */
export async function tauriScanRoms(
  dirPath: string,
  recursive = false,
): Promise<TauriRomScanResult[]> {
  if (isTauri()) {
    return tauriInvoke<TauriRomScanResult[]>('scan_rom_directory', { dir: dirPath, recursive });
  }

  const params = new URLSearchParams({ dir: dirPath, recursive: String(recursive) });
  const res = await fetch(`${API_BASE}/api/roms/scan?${params}`);
  if (!res.ok) throw new Error(`ROM scan failed: HTTP ${res.status}`);
  return (await res.json()) as TauriRomScanResult[];
}

// ---------------------------------------------------------------------------
// Command: pick_file
// Tauri Rust signature: `#[tauri::command] fn pick_file(filters: Vec<FileFilter>)`
// ---------------------------------------------------------------------------

/**
 * Open a native file-picker dialog so the user can choose a ROM file.
 *
 * In Tauri: invokes the native `pick_file` command (uses `tauri-plugin-dialog`).
 * Fallback: resolves immediately with `{ path: null }` — the caller must
 *           present an alternative UI (e.g. a manual path text input).
 */
export async function tauriPickFile(
  filters?: Array<{ name: string; extensions: string[] }>,
): Promise<TauriFilePickResult> {
  if (isTauri()) {
    return tauriInvoke<TauriFilePickResult>('pick_file', { filters });
  }
  // Non-native: return null so callers know native picker isn't available.
  return { path: null };
}

// ---------------------------------------------------------------------------
// Command: pick_directory
// Tauri Rust signature: `#[tauri::command] fn pick_directory()`
// ---------------------------------------------------------------------------

/**
 * Open a native directory-picker dialog (e.g. for choosing the ROM folder).
 *
 * In Tauri: invokes `pick_directory`.
 * Fallback: returns `{ path: null }`.
 */
export async function tauriPickDirectory(): Promise<TauriDirectoryPickResult> {
  if (isTauri()) {
    return tauriInvoke<TauriDirectoryPickResult>('pick_directory');
  }
  return { path: null };
}

// ---------------------------------------------------------------------------
// Command: launch_emulator
// Tauri Rust signature: `#[tauri::command] fn launch_emulator(opts: LaunchOpts)`
// ---------------------------------------------------------------------------

/**
 * Launch an emulator process with the given options.
 *
 * In Tauri: invokes the native `launch_emulator` command which uses
 * `std::process::Command` directly and can inject the session token env var.
 * Fallback: calls `POST /api/launch` on the local lobby server.
 */
export async function tauriLaunchEmulator(opts: {
  romPath: string;
  system: string;
  backendId: string;
  saveDirectory?: string;
  playerSlot?: number;
  netplayHost?: string;
  netplayPort?: number;
  sessionToken?: string;
  fullscreen?: boolean;
}): Promise<TauriLaunchResult> {
  if (isTauri()) {
    return tauriInvoke<TauriLaunchResult>('launch_emulator', opts as Record<string, unknown>);
  }

  const res = await fetch(`${API_BASE}/api/launch`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(opts),
  });
  return (await res.json()) as TauriLaunchResult;
}

// ---------------------------------------------------------------------------
// Command: launch_local
// Single-player local launch — no netplay parameters required.
// Tauri Rust signature: `#[tauri::command] fn launch_local(opts: LocalLaunchOpts)`
// ---------------------------------------------------------------------------

/**
 * Launch an emulator for single-player / local play.
 *
 * Unlike `tauriLaunchEmulator` this requires no session token or netplay
 * addresses. In Tauri it invokes the `launch_local` command directly;
 * outside Tauri it falls back to `POST /api/launch/local`.
 *
 * @param emulatorPath - Absolute path to the emulator executable.
 * @param romPath      - Absolute path to the ROM file.
 * @param system       - System shortname (e.g. 'gba', 'nes').
 * @param backendId    - Backend ID (e.g. 'mgba', 'fceux').
 * @param saveDirectory - Optional directory for save files.
 * @param fullscreen    - Launch in fullscreen mode (default false).
 *
 * @returns `{ success: true, pid }` on success, or `{ success: false, error }` on
 *          failure where `error` is a human-readable message suitable for display
 *          in the UI (e.g. "ROM file not found" or "executable not found at path").
 */
export async function tauriLaunchLocal(opts: {
  emulatorPath: string;
  romPath: string;
  system: string;
  backendId: string;
  saveDirectory?: string;
  fullscreen?: boolean;
}): Promise<TauriLaunchResult> {
  if (isTauri()) {
    return tauriInvoke<TauriLaunchResult>('launch_local', opts as Record<string, unknown>);
  }

  const res = await fetch(`${API_BASE}/api/launch/local`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(opts),
  });
  if (!res.ok) {
    return { success: false, error: `HTTP ${res.status}` };
  }
  return (await res.json()) as TauriLaunchResult;
}

// ---------------------------------------------------------------------------
// Command: check_file_exists
// Tauri Rust signature: `#[tauri::command] fn check_file_exists(path: String) -> bool`
// ---------------------------------------------------------------------------

/**
 * Check whether a file exists at the given absolute path.
 *
 * In Tauri: invokes the native `check_file_exists` command.
 * Fallback: calls `GET /api/fs/exists?path=<path>`.
 *
 * Returns true if the file exists, false otherwise.
 */
export async function tauriCheckFileExists(filePath: string): Promise<boolean> {
  if (isTauri()) {
    return tauriInvoke<boolean>('check_file_exists', { path: filePath });
  }

  try {
    const params = new URLSearchParams({ path: filePath });
    const res = await fetch(`${API_BASE}/api/fs/exists?${params}`);
    if (!res.ok) return false;
    const data = (await res.json()) as { exists: boolean };
    return data.exists ?? false;
  } catch {
    return false;
  }
}
