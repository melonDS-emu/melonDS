/**
 * Matchmaking queue.
 *
 * Players join the queue specifying the game and desired player count.
 * When enough players are waiting for the same game/system/maxPlayers
 * combination the queue emits a `match-found` event so the caller can
 * auto-create a room.
 *
 * State is persisted to SQLite so in-flight queues survive a restart.
 */

import type { DatabaseType } from './db';

export interface MatchmakingEntry {
  playerId: string;
  displayName: string;
  gameId: string;
  gameTitle: string;
  system: string;
  maxPlayers: number;
  joinedAt: string;
}

export interface MatchResult {
  gameId: string;
  gameTitle: string;
  system: string;
  maxPlayers: number;
  players: MatchmakingEntry[];
}

interface QueueRow {
  player_id: string;
  display_name: string;
  game_id: string;
  game_title: string;
  system: string;
  max_players: number;
  joined_at: string;
}

function rowToEntry(row: QueueRow): MatchmakingEntry {
  return {
    playerId: row.player_id,
    displayName: row.display_name,
    gameId: row.game_id,
    gameTitle: row.game_title,
    system: row.system,
    maxPlayers: row.max_players,
    joinedAt: row.joined_at,
  };
}

export class MatchmakingQueue {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
  }

  /**
   * Add a player to the queue.
   * Returns the entry if it was added (or already existed).
   */
  join(entry: Omit<MatchmakingEntry, 'joinedAt'>): MatchmakingEntry {
    const existing = this.db
      .prepare<string, QueueRow>('SELECT * FROM matchmaking_queue WHERE player_id = ?')
      .get(entry.playerId);

    if (existing) return rowToEntry(existing);

    const now = new Date().toISOString();
    this.db.prepare(`
      INSERT INTO matchmaking_queue (player_id, display_name, game_id, game_title, system, max_players, joined_at)
      VALUES (?, ?, ?, ?, ?, ?, ?)
    `).run(entry.playerId, entry.displayName, entry.gameId, entry.gameTitle, entry.system, entry.maxPlayers, now);

    return { ...entry, joinedAt: now };
  }

  /**
   * Remove a player from the queue.
   * Returns true if they were in the queue.
   */
  leave(playerId: string): boolean {
    const result = this.db.prepare('DELETE FROM matchmaking_queue WHERE player_id = ?').run(playerId);
    return result.changes > 0;
  }

  /**
   * Return all entries currently in the queue.
   */
  getAll(): MatchmakingEntry[] {
    return this.db
      .prepare<[], QueueRow>('SELECT * FROM matchmaking_queue ORDER BY joined_at ASC')
      .all()
      .map(rowToEntry);
  }

  /**
   * Check whether a player is currently in the queue.
   */
  isQueued(playerId: string): boolean {
    return !!this.db
      .prepare<string, { player_id: string }>('SELECT player_id FROM matchmaking_queue WHERE player_id = ?')
      .get(playerId);
  }

  /**
   * Scan the queue for groups that have filled up to `maxPlayers` for the
   * same (gameId, system, maxPlayers) combination.
   *
   * Returns an array of MatchResult objects (one per filled group) and removes
   * the matched players from the queue.
   */
  flushMatches(): MatchResult[] {
    const all = this.getAll();
    const results: MatchResult[] = [];

    // Group by (gameId, system, maxPlayers)
    type GroupKey = string;
    const groups = new Map<GroupKey, MatchmakingEntry[]>();
    for (const entry of all) {
      const key = `${entry.gameId}::${entry.system}::${entry.maxPlayers}`;
      if (!groups.has(key)) groups.set(key, []);
      groups.get(key)!.push(entry);
    }

    for (const [, entries] of groups) {
      const maxPlayers = entries[0].maxPlayers;
      while (entries.length >= maxPlayers) {
        const matched = entries.splice(0, maxPlayers);
        // Remove matched players from the queue
        const del = this.db.prepare('DELETE FROM matchmaking_queue WHERE player_id = ?');
        for (const e of matched) del.run(e.playerId);

        results.push({
          gameId: matched[0].gameId,
          gameTitle: matched[0].gameTitle,
          system: matched[0].system,
          maxPlayers,
          players: matched,
        });
      }
    }

    return results;
  }
}
