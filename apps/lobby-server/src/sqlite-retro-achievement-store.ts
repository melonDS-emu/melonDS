/**
 * SQLite-backed Retro Achievement store.
 *
 * Persists per-player retro achievement progress across server restarts.
 * Implements the same public API as `RetroAchievementStore` so callers can
 * swap between the two transparently.
 *
 * Schema (added via Phase 24 migration in db.ts):
 *   retro_achievement_progress (player_id, achievement_id, game_id, earned_at, session_id)
 */

import type { DatabaseType } from './db';
import {
  RETRO_ACHIEVEMENT_DEFS,
  type RetroGameAchievementDef,
  type EarnedRetroAchievement,
  type RetroPlayerProgress,
  type RetroGameSummary,
  type RetroLeaderboardEntry,
} from './retro-achievement-store';

// ---------------------------------------------------------------------------
// SqliteRetroAchievementStore
// ---------------------------------------------------------------------------

export class SqliteRetroAchievementStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
  }

  // ── read-only catalog helpers (identical to in-memory store) ──────────────

  getDefinitions(): RetroGameAchievementDef[] {
    return RETRO_ACHIEVEMENT_DEFS;
  }

  getGameDefinitions(gameId: string): RetroGameAchievementDef[] {
    return RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === gameId);
  }

  getGameIds(): string[] {
    return [...new Set(RETRO_ACHIEVEMENT_DEFS.map((d) => d.gameId))];
  }

  getGameSummaries(): RetroGameSummary[] {
    const map = new Map<string, RetroGameSummary>();
    for (const def of RETRO_ACHIEVEMENT_DEFS) {
      const existing = map.get(def.gameId);
      if (existing) {
        existing.totalAchievements++;
        existing.totalPoints += def.points;
      } else {
        map.set(def.gameId, {
          gameId: def.gameId,
          totalAchievements: 1,
          totalPoints: def.points,
        });
      }
    }
    return [...map.values()];
  }

  // ── per-player progress ───────────────────────────────────────────────────

  /** Return progress for a player. Creates no row in DB (reads on-demand). */
  getProgress(playerId: string): RetroPlayerProgress {
    const earned = this.getEarned(playerId);
    const totalPoints = earned.reduce((sum, e) => {
      const def = RETRO_ACHIEVEMENT_DEFS.find((d) => d.id === e.achievementId);
      return sum + (def?.points ?? 0);
    }, 0);
    return { playerId, earned, totalPoints };
  }

  getEarned(playerId: string, gameId?: string): EarnedRetroAchievement[] {
    type Row = { achievement_id: string; game_id: string; earned_at: string; session_id: string | null };
    if (gameId) {
      return this.db
        .prepare<[string, string], Row>(
          'SELECT achievement_id, game_id, earned_at, session_id FROM retro_achievement_progress WHERE player_id = ? AND game_id = ? ORDER BY earned_at ASC'
        )
        .all(playerId, gameId)
        .map((r) => ({
          achievementId: r.achievement_id,
          gameId: r.game_id,
          earnedAt: r.earned_at,
          ...(r.session_id != null ? { sessionId: r.session_id } : {}),
        }));
    }
    return this.db
      .prepare<[string], Row>(
        'SELECT achievement_id, game_id, earned_at, session_id FROM retro_achievement_progress WHERE player_id = ? ORDER BY earned_at ASC'
      )
      .all(playerId)
      .map((r) => ({
        achievementId: r.achievement_id,
        gameId: r.game_id,
        earnedAt: r.earned_at,
        ...(r.session_id != null ? { sessionId: r.session_id } : {}),
      }));
  }

  unlock(
    playerId: string,
    achievementId: string,
    sessionId?: string
  ): RetroGameAchievementDef | null {
    const def = RETRO_ACHIEVEMENT_DEFS.find((d) => d.id === achievementId);
    if (!def) return null;

    const existing = this.db
      .prepare<[string, string], { achievement_id: string }>(
        'SELECT achievement_id FROM retro_achievement_progress WHERE player_id = ? AND achievement_id = ?'
      )
      .get(playerId, achievementId);
    if (existing) return null;

    const now = new Date().toISOString();
    this.db
      .prepare(
        'INSERT INTO retro_achievement_progress (player_id, achievement_id, game_id, earned_at, session_id) VALUES (?, ?, ?, ?, ?)'
      )
      .run(playerId, achievementId, def.gameId, now, sessionId ?? null);

    return def;
  }

  unlockMany(
    playerId: string,
    achievementIds: string[],
    sessionId?: string
  ): RetroGameAchievementDef[] {
    return achievementIds
      .map((id) => this.unlock(playerId, id, sessionId))
      .filter((d): d is RetroGameAchievementDef => d !== null);
  }

  playerCount(): number {
    const row = this.db
      .prepare<[], { cnt: number }>('SELECT COUNT(DISTINCT player_id) AS cnt FROM retro_achievement_progress')
      .get();
    return row?.cnt ?? 0;
  }

  getLeaderboard(limit = 10): RetroLeaderboardEntry[] {
    // Use SQL to get per-player counts; compute point totals in memory
    // using the catalog (avoids storing points in the DB and keeps it DRY).
    const pointMap = new Map<string, number>();
    for (const def of RETRO_ACHIEVEMENT_DEFS) {
      pointMap.set(def.id, def.points);
    }

    type Row = { player_id: string; achievement_id: string };
    const rows = this.db
      .prepare<[], Row>(
        'SELECT player_id, achievement_id FROM retro_achievement_progress ORDER BY earned_at ASC'
      )
      .all();

    const map = new Map<string, { totalPoints: number; earnedCount: number }>();
    for (const row of rows) {
      const pts = pointMap.get(row.achievement_id) ?? 0;
      const entry = map.get(row.player_id) ?? { totalPoints: 0, earnedCount: 0 };
      entry.totalPoints += pts;
      entry.earnedCount++;
      map.set(row.player_id, entry);
    }

    const entries: RetroLeaderboardEntry[] = [...map.entries()].map(
      ([playerId, { totalPoints, earnedCount }]) => ({ playerId, totalPoints, earnedCount })
    );
    entries.sort((a, b) => {
      if (b.totalPoints !== a.totalPoints) return b.totalPoints - a.totalPoints;
      if (b.earnedCount !== a.earnedCount) return b.earnedCount - a.earnedCount;
      return a.playerId.localeCompare(b.playerId);
    });
    return entries.slice(0, limit);
  }
}
