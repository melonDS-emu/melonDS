/**
 * Recent Players store — remembers which players you have recently shared a
 * room with, so they appear in the "People you've played with" list even after
 * a session ends.
 *
 * The store is keyed by persistent player ID and records the most recent
 * MAX_ENTRIES interactions per player.  Entries are evicted LRU-style when
 * the cap is hit.
 */

import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';

export interface RecentPlayerEntry {
  id: string;
  /** The player whose recent-players list this belongs to. */
  ownerId: string;
  /** The player who was met. */
  metPlayerId: string;
  metDisplayName: string;
  /** Room code of the shared session. */
  roomCode: string;
  /** Game being played at the time (if any). */
  gameTitle?: string;
  playedAt: string;
}

const MAX_ENTRIES = 50;

// ---------------------------------------------------------------------------
// In-memory implementation
// ---------------------------------------------------------------------------

export class RecentPlayersStore {
  /** map: ownerId → list ordered newest-first */
  private entries: Map<string, RecentPlayerEntry[]> = new Map();

  /**
   * Record that `ownerId` played in the same session as `metPlayerId`.
   * Idempotent within the same session (room code deduplicates same-session
   * entries within a short window).
   */
  record(
    ownerId: string,
    metPlayerId: string,
    metDisplayName: string,
    roomCode: string,
    gameTitle?: string,
  ): RecentPlayerEntry {
    const list = this.entries.get(ownerId) ?? [];

    // Deduplicate: update existing entry for the same session/player pair
    const existing = list.find(
      (e) => e.metPlayerId === metPlayerId && e.roomCode === roomCode,
    );
    if (existing) {
      existing.playedAt = new Date().toISOString();
      existing.gameTitle = gameTitle;
      return existing;
    }

    const entry: RecentPlayerEntry = {
      id: randomUUID(),
      ownerId,
      metPlayerId,
      metDisplayName,
      roomCode,
      gameTitle,
      playedAt: new Date().toISOString(),
    };

    list.unshift(entry);
    if (list.length > MAX_ENTRIES) list.splice(MAX_ENTRIES);
    this.entries.set(ownerId, list);
    return entry;
  }

  /** Return up to `limit` recent players for `ownerId` (newest first). */
  getRecent(ownerId: string, limit = 20): RecentPlayerEntry[] {
    return (this.entries.get(ownerId) ?? []).slice(0, limit);
  }

  /** Remove a single entry (e.g. if the other player was blocked). */
  remove(ownerId: string, entryId: string): boolean {
    const list = this.entries.get(ownerId);
    if (!list) return false;
    const before = list.length;
    const filtered = list.filter((e) => e.id !== entryId);
    this.entries.set(ownerId, filtered);
    return filtered.length < before;
  }

  /** Clear all recent players for an owner. */
  clear(ownerId: string): void {
    this.entries.delete(ownerId);
  }
}

// ---------------------------------------------------------------------------
// SQLite-backed implementation
// ---------------------------------------------------------------------------

interface RecentPlayerRow {
  id: string;
  owner_id: string;
  met_player_id: string;
  met_display_name: string;
  room_code: string;
  game_title: string | null;
  played_at: string;
}

function rowToEntry(row: RecentPlayerRow): RecentPlayerEntry {
  return {
    id: row.id,
    ownerId: row.owner_id,
    metPlayerId: row.met_player_id,
    metDisplayName: row.met_display_name,
    roomCode: row.room_code,
    gameTitle: row.game_title ?? undefined,
    playedAt: row.played_at,
  };
}

export class SqliteRecentPlayersStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
    db.exec(`
      CREATE TABLE IF NOT EXISTS recent_players (
        id               TEXT PRIMARY KEY,
        owner_id         TEXT NOT NULL,
        met_player_id    TEXT NOT NULL,
        met_display_name TEXT NOT NULL,
        room_code        TEXT NOT NULL,
        game_title       TEXT,
        played_at        TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS idx_recent_players_owner
        ON recent_players (owner_id, played_at DESC);
    `);
  }

  record(
    ownerId: string,
    metPlayerId: string,
    metDisplayName: string,
    roomCode: string,
    gameTitle?: string,
  ): RecentPlayerEntry {
    const now = new Date().toISOString();
    // Upsert by (owner_id, met_player_id, room_code)
    const existing = this.db
      .prepare<[string, string, string], RecentPlayerRow>(
        `SELECT * FROM recent_players
         WHERE owner_id = ? AND met_player_id = ? AND room_code = ?`,
      )
      .get(ownerId, metPlayerId, roomCode);

    if (existing) {
      this.db.prepare(
        `UPDATE recent_players SET played_at = ?, game_title = ? WHERE id = ?`,
      ).run(now, gameTitle ?? null, existing.id);
      return rowToEntry({ ...existing, played_at: now, game_title: gameTitle ?? null });
    }

    const id = randomUUID();
    this.db.prepare(`
      INSERT INTO recent_players
        (id, owner_id, met_player_id, met_display_name, room_code, game_title, played_at)
      VALUES (?, ?, ?, ?, ?, ?, ?)
    `).run(id, ownerId, metPlayerId, metDisplayName, roomCode, gameTitle ?? null, now);

    // Evict oldest entries beyond MAX_ENTRIES
    this.db.prepare(`
      DELETE FROM recent_players
      WHERE owner_id = ? AND id NOT IN (
        SELECT id FROM recent_players
        WHERE owner_id = ?
        ORDER BY played_at DESC
        LIMIT ${MAX_ENTRIES}
      )
    `).run(ownerId, ownerId);

    return rowToEntry(
      this.db.prepare<string, RecentPlayerRow>('SELECT * FROM recent_players WHERE id = ?').get(id)!,
    );
  }

  getRecent(ownerId: string, limit = 20): RecentPlayerEntry[] {
    return this.db
      .prepare<[string, number], RecentPlayerRow>(
        `SELECT * FROM recent_players WHERE owner_id = ? ORDER BY played_at DESC LIMIT ?`,
      )
      .all(ownerId, limit)
      .map(rowToEntry);
  }

  remove(ownerId: string, entryId: string): boolean {
    const result = this.db
      .prepare('DELETE FROM recent_players WHERE owner_id = ? AND id = ?')
      .run(ownerId, entryId);
    return result.changes > 0;
  }

  clear(ownerId: string): void {
    this.db.prepare('DELETE FROM recent_players WHERE owner_id = ?').run(ownerId);
  }
}
