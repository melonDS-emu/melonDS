/**
 * Persistent player identity store.
 *
 * Each WebSocket connection is ephemeral and gets a freshly generated
 * session UUID.  This store provides a persistent layer on top of that:
 * players supply an `identityToken` (a secret they persist client-side,
 * e.g. in localStorage) and the server maps it to a stable `playerId`
 * and `displayName`.
 *
 * On first use, a new identity record is created from the provided display
 * name.  Subsequent connections with the same token reuse the same
 * persistent player ID, preserving friend lists and session history.
 */

import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';

export interface PlayerIdentity {
  /** Stable player ID used for friend lists, session history, etc. */
  id: string;
  /** Human-readable display name. */
  displayName: string;
  /** ISO timestamp of first registration. */
  createdAt: string;
  /** ISO timestamp of most recent connection. */
  lastSeenAt: string;
}

interface PlayerRow {
  id: string;
  display_name: string;
  created_at: string;
  last_seen_at: string;
}

function rowToIdentity(row: PlayerRow): PlayerIdentity {
  return {
    id: row.id,
    displayName: row.display_name,
    createdAt: row.created_at,
    lastSeenAt: row.last_seen_at,
  };
}

export class PlayerIdentityStore {
  private db: DatabaseType;
  /** In-process cache: identityToken -> PlayerIdentity */
  private cache = new Map<string, PlayerIdentity>();

  constructor(db: DatabaseType) {
    this.db = db;
  }

  /**
   * Look up or create a player identity for the given `identityToken`.
   *
   * If the token is new, a fresh player record is created with the supplied
   * `displayName`.  If the token is known, the existing record is returned
   * (the `displayName` argument is ignored on subsequent calls unless `updateName`
   * is true).
   *
   * @param identityToken  Client-supplied opaque secret (e.g. a UUID from localStorage).
   * @param displayName    Desired display name (used only on first registration or when updateName=true).
   * @param updateName     When true, update the stored display name.
   */
  resolve(identityToken: string, displayName: string, updateName = false): PlayerIdentity {
    // Cache hit
    const cached = this.cache.get(identityToken);
    if (cached && !updateName) {
      // Update last_seen_at
      const now = new Date().toISOString();
      this.db.prepare('UPDATE players SET last_seen_at = ? WHERE id = ?').run(now, cached.id);
      cached.lastSeenAt = now;
      return cached;
    }

    // Token used as the stable player ID (it is a UUID generated on the client)
    const existingRow = this.db
      .prepare<string, PlayerRow>('SELECT * FROM players WHERE id = ?')
      .get(identityToken);

    const now = new Date().toISOString();
    let identity: PlayerIdentity;

    if (existingRow) {
      if (updateName) {
        this.db.prepare('UPDATE players SET display_name = ?, last_seen_at = ? WHERE id = ?')
          .run(displayName, now, identityToken);
      } else {
        this.db.prepare('UPDATE players SET last_seen_at = ? WHERE id = ?').run(now, identityToken);
      }
      const updated = this.db.prepare<string, PlayerRow>('SELECT * FROM players WHERE id = ?').get(identityToken)!;
      identity = rowToIdentity(updated);
    } else {
      this.db.prepare('INSERT INTO players (id, display_name, created_at, last_seen_at) VALUES (?, ?, ?, ?)')
        .run(identityToken, displayName, now, now);
      identity = { id: identityToken, displayName, createdAt: now, lastSeenAt: now };
    }

    this.cache.set(identityToken, identity);
    return identity;
  }

  /**
   * Look up an existing identity by stable player ID.
   */
  getById(playerId: string): PlayerIdentity | null {
    const row = this.db.prepare<string, PlayerRow>('SELECT * FROM players WHERE id = ?').get(playerId);
    return row ? rowToIdentity(row) : null;
  }

  /**
   * Update the display name for a given player.
   */
  updateDisplayName(playerId: string, displayName: string): PlayerIdentity | null {
    const row = this.db.prepare<string, PlayerRow>('SELECT * FROM players WHERE id = ?').get(playerId);
    if (!row) return null;
    const now = new Date().toISOString();
    this.db.prepare('UPDATE players SET display_name = ?, last_seen_at = ? WHERE id = ?').run(displayName, now, playerId);
    const updated = rowToIdentity({ ...row, display_name: displayName, last_seen_at: now });
    // Update cache entries that reference this player
    for (const [token, identity] of this.cache.entries()) {
      if (identity.id === playerId) {
        this.cache.set(token, updated);
      }
    }
    return updated;
  }

  /**
   * Generate a new identity token (UUID).  Call this server-side when
   * a client connects without an existing token.
   */
  static generateToken(): string {
    return randomUUID();
  }
}
