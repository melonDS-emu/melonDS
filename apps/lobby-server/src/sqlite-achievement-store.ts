/**
 * SQLite-backed achievement store.
 *
 * Persists earned achievements per-player so they survive server restarts.
 * Follows the same pattern as SqliteSessionHistory — the in-memory
 * AchievementStore is replaced by this class when DB_PATH is set.
 */

import type { DatabaseType } from './db';
import {
  ACHIEVEMENT_DEFS,
  type AchievementDef,
  type PlayerAchievement,
  type PlayerAchievementState,
} from './achievement-store';
import type { SessionRecord } from './session-history';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const MARIO_KART_IDS = new Set(['mk64', 'super-mario-kart', 'mario-kart-ds']);
const POKEMON_KEYWORDS = ['pokemon', 'pokémon'];
const PARTY_KEYWORDS = ['party', 'mario party'];
const ALL_SYSTEMS = ['NES', 'SNES', 'GB', 'GBC', 'GBA', 'N64', 'NDS'];

// ---------------------------------------------------------------------------
// SqliteAchievementStore
// ---------------------------------------------------------------------------

export class SqliteAchievementStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
  }

  /** Return (or create) achievement state for a player. */
  getState(playerId: string, displayName: string): PlayerAchievementState {
    const meta = this.db
      .prepare<string, { display_name: string; last_checked_at: string }>(
        'SELECT display_name, last_checked_at FROM player_achievement_meta WHERE player_id = ?'
      )
      .get(playerId);

    if (!meta) {
      const now = new Date().toISOString();
      this.db
        .prepare(
          'INSERT OR IGNORE INTO player_achievement_meta (player_id, display_name, last_checked_at) VALUES (?, ?, ?)'
        )
        .run(playerId, displayName, now);
    }

    return {
      playerId,
      displayName: meta?.display_name ?? displayName,
      earned: this.getEarned(playerId),
      lastCheckedAt: meta?.last_checked_at ?? new Date().toISOString(),
    };
  }

  /** Return earned achievements for a player (empty array if unknown). */
  getEarned(playerId: string): PlayerAchievement[] {
    return this.db
      .prepare<string, { achievement_id: string; unlocked_at: string }>(
        'SELECT achievement_id, unlocked_at FROM player_achievements WHERE player_id = ? ORDER BY unlocked_at ASC'
      )
      .all(playerId)
      .map((row) => ({
        achievementId: row.achievement_id,
        unlockedAt: row.unlocked_at,
      }));
  }

  /** All achievement definitions. */
  getDefinitions(): AchievementDef[] {
    return ACHIEVEMENT_DEFS;
  }

  /**
   * Check all achievements for a player given their complete session history.
   * Persists any newly earned achievements and returns the newly unlocked defs.
   */
  checkAndUnlock(
    playerId: string,
    displayName: string,
    sessions: SessionRecord[]
  ): AchievementDef[] {
    const earnedRows = this.db
      .prepare<string, { achievement_id: string }>(
        'SELECT achievement_id FROM player_achievements WHERE player_id = ?'
      )
      .all(playerId);
    const earnedIds = new Set(earnedRows.map((r) => r.achievement_id));
    const newly: AchievementDef[] = [];

    const completed = sessions.filter((s) => s.endedAt !== null);
    const now = new Date().toISOString();

    const insertAchievement = this.db.prepare(
      'INSERT OR IGNORE INTO player_achievements (player_id, achievement_id, unlocked_at) VALUES (?, ?, ?)'
    );

    for (const def of ACHIEVEMENT_DEFS) {
      if (earnedIds.has(def.id)) continue;
      if (this.isUnlocked(def, displayName, completed)) {
        insertAchievement.run(playerId, def.id, now);
        earnedIds.add(def.id);
        newly.push(def);
      }
    }

    // Upsert meta row with updated last-checked timestamp
    this.db
      .prepare(`
        INSERT INTO player_achievement_meta (player_id, display_name, last_checked_at)
        VALUES (?, ?, ?)
        ON CONFLICT(player_id) DO UPDATE SET display_name = excluded.display_name, last_checked_at = excluded.last_checked_at
      `)
      .run(playerId, displayName, now);

    return newly;
  }

  /** Total number of players with achievement records. */
  playerCount(): number {
    const row = this.db
      .prepare<[], { cnt: number }>('SELECT COUNT(*) AS cnt FROM player_achievement_meta')
      .get();
    return row?.cnt ?? 0;
  }

  // ---------------------------------------------------------------------------
  // Unlock predicates (mirrored from AchievementStore)
  // ---------------------------------------------------------------------------

  private isUnlocked(
    def: AchievementDef,
    displayName: string,
    completed: SessionRecord[]
  ): boolean {
    switch (def.id) {
      case 'first-session':
        return completed.length >= 1;
      case 'sessions-5':
        return completed.length >= 5;
      case 'sessions-25':
        return completed.length >= 25;
      case 'sessions-100':
        return completed.length >= 100;

      case 'marathon': {
        const totalSecs = completed.reduce((sum, s) => sum + (s.durationSecs ?? 0), 0);
        return totalSecs >= (def.threshold ?? 36000);
      }

      case 'long-session': {
        const longest = Math.max(0, ...completed.map((s) => s.durationSecs ?? 0));
        return longest >= (def.threshold ?? 3600);
      }

      case 'party-of-four':
        return completed.some((s) => s.playerCount >= 4);

      case 'social-butterfly': {
        const allNames = new Set<string>();
        for (const s of completed) {
          for (const name of s.players) {
            if (name !== displayName) allNames.add(name);
          }
        }
        return allNames.size >= (def.threshold ?? 10);
      }

      case 'host-master': {
        const hosted = completed.filter((s) => s.players[0] === displayName);
        return hosted.length >= (def.threshold ?? 10);
      }

      case 'n64-debut':
        return completed.some((s) => s.system.toUpperCase() === 'N64');
      case 'nds-debut':
        return completed.some((s) => s.system.toUpperCase() === 'NDS');

      case 'retro-purist': {
        const systems = new Set(completed.map((s) => s.system.toUpperCase()));
        return systems.size >= (def.threshold ?? 4);
      }
      case 'system-collector': {
        const systems = new Set(completed.map((s) => s.system.toUpperCase()));
        return ALL_SYSTEMS.every((sys) => systems.has(sys));
      }

      case 'mario-kart-fan': {
        const mkSessions = completed.filter(
          (s) =>
            MARIO_KART_IDS.has(s.gameId) ||
            s.gameTitle.toLowerCase().includes('mario kart')
        );
        return mkSessions.length >= (def.threshold ?? 5);
      }
      case 'pokemon-trainer': {
        const pkSessions = completed.filter((s) =>
          POKEMON_KEYWORDS.some((kw) => s.gameTitle.toLowerCase().includes(kw))
        );
        return pkSessions.length >= (def.threshold ?? 3);
      }
      case 'party-animal': {
        const partySessions = completed.filter((s) =>
          PARTY_KEYWORDS.some((kw) => s.gameTitle.toLowerCase().includes(kw))
        );
        return partySessions.length >= (def.threshold ?? 10);
      }

      case 'daily-player': {
        const days = new Set(completed.map((s) => s.startedAt.slice(0, 10)));
        return days.size >= (def.threshold ?? 7);
      }

      case 'comeback-king': {
        const sorted = [...completed].sort((a, b) =>
          a.startedAt.localeCompare(b.startedAt)
        );
        for (let i = 1; i < sorted.length; i++) {
          const prev = new Date(sorted[i - 1].startedAt).getTime();
          const curr = new Date(sorted[i].startedAt).getTime();
          if (curr - prev >= 7 * 24 * 60 * 60 * 1000) return true;
        }
        return false;
      }

      default:
        return false;
    }
  }
}
