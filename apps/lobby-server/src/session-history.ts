/**
 * In-memory session history tracker.
 *
 * Records each completed netplay session so it can be queried via GET /api/sessions.
 * This is intentionally in-memory — for persistence across restarts a future iteration
 * should write to SQLite or a flat-file store.
 */

import { randomUUID } from 'crypto';

export interface SessionRecord {
  id: string;
  roomId: string;
  gameId: string;
  gameTitle: string;
  system: string;
  /** ISO timestamp when the game was started. */
  startedAt: string;
  /** ISO timestamp when the session ended (all players left or the room closed). */
  endedAt: string | null;
  /** Duration in seconds (null until the session has ended). */
  durationSecs: number | null;
  /** Player display names that participated. */
  players: string[];
  /** Number of players (including spectators) who were in the room at start. */
  playerCount: number;
}

/** Maximum number of records to retain (oldest are evicted first). */
const MAX_HISTORY = 500;

export class SessionHistory {
  private records: Map<string, SessionRecord> = new Map();

  /**
   * Record that a game has started for the given room.
   */
  startSession(
    roomId: string,
    gameId: string,
    gameTitle: string,
    system: string,
    players: string[]
  ): SessionRecord {
    const record: SessionRecord = {
      id: randomUUID(),
      roomId,
      gameId,
      gameTitle,
      system,
      startedAt: new Date().toISOString(),
      endedAt: null,
      durationSecs: null,
      players,
      playerCount: players.length,
    };

    this.records.set(roomId, record);
    this.evictOldest();
    return record;
  }

  /**
   * Mark the session for the given room as ended.
   * No-op if the room has no active session.
   */
  endSession(roomId: string): SessionRecord | null {
    const record = this.records.get(roomId);
    if (!record || record.endedAt !== null) return null;

    record.endedAt = new Date().toISOString();
    record.durationSecs = Math.round(
      (new Date(record.endedAt).getTime() - new Date(record.startedAt).getTime()) / 1000
    );

    return record;
  }

  /**
   * Return all session records (most recent first).
   */
  getAll(): SessionRecord[] {
    return Array.from(this.records.values())
      .sort((a, b) => b.startedAt.localeCompare(a.startedAt));
  }

  /**
   * Return only completed (ended) sessions.
   */
  getCompleted(): SessionRecord[] {
    return this.getAll().filter((r) => r.endedAt !== null);
  }

  /**
   * Get a single session record by room ID.
   */
  getByRoomId(roomId: string): SessionRecord | null {
    return this.records.get(roomId) ?? null;
  }

  private evictOldest(): void {
    if (this.records.size <= MAX_HISTORY) return;
    const sorted = this.getAll();
    // Remove the oldest records
    const toRemove = sorted.slice(MAX_HISTORY);
    for (const r of toRemove) {
      this.records.delete(r.roomId);
    }
  }
}
