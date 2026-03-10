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

import { getRomDirectory } from './rom-settings';

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
 * Resolution order:
 *   1. Explicit per-game association stored via `setRomAssociation`.
 *   2. Derived path: `<romDirectory>/<gameId>` — a simple heuristic used
 *      when no association exists but a ROM directory has been configured.
 *      The caller must still verify the file actually exists.
 *
 * Returns `null` when no ROM directory is configured and no association exists.
 */
export function resolveGameRomPath(gameId: string): string | null {
  const assoc = getRomAssociation(gameId);
  if (assoc) return assoc.romPath;

  const dir = getRomDirectory();
  if (!dir) return null;

  // Heuristic: the ROM directory itself is returned so the launch API can
  // attempt discovery. This preserves the pre-association legacy behaviour
  // where users pointed the ROM path at a directory.
  return dir;
}
