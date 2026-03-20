/**
 * Retro Achievement service — fetches per-game retro achievement data from the
 * lobby server. Falls back to empty data if the server is not reachable.
 */

const API_BASE =
  (import.meta.env.VITE_LAUNCH_API_URL as string | undefined) ??
  (import.meta.env.VITE_WS_URL as string | undefined)?.replace(/^ws/, 'http') ??
  'http://localhost:8080';

// ─── Types ────────────────────────────────────────────────────────────────────

export interface RetroGameAchievementDef {
  id: string;
  gameId: string;
  title: string;
  description: string;
  points: number;
  badge: string;
}

export interface EarnedRetroAchievement {
  achievementId: string;
  gameId: string;
  earnedAt: string;
  sessionId?: string;
}

export interface RetroPlayerProgress {
  playerId: string;
  earned: EarnedRetroAchievement[];
  totalPoints: number;
}

export interface RetroGameProgress {
  playerId: string;
  gameId: string;
  earned: EarnedRetroAchievement[];
  totalEarned: number;
  totalAvailable: number;
}

export interface RetroGameSummary {
  gameId: string;
  totalAchievements: number;
  totalPoints: number;
}

export interface RetroLeaderboardEntry {
  playerId: string;
  totalPoints: number;
  earnedCount: number;
}

// ─── API calls ────────────────────────────────────────────────────────────────

/** Fetch all retro achievement definitions. */
export async function fetchRetroAchievementDefs(): Promise<RetroGameAchievementDef[]> {
  try {
    const res = await fetch(`${API_BASE}/api/retro-achievements`);
    if (!res.ok) return [];
    return res.json() as Promise<RetroGameAchievementDef[]>;
  } catch {
    return [];
  }
}

/** Fetch game summaries (list of games with achievement counts). */
export async function fetchRetroGameSummaries(): Promise<RetroGameSummary[]> {
  try {
    const res = await fetch(`${API_BASE}/api/retro-achievements/games`);
    if (!res.ok) return [];
    return res.json() as Promise<RetroGameSummary[]>;
  } catch {
    return [];
  }
}

/** Fetch all retro achievement definitions for a single game. */
export async function fetchRetroGameDefs(
  gameId: string
): Promise<RetroGameAchievementDef[]> {
  try {
    const res = await fetch(
      `${API_BASE}/api/retro-achievements/games/${encodeURIComponent(gameId)}`
    );
    if (!res.ok) return [];
    return res.json() as Promise<RetroGameAchievementDef[]>;
  } catch {
    return [];
  }
}

/** Fetch all earned retro achievements for a player (across all games). */
export async function fetchRetroPlayerProgress(
  playerId: string
): Promise<RetroPlayerProgress | null> {
  try {
    const res = await fetch(
      `${API_BASE}/api/retro-achievements/player/${encodeURIComponent(playerId)}`
    );
    if (!res.ok) return null;
    return res.json() as Promise<RetroPlayerProgress>;
  } catch {
    return null;
  }
}

/** Fetch player progress for a specific game. */
export async function fetchRetroGameProgress(
  playerId: string,
  gameId: string
): Promise<RetroGameProgress | null> {
  try {
    const res = await fetch(
      `${API_BASE}/api/retro-achievements/player/${encodeURIComponent(playerId)}/game/${encodeURIComponent(gameId)}`
    );
    if (!res.ok) return null;
    return res.json() as Promise<RetroGameProgress>;
  } catch {
    return null;
  }
}

/** Unlock a retro achievement for a player. */
export async function unlockRetroAchievement(
  playerId: string,
  gameId: string,
  achievementId: string,
  sessionId?: string
): Promise<RetroGameAchievementDef | null> {
  try {
    const res = await fetch(
      `${API_BASE}/api/retro-achievements/player/${encodeURIComponent(playerId)}/game/${encodeURIComponent(gameId)}/unlock`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ achievementId, ...(sessionId ? { sessionId } : {}) }),
      }
    );
    if (!res.ok) return null;
    const data = await res.json() as { unlocked: RetroGameAchievementDef };
    return data.unlocked ?? null;
  } catch {
    return null;
  }
}

/** Fetch the retro achievement leaderboard (top players by total points). */
export async function fetchRetroLeaderboard(limit = 10): Promise<RetroLeaderboardEntry[]> {
  try {
    const res = await fetch(`${API_BASE}/api/retro-achievements/leaderboard?limit=${limit}`);
    if (!res.ok) return [];
    return res.json() as Promise<RetroLeaderboardEntry[]>;
  } catch {
    return [];
  }
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

/** Human-readable game name from a catalog game ID. */
export function gameIdToTitle(gameId: string): string {
  const TITLES: Record<string, string> = {
    'n64-mario-kart-64': 'Mario Kart 64',
    'n64-super-smash-bros': 'Super Smash Bros.',
    'nds-mario-kart-ds': 'Mario Kart DS',
    'nds-pokemon-diamond': 'Pokémon Diamond',
    'gba-pokemon-emerald': 'Pokémon Emerald',
    'gb-tetris': 'Tetris',
    'nes-contra': 'Contra',
    'snes-super-bomberman': 'Super Bomberman',
    'genesis-sonic-the-hedgehog-2': 'Sonic the Hedgehog 2',
    'psx-crash-bandicoot': 'Crash Bandicoot',
    // Phase 24 additions
    'gc-mario-kart-double-dash': 'Mario Kart: Double Dash!!',
    'wii-mario-kart-wii': 'Mario Kart Wii',
    '3ds-mario-kart-7': 'Mario Kart 7',
    'dc-sonic-adventure-2': 'Sonic Adventure 2',
    'ps2-gta-san-andreas': 'GTA: San Andreas',
    // Phase 25 additions
    'gbc-pokemon-crystal': 'Pokémon Crystal',
    'psp-monster-hunter-fu': 'Monster Hunter Freedom Unite',
    'psx-castlevania-sotn': 'Castlevania: Symphony of the Night',
    'snes-secret-of-mana': 'Secret of Mana',
    'psx-metal-gear-solid': 'Metal Gear Solid',
  };
  return TITLES[gameId] ?? gameId;
}

/** System label from a catalog game ID. */
export function gameIdToSystem(gameId: string): string {
  if (gameId.startsWith('n64-')) return 'N64';
  if (gameId.startsWith('nds-')) return 'NDS';
  if (gameId.startsWith('gba-')) return 'GBA';
  if (gameId.startsWith('gb-')) return 'GB';
  if (gameId.startsWith('nes-')) return 'NES';
  if (gameId.startsWith('snes-')) return 'SNES';
  if (gameId.startsWith('genesis-')) return 'Genesis';
  if (gameId.startsWith('gc-')) return 'GameCube';
  if (gameId.startsWith('wii-')) return 'Wii';
  if (gameId.startsWith('3ds-')) return '3DS';
  if (gameId.startsWith('dc-')) return 'Dreamcast';
  if (gameId.startsWith('psx-')) return 'PSX';
  if (gameId.startsWith('ps2-')) return 'PS2';
  if (gameId.startsWith('psp-')) return 'PSP';
  return gameId.split('-')[0].toUpperCase();
}
