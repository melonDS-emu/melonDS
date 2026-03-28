/**
 * Recently-played store — tracks the last N games launched locally.
 *
 * Entries are ordered most-recent first. Duplicate game IDs are de-duped:
 * playing a game that is already in the list moves it to the front.
 */

const RECENTLY_PLAYED_KEY = 'retro-oasis-recently-played';
const MAX_ENTRIES = 20;

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export interface RecentlyPlayedEntry {
  /** Game catalog ID (e.g. 'gba-pokemon-fire-red'). */
  gameId: string;
  /** ISO timestamp of the most recent local launch. */
  playedAt: string;
}

// ---------------------------------------------------------------------------
// Persistence helpers
// ---------------------------------------------------------------------------

function loadEntries(): RecentlyPlayedEntry[] {
  try {
    const raw = localStorage.getItem(RECENTLY_PLAYED_KEY);
    return raw ? (JSON.parse(raw) as RecentlyPlayedEntry[]) : [];
  } catch {
    return [];
  }
}

function saveEntries(entries: RecentlyPlayedEntry[]): void {
  localStorage.setItem(RECENTLY_PLAYED_KEY, JSON.stringify(entries));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Record a game as recently played.
 *
 * If the game is already in the list it is moved to the front (de-duped).
 * The list is capped at MAX_ENTRIES oldest-entries dropped first.
 */
export function recordRecentlyPlayed(gameId: string): void {
  const entries = loadEntries().filter((e) => e.gameId !== gameId);
  entries.unshift({ gameId, playedAt: new Date().toISOString() });
  saveEntries(entries.slice(0, MAX_ENTRIES));
}

/**
 * Return recently-played entries in most-recent-first order.
 * Pass a `limit` to cap the returned list.
 */
export function getRecentlyPlayed(limit?: number): RecentlyPlayedEntry[] {
  const entries = loadEntries();
  return limit !== undefined ? entries.slice(0, limit) : entries;
}

/** Remove all recently-played history. */
export function clearRecentlyPlayed(): void {
  saveEntries([]);
}
