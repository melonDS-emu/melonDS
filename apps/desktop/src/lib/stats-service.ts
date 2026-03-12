/**
 * Stats & achievement service.
 *
 * Fetches player stats and achievement data from the lobby server.
 * Falls back to empty data if the server is not reachable (dev mode).
 */

const API_BASE = (import.meta.env.VITE_LAUNCH_API_URL as string | undefined)
  ?? (import.meta.env.VITE_WS_URL as string | undefined)?.replace(/^ws/, 'http')
  ?? 'http://localhost:8080';

// ─── Types (mirroring server-side definitions) ────────────────────────────

export interface AchievementDef {
  id: string;
  name: string;
  description: string;
  icon: string;
  category: 'sessions' | 'social' | 'systems' | 'games' | 'streaks';
  threshold?: number;
  requiredGameId?: string;
  requiredSystem?: string;
}

export interface PlayerAchievement {
  achievementId: string;
  unlockedAt: string;
}

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
  avgSessionSecs: number;
  longestSessionSecs: number;
  uniquePlaymates: number;
  bySystem: SystemStat[];
  topGames: GameStat[];
  uniqueGamesCount: number;
  activeDays: number;
  firstSessionAt: string | null;
  lastSessionAt: string | null;
}

export interface LeaderboardEntry {
  rank: number;
  playerId: string;
  displayName: string;
  value: number;
}

export type LeaderboardMetric = 'sessions' | 'playtime' | 'games' | 'systems';

/** Summary of a tournament a player participated in. */
export interface PlayerTournamentEntry {
  id: string;
  name: string;
  gameTitle: string;
  status: string;
  winner: string | null;
  playerCount: number;
  createdAt: string;
}

// ─── API calls ────────────────────────────────────────────────────────────

/** Fetch all achievement definitions. */
export async function fetchAchievementDefs(): Promise<AchievementDef[]> {
  try {
    const res = await fetch(`${API_BASE}/api/achievements`);
    if (!res.ok) return [];
    return res.json() as Promise<AchievementDef[]>;
  } catch {
    return [];
  }
}

/** Fetch earned achievements for a player by display name (used as playerId fallback). */
export async function fetchPlayerAchievements(
  displayName: string
): Promise<PlayerAchievement[]> {
  try {
    const res = await fetch(
      `${API_BASE}/api/achievements/${encodeURIComponent(displayName)}`
    );
    if (!res.ok) return [];
    const data = await res.json() as { earned: PlayerAchievement[] };
    return data.earned ?? [];
  } catch {
    return [];
  }
}

/** Fetch aggregate stats for a player by display name. */
export async function fetchPlayerStats(
  displayName: string
): Promise<PlayerStats | null> {
  try {
    const res = await fetch(
      `${API_BASE}/api/stats/${encodeURIComponent(displayName)}`
    );
    if (!res.ok) return null;
    return res.json() as Promise<PlayerStats>;
  } catch {
    return null;
  }
}

/** Fetch the global leaderboard. */
export async function fetchLeaderboard(
  metric: LeaderboardMetric = 'sessions',
  limit = 10
): Promise<LeaderboardEntry[]> {
  try {
    const res = await fetch(
      `${API_BASE}/api/leaderboard?metric=${metric}&limit=${limit}`
    );
    if (!res.ok) return [];
    return res.json() as Promise<LeaderboardEntry[]>;
  } catch {
    return [];
  }
}

/** Fetch tournament history for a player by display name. */
export async function fetchPlayerTournaments(
  displayName: string
): Promise<PlayerTournamentEntry[]> {
  try {
    const res = await fetch(
      `${API_BASE}/api/tournaments/player/${encodeURIComponent(displayName)}`
    );
    if (!res.ok) return [];
    return res.json() as Promise<PlayerTournamentEntry[]>;
  } catch {
    return [];
  }
}

// ─── Formatting helpers ───────────────────────────────────────────────────

/** Format seconds as "Xh Ym" or "Ym" or "Xs". */
export function formatDuration(secs: number): string {
  if (secs <= 0) return '0m';
  const h = Math.floor(secs / 3600);
  const m = Math.floor((secs % 3600) / 60);
  const s = secs % 60;
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m}m`;
  return `${s}s`;
}
