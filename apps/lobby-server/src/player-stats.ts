/**
 * Player stats aggregation.
 *
 * Computes aggregate statistics from a player's session history.
 * All computation is done on-demand (no separate store); callers
 * pass the relevant completed SessionRecord array.
 */

import type { SessionRecord } from './session-history';

export interface SystemStat {
  system: string;
  sessions: number;
  totalSecs: number;
}

export interface GameStat {
  gameId: string;
  gameTitle: string;
  system: string;
  sessions: number;
  totalSecs: number;
}

export interface PlayerStats {
  playerId: string;
  displayName: string;
  totalSessions: number;
  completedSessions: number;
  totalPlaytimeSecs: number;
  /** Average session length in seconds (completed only). */
  avgSessionSecs: number;
  /** Longest session in seconds. */
  longestSessionSecs: number;
  /** Number of distinct players this player has played with. */
  uniquePlaymates: number;
  /** Per-system breakdown (sorted by sessions desc). */
  bySystem: SystemStat[];
  /** Top-5 most played games. */
  topGames: GameStat[];
  /** Total number of distinct games played (not limited to top 5). */
  uniqueGamesCount: number;
  /** Number of distinct calendar days with at least one session. */
  activeDays: number;
  /** ISO date of first ever session, or null. */
  firstSessionAt: string | null;
  /** ISO date of most recent session, or null. */
  lastSessionAt: string | null;
}

/**
 * Compute stats for a single player from a filtered list of their sessions.
 *
 * @param playerId  Player identifier.
 * @param displayName  Player display name.
 * @param sessions  All session records that include this player (may include
 *                  active/incomplete sessions — completed ones are filtered
 *                  internally for time-based stats).
 */
export function computePlayerStats(
  playerId: string,
  displayName: string,
  sessions: SessionRecord[]
): PlayerStats {
  const completed = sessions.filter((s) => s.endedAt !== null);

  const totalPlaytimeSecs = completed.reduce(
    (sum, s) => sum + (s.durationSecs ?? 0),
    0
  );
  const avgSessionSecs =
    completed.length > 0 ? Math.round(totalPlaytimeSecs / completed.length) : 0;
  const longestSessionSecs =
    completed.length > 0
      ? Math.max(...completed.map((s) => s.durationSecs ?? 0))
      : 0;

  // Unique playmates (excluding ourselves)
  const playmates = new Set<string>();
  for (const s of sessions) {
    for (const name of s.players) {
      if (name !== displayName) playmates.add(name);
    }
  }

  // Per-system breakdown
  const systemMap = new Map<string, SystemStat>();
  for (const s of sessions) {
    const sys = s.system.toUpperCase();
    const existing = systemMap.get(sys) ?? { system: sys, sessions: 0, totalSecs: 0 };
    existing.sessions++;
    existing.totalSecs += s.durationSecs ?? 0;
    systemMap.set(sys, existing);
  }
  const bySystem = [...systemMap.values()].sort((a, b) => b.sessions - a.sessions);

  // Top-5 games
  const gameMap = new Map<string, GameStat>();
  for (const s of sessions) {
    const existing = gameMap.get(s.gameId) ?? {
      gameId: s.gameId,
      gameTitle: s.gameTitle,
      system: s.system,
      sessions: 0,
      totalSecs: 0,
    };
    existing.sessions++;
    existing.totalSecs += s.durationSecs ?? 0;
    gameMap.set(s.gameId, existing);
  }
  const allGames = [...gameMap.values()].sort((a, b) => b.sessions - a.sessions);
  const topGames = allGames.slice(0, 5);
  const uniqueGamesCount = gameMap.size;

  // Active days
  const days = new Set(sessions.map((s) => s.startedAt.slice(0, 10)));

  const sorted = [...sessions].sort((a, b) =>
    a.startedAt.localeCompare(b.startedAt)
  );

  return {
    playerId,
    displayName,
    totalSessions: sessions.length,
    completedSessions: completed.length,
    totalPlaytimeSecs,
    avgSessionSecs,
    longestSessionSecs,
    uniquePlaymates: playmates.size,
    bySystem,
    topGames,
    uniqueGamesCount,
    activeDays: days.size,
    firstSessionAt: sorted.length > 0 ? sorted[0].startedAt : null,
    lastSessionAt: sorted.length > 0 ? sorted[sorted.length - 1].startedAt : null,
  };
}

/** Leaderboard entry. */
export interface LeaderboardEntry {
  rank: number;
  playerId: string;
  displayName: string;
  value: number;
}

export type LeaderboardMetric = 'sessions' | 'playtime' | 'games' | 'systems';

/**
 * Compute a global leaderboard from aggregated per-player stats.
 */
export function computeLeaderboard(
  allStats: PlayerStats[],
  metric: LeaderboardMetric = 'sessions',
  limit = 10
): LeaderboardEntry[] {
  const scored = allStats.map((s) => ({
    playerId: s.playerId,
    displayName: s.displayName,
    value:
      metric === 'sessions'
        ? s.completedSessions
        : metric === 'playtime'
          ? s.totalPlaytimeSecs
          : metric === 'games'
            ? s.uniqueGamesCount
            : s.bySystem.length,
  }));

  return scored
    .sort((a, b) => b.value - a.value)
    .slice(0, limit)
    .map((entry, idx) => ({ rank: idx + 1, ...entry }));
}
