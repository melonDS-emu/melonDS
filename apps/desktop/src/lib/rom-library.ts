/**
 * ROM Library — per-game ROM file associations.
 *
 * Stores a mapping of `gameId → absolute ROM file path` in localStorage so the
 * app can auto-launch the correct ROM when a session starts. This bridges the
 * gap between the game catalog (which only knows about game IDs and systems)
 * and the local filesystem (where the user keeps their ROM files).
 *
 * The full native ROM scan + file-picker flow will use Tauri IPC to write these
 * associations once the Tauri shell integration is complete. Until then they can
 * be set manually via the Settings page or the game detail page.
 */

import { tauriCheckFileExists } from './tauri-ipc';

const ASSOCIATIONS_KEY = 'retro-oasis-rom-associations';

export interface RomAssociation {
  /** Game catalog ID (e.g. 'n64-mario-kart-64'). */
  gameId: string;
  /** Absolute path to the ROM file on the local filesystem. */
  romPath: string;
  /** ISO timestamp of when the association was last updated. */
  setAt: string;
}

// ---------------------------------------------------------------------------
// Persistence helpers
// ---------------------------------------------------------------------------

function loadAll(): Record<string, RomAssociation> {
  try {
    const raw = localStorage.getItem(ASSOCIATIONS_KEY);
    return raw ? (JSON.parse(raw) as Record<string, RomAssociation>) : {};
  } catch {
    return {};
  }
}

function saveAll(associations: Record<string, RomAssociation>): void {
  localStorage.setItem(ASSOCIATIONS_KEY, JSON.stringify(associations));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Associate a specific ROM file path with a game catalog entry.
 * Overwrites any existing association for the same game.
 *
 * Callers are responsible for ensuring `romPath` is a valid absolute path
 * before storing it. In the Tauri native build, the path should come from a
 * file-picker dialog (`tauriPickFile`) which returns absolute paths only.
 * In dev/browser mode, basic sanity checks should be applied in the UI layer.
 */
export function setRomAssociation(gameId: string, romPath: string): void {
  const all = loadAll();
  all[gameId] = { gameId, romPath: romPath.trim(), setAt: new Date().toISOString() };
  saveAll(all);
}

/**
 * Retrieve the ROM file path associated with a game, or `null` if none is set.
 */
export function getRomAssociation(gameId: string): RomAssociation | null {
  return loadAll()[gameId] ?? null;
}

/**
 * Remove the ROM file association for a game.
 */
export function clearRomAssociation(gameId: string): void {
  const all = loadAll();
  delete all[gameId];
  saveAll(all);
}

/**
 * Return all stored ROM associations as an array.
 */
export function getAllAssociations(): RomAssociation[] {
  return Object.values(loadAll());
}

/**
 * Resolve the best available ROM path for a game at launch time.
 *
 * Returns the explicit per-game ROM file path stored via `setRomAssociation`,
 * or `null` when none is set. A `null` result means the user still needs to
 * associate a ROM file for this game (via the file-picker or a ROM scan).
 *
 * Previously this function fell back to returning the ROM *directory* when no
 * explicit association existed, but passing a directory as a ROM path to an
 * emulator always fails. The caller must check for `null` and prompt the user
 * to set a ROM file.
 */
export function resolveGameRomPath(gameId: string): string | null {
  return getRomAssociation(gameId)?.romPath ?? null;
}

/**
 * Verify that the ROM file currently associated with a game still exists on
 * disk.  Returns `true` when the file exists (or when no association is set
 * yet, since there is nothing to verify), and `false` when the stored path
 * refers to a file that can no longer be found.
 *
 * Uses `tauriCheckFileExists` so it works both inside the Tauri native shell
 * and in browser / dev mode (where it falls back to the lobby-server HTTP API).
 */
export async function verifyRomAssociation(gameId: string): Promise<boolean> {
  const assoc = getRomAssociation(gameId);
  if (!assoc) return true; // nothing to verify
  return tauriCheckFileExists(assoc.romPath);
}

/**
 * Remove the stored ROM association for a game if the file no longer exists
 * on disk, and return whether it was cleared.
 */
export async function pruneStaleAssociation(gameId: string): Promise<boolean> {
  const valid = await verifyRomAssociation(gameId);
  if (!valid) {
    clearRomAssociation(gameId);
    return true;
  }
  return false;
}
