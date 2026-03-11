/**
 * SQLite-backed session history tracker.
 *
 * Persists session records to the database so they survive server restarts.
 * Includes per-player stats, most-played-games aggregation, and total session
 * time computations on top of the basic interface.
 */

import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';
import type { SessionRecord } from './session-history';

interface SessionRow {
  id: string;
  room_id: string;
  game_id: string;
  game_title: string;
  system: string;
  started_at: string;
  ended_at: string | null;
  duration_secs: number | null;
  player_count: number;
}

/** Per-game stats aggregated over all completed sessions. */
export interface GameStats {
  gameId: string;
  gameTitle: string;
  system: string;
  sessionCount: number;
  totalTimeSecs: number;
  uniquePlayers: number;
}

/** Per-player stats aggregated across sessions. */
export interface PlayerStats {
  displayName: string;
  sessionCount: number;
  totalTimeSecs: number;
  mostPlayedGame: string | null;
}

function rowToRecord(row: SessionRow, players: string[]): SessionRecord {
  return {
    id: row.id,
    roomId: row.room_id,
    gameId: row.game_id,
    gameTitle: row.game_title,
    system: row.system,
    startedAt: row.started_at,
    endedAt: row.ended_at,
    durationSecs: row.duration_secs,
    players,
    playerCount: row.player_count,
  };
}

export class SqliteSessionHistory {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
  }

  private getPlayers(sessionId: string): string[] {
    return this.db
      .prepare<string, { display_name: string }>('SELECT display_name FROM session_players WHERE session_id = ?')
      .all(sessionId)
      .map((r) => r.display_name);
  }

  // ---------------------------------------------------------------------------
  // Core CRUD (same interface as in-memory SessionHistory)
  // ---------------------------------------------------------------------------

  startSession(
    roomId: string,
    gameId: string,
    gameTitle: string,
    system: string,
    players: string[]
  ): SessionRecord {
    const id = randomUUID();
    const now = new Date().toISOString();

    this.db.prepare(`
      INSERT INTO sessions (id, room_id, game_id, game_title, system, started_at, player_count)
      VALUES (?, ?, ?, ?, ?, ?, ?)
    `).run(id, roomId, gameId, gameTitle, system, now, players.length);

    const insertPlayer = this.db.prepare(
      'INSERT OR IGNORE INTO session_players (session_id, display_name) VALUES (?, ?)'
    );
    for (const name of players) {
      insertPlayer.run(id, name);
    }

    return rowToRecord(
      this.db.prepare<string, SessionRow>('SELECT * FROM sessions WHERE id = ?').get(id)!,
      players
    );
  }

  endSession(roomId: string): SessionRecord | null {
    const row = this.db
      .prepare<string, SessionRow>(`SELECT * FROM sessions WHERE room_id = ? AND ended_at IS NULL`)
      .get(roomId);
    if (!row) return null;

    const now = new Date().toISOString();
    const durationSecs = Math.round(
      (new Date(now).getTime() - new Date(row.started_at).getTime()) / 1000
    );

    this.db.prepare('UPDATE sessions SET ended_at = ?, duration_secs = ? WHERE id = ?')
      .run(now, durationSecs, row.id);

    const players = this.getPlayers(row.id);
    return rowToRecord({ ...row, ended_at: now, duration_secs: durationSecs }, players);
  }

  getAll(): SessionRecord[] {
    const rows = this.db
      .prepare<[], SessionRow>('SELECT * FROM sessions ORDER BY started_at DESC')
      .all();
    return rows.map((r) => rowToRecord(r, this.getPlayers(r.id)));
  }

  getCompleted(): SessionRecord[] {
    const rows = this.db
      .prepare<[], SessionRow>('SELECT * FROM sessions WHERE ended_at IS NOT NULL ORDER BY started_at DESC')
      .all();
    return rows.map((r) => rowToRecord(r, this.getPlayers(r.id)));
  }

  getByRoomId(roomId: string): SessionRecord | null {
    const row = this.db
      .prepare<string, SessionRow>('SELECT * FROM sessions WHERE room_id = ? ORDER BY started_at DESC LIMIT 1')
      .get(roomId);
    if (!row) return null;
    return rowToRecord(row, this.getPlayers(row.id));
  }

  // ---------------------------------------------------------------------------
  // Phase 8: enhanced stats endpoints
  // ---------------------------------------------------------------------------

  /**
   * Return per-game stats (session count, total play time, unique players)
   * for all games that have at least one completed session.
   */
  getMostPlayedGames(limit = 10): GameStats[] {
    const rows = this.db.prepare<[number], {
      game_id: string;
      game_title: string;
      system: string;
      session_count: number;
      total_time_secs: number;
    }>(`
      SELECT
        s.game_id,
        s.game_title,
        s.system,
        COUNT(*) AS session_count,
        COALESCE(SUM(s.duration_secs), 0) AS total_time_secs
      FROM sessions s
      WHERE s.ended_at IS NOT NULL
      GROUP BY s.game_id
      ORDER BY session_count DESC, total_time_secs DESC
      LIMIT ?
    `).all(limit);

    return rows.map((r) => {
      const uniquePlayers = (this.db.prepare<string, { cnt: number }>(`
        SELECT COUNT(DISTINCT sp.display_name) AS cnt
        FROM session_players sp
        JOIN sessions s ON sp.session_id = s.id
        WHERE s.game_id = ? AND s.ended_at IS NOT NULL
      `).get(r.game_id) ?? { cnt: 0 }).cnt;

      return {
        gameId: r.game_id,
        gameTitle: r.game_title,
        system: r.system,
        sessionCount: r.session_count,
        totalTimeSecs: r.total_time_secs,
        uniquePlayers,
      };
    });
  }

  /**
   * Return total accumulated play time (in seconds) across all completed sessions.
   */
  getTotalPlayTimeSecs(): number {
    const row = this.db
      .prepare<[], { total: number }>('SELECT COALESCE(SUM(duration_secs), 0) AS total FROM sessions WHERE ended_at IS NOT NULL')
      .get();
    return row?.total ?? 0;
  }

  /**
   * Return stats for a specific player display name.
   */
  getPlayerStats(displayName: string): PlayerStats {
    const sessionCount = (this.db.prepare<string, { cnt: number }>(`
      SELECT COUNT(*) AS cnt
      FROM session_players sp
      JOIN sessions s ON sp.session_id = s.id
      WHERE sp.display_name = ? AND s.ended_at IS NOT NULL
    `).get(displayName) ?? { cnt: 0 }).cnt;

    const totalTime = (this.db.prepare<string, { total: number }>(`
      SELECT COALESCE(SUM(s.duration_secs), 0) AS total
      FROM session_players sp
      JOIN sessions s ON sp.session_id = s.id
      WHERE sp.display_name = ? AND s.ended_at IS NOT NULL
    `).get(displayName) ?? { total: 0 }).total;

    const mostPlayedRow = this.db.prepare<string, { game_title: string }>(`
      SELECT s.game_title
      FROM session_players sp
      JOIN sessions s ON sp.session_id = s.id
      WHERE sp.display_name = ? AND s.ended_at IS NOT NULL
      GROUP BY s.game_id
      ORDER BY COUNT(*) DESC
      LIMIT 1
    `).get(displayName);

    return {
      displayName,
      sessionCount,
      totalTimeSecs: totalTime,
      mostPlayedGame: mostPlayedRow?.game_title ?? null,
    };
  }
}
