/**
 * Player Privacy store — persists per-player privacy preferences.
 *
 * Currently tracks:
 *  - showOnline:    whether the player's online/in-game status is visible to
 *                   non-friends (defaults true).
 *  - allowInvites:  whether the player can receive game invites from non-friends
 *                   (defaults true).
 *  - showActivity:  whether this player's activity appears in the global feed
 *                   (defaults true).
 */

import type { DatabaseType } from './db';

export interface PlayerPrivacySettings {
  /** Persistent player ID. */
  playerId: string;
  /** Show online/in-game status to everyone (false = friends only). */
  showOnline: boolean;
  /** Accept game invites from anyone (false = friends only). */
  allowInvites: boolean;
  /** Appear in the global activity feed. */
  showActivity: boolean;
  updatedAt: string;
}

const DEFAULTS: Omit<PlayerPrivacySettings, 'playerId' | 'updatedAt'> = {
  showOnline: true,
  allowInvites: true,
  showActivity: true,
};

// ---------------------------------------------------------------------------
// In-memory implementation
// ---------------------------------------------------------------------------

export class PlayerPrivacyStore {
  private settings: Map<string, PlayerPrivacySettings> = new Map();

  /** Return privacy settings for a player (defaults if never set). */
  get(playerId: string): PlayerPrivacySettings {
    return (
      this.settings.get(playerId) ?? {
        playerId,
        ...DEFAULTS,
        updatedAt: new Date().toISOString(),
      }
    );
  }

  /** Upsert privacy settings (partial update: only provided keys are changed). */
  update(
    playerId: string,
    patch: Partial<Omit<PlayerPrivacySettings, 'playerId' | 'updatedAt'>>,
  ): PlayerPrivacySettings {
    const current = this.get(playerId);
    const updated: PlayerPrivacySettings = {
      ...current,
      ...patch,
      playerId,
      updatedAt: new Date().toISOString(),
    };
    this.settings.set(playerId, updated);
    return updated;
  }
}

// ---------------------------------------------------------------------------
// SQLite-backed implementation
// ---------------------------------------------------------------------------

interface PrivacyRow {
  player_id: string;
  show_online: number;
  allow_invites: number;
  show_activity: number;
  updated_at: string;
}

function rowToSettings(row: PrivacyRow): PlayerPrivacySettings {
  return {
    playerId: row.player_id,
    showOnline: row.show_online !== 0,
    allowInvites: row.allow_invites !== 0,
    showActivity: row.show_activity !== 0,
    updatedAt: row.updated_at,
  };
}

export class SqlitePlayerPrivacyStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
    db.exec(`
      CREATE TABLE IF NOT EXISTS player_privacy (
        player_id     TEXT PRIMARY KEY,
        show_online   INTEGER NOT NULL DEFAULT 1,
        allow_invites INTEGER NOT NULL DEFAULT 1,
        show_activity INTEGER NOT NULL DEFAULT 1,
        updated_at    TEXT NOT NULL
      )
    `);
  }

  get(playerId: string): PlayerPrivacySettings {
    const row = this.db
      .prepare<string, PrivacyRow>('SELECT * FROM player_privacy WHERE player_id = ?')
      .get(playerId);
    if (row) return rowToSettings(row);
    return { playerId, ...DEFAULTS, updatedAt: new Date().toISOString() };
  }

  update(
    playerId: string,
    patch: Partial<Omit<PlayerPrivacySettings, 'playerId' | 'updatedAt'>>,
  ): PlayerPrivacySettings {
    const current = this.get(playerId);
    const merged = { ...current, ...patch };
    const now = new Date().toISOString();
    this.db.prepare(`
      INSERT INTO player_privacy (player_id, show_online, allow_invites, show_activity, updated_at)
      VALUES (?, ?, ?, ?, ?)
      ON CONFLICT(player_id) DO UPDATE SET
        show_online   = excluded.show_online,
        allow_invites = excluded.allow_invites,
        show_activity = excluded.show_activity,
        updated_at    = excluded.updated_at
    `).run(
      playerId,
      merged.showOnline ? 1 : 0,
      merged.allowInvites ? 1 : 0,
      merged.showActivity ? 1 : 0,
      now,
    );
    return { ...merged, playerId, updatedAt: now };
  }
}
