/**
 * Friend Code store — persists Nintendo DS in-game friend codes.
 *
 * DS games use per-game 12-digit friend codes (formatted XXXX-XXXX-XXXX).
 * Players register their code so others can find them without out-of-band
 * coordination.
 */

export interface FriendCodeEntry {
  /** Composite ID: `<displayName>:<gameId>` */
  id: string;
  displayName: string;
  gameId: string;
  gameTitle: string;
  /** Formatted friend code, e.g. '1234-5678-9012' */
  code: string;
  createdAt: string;
}

const FRIEND_CODE_RE = /^\d{4}-\d{4}-\d{4}$/;

/** Returns an error message when the code is invalid, null when valid. */
export function validateFriendCode(code: string): string | null {
  if (!FRIEND_CODE_RE.test(code)) {
    return 'Friend code must be in the format XXXX-XXXX-XXXX (12 digits).';
  }
  return null;
}

/**
 * In-memory friend code store.
 * When DB_PATH is set a SQLite-backed variant can be added; for now
 * the in-memory version is sufficient to power the Phase 4 Pokémon UI.
 */
export class FriendCodeStore {
  private codes: Map<string, FriendCodeEntry> = new Map();

  /**
   * Register or update a friend code for a player + game.
   * Upserts by (displayName, gameId).
   */
  put(displayName: string, gameId: string, gameTitle: string, code: string): FriendCodeEntry {
    const id = `${displayName}:${gameId}`;
    const entry: FriendCodeEntry = {
      id,
      displayName,
      gameId,
      gameTitle,
      code,
      createdAt: new Date().toISOString(),
    };
    this.codes.set(id, entry);
    return entry;
  }

  /** Get all friend codes registered by a display name. */
  getByPlayer(displayName: string): FriendCodeEntry[] {
    return Array.from(this.codes.values()).filter((e) => e.displayName === displayName);
  }

  /** Get all friend codes for a given game — useful for building a directory. */
  getByGame(gameId: string): FriendCodeEntry[] {
    return Array.from(this.codes.values()).filter((e) => e.gameId === gameId);
  }

  /** Get all registered codes. */
  getAll(): FriendCodeEntry[] {
    return Array.from(this.codes.values());
  }

  /** Delete a friend code entry. Returns true if an entry was removed. */
  delete(displayName: string, gameId: string): boolean {
    return this.codes.delete(`${displayName}:${gameId}`);
  }
}
