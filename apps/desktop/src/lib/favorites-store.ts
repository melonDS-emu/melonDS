/**
 * Favorites store — per-user favorites list persisted in localStorage.
 *
 * Favorites are stored as a simple array of game IDs. The store is
 * intentionally lightweight: no timestamps, no ordering. The LibraryPage
 * renders favorited games in a dedicated section at the top.
 */

const FAVORITES_KEY = 'retro-oasis-favorites';

// ---------------------------------------------------------------------------
// Persistence helpers
// ---------------------------------------------------------------------------

function loadFavorites(): Set<string> {
  try {
    const raw = localStorage.getItem(FAVORITES_KEY);
    const arr = raw ? (JSON.parse(raw) as string[]) : [];
    return new Set(arr);
  } catch {
    return new Set();
  }
}

function saveFavorites(set: Set<string>): void {
  localStorage.setItem(FAVORITES_KEY, JSON.stringify([...set]));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/** Returns true if the given game is in the favorites list. */
export function isFavorite(gameId: string): boolean {
  return loadFavorites().has(gameId);
}

/**
 * Toggle a game's favorite status.
 * Returns the new favorite state (true = now favorited).
 */
export function toggleFavorite(gameId: string): boolean {
  const faves = loadFavorites();
  if (faves.has(gameId)) {
    faves.delete(gameId);
  } else {
    faves.add(gameId);
  }
  saveFavorites(faves);
  return faves.has(gameId);
}

/** Return all favorited game IDs as an array. */
export function getFavoriteIds(): string[] {
  return [...loadFavorites()];
}

/** Remove all favorites. */
export function clearFavorites(): void {
  saveFavorites(new Set());
}
