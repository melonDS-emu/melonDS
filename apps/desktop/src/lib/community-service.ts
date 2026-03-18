/**
 * Community Service — Phase 15
 *
 * Typed fetch helpers for the Community Hub REST API:
 *   - Game ratings & reviews
 *   - Community activity feed
 *   - ELO rankings
 */

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

// ---------------------------------------------------------------------------
// Shared types (mirrors lobby-server types)
// ---------------------------------------------------------------------------

export interface GameReview {
  id: string;
  gameId: string;
  gameTitle: string;
  playerId: string;
  playerName: string;
  rating: number;
  text?: string;
  createdAt: string;
  updatedAt: string;
}

export interface GameRatingSummary {
  gameId: string;
  gameTitle: string;
  averageRating: number;
  reviewCount: number;
  distribution: Record<string, number>;
}

export type ActivityEventType =
  | 'session-started'
  | 'session-ended'
  | 'achievement-unlocked'
  | 'tournament-created'
  | 'tournament-won'
  | 'review-submitted'
  | 'friend-added';

export interface ActivityEvent {
  id: string;
  type: ActivityEventType;
  playerName: string;
  description: string;
  subject?: string;
  createdAt: string;
  meta?: Record<string, string | number>;
}

export type RankTier = 'Bronze' | 'Silver' | 'Gold' | 'Platinum' | 'Diamond';

export interface PlayerRank {
  playerId: string;
  playerName: string;
  elo: number;
  wins: number;
  losses: number;
  draws: number;
  gamesPlayed: number;
  tier: RankTier;
  updatedAt: string;
}

export interface GameRank {
  gameId: string;
  gameTitle: string;
  playerId: string;
  playerName: string;
  elo: number;
  wins: number;
  losses: number;
  gamesPlayed: number;
  updatedAt: string;
}

// ---------------------------------------------------------------------------
// Reviews
// ---------------------------------------------------------------------------

export async function fetchGameReviews(gameId: string): Promise<GameReview[]> {
  const res = await fetch(`${API}/api/reviews/${encodeURIComponent(gameId)}`);
  if (!res.ok) throw new Error('Failed to fetch reviews');
  const data = (await res.json()) as { reviews: GameReview[] };
  return data.reviews;
}

export async function fetchGameRatingSummary(gameId: string): Promise<GameRatingSummary | null> {
  const res = await fetch(`${API}/api/reviews/${encodeURIComponent(gameId)}/summary`);
  if (res.status === 404) return null;
  if (!res.ok) throw new Error('Failed to fetch rating summary');
  return res.json() as Promise<GameRatingSummary>;
}

export async function fetchTopRatedGames(limit = 10): Promise<GameRatingSummary[]> {
  const res = await fetch(`${API}/api/reviews/top?limit=${limit}`);
  if (!res.ok) throw new Error('Failed to fetch top-rated games');
  const data = (await res.json()) as { games: GameRatingSummary[] };
  return data.games;
}

export async function submitReview(
  gameId: string,
  gameTitle: string,
  playerId: string,
  playerName: string,
  rating: number,
  text?: string,
): Promise<GameReview> {
  const res = await fetch(`${API}/api/reviews/${encodeURIComponent(gameId)}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ gameTitle, playerId, playerName, rating, text }),
  });
  if (!res.ok) throw new Error('Failed to submit review');
  const data = (await res.json()) as { review: GameReview };
  return data.review;
}

// ---------------------------------------------------------------------------
// Activity Feed
// ---------------------------------------------------------------------------

export async function fetchActivity(
  opts: { limit?: number; type?: ActivityEventType; player?: string } = {},
): Promise<{ events: ActivityEvent[]; total: number }> {
  const params = new URLSearchParams();
  if (opts.limit) params.set('limit', String(opts.limit));
  if (opts.type) params.set('type', opts.type);
  if (opts.player) params.set('player', opts.player);
  const res = await fetch(`${API}/api/activity?${params}`);
  if (!res.ok) throw new Error('Failed to fetch activity');
  return res.json() as Promise<{ events: ActivityEvent[]; total: number }>;
}

// ---------------------------------------------------------------------------
// Rankings
// ---------------------------------------------------------------------------

export async function fetchGlobalRankings(limit = 20): Promise<PlayerRank[]> {
  const res = await fetch(`${API}/api/rankings?limit=${limit}`);
  if (!res.ok) throw new Error('Failed to fetch global rankings');
  const data = (await res.json()) as { rankings: PlayerRank[] };
  return data.rankings;
}

export async function fetchGameRankings(gameId: string, limit = 20): Promise<GameRank[]> {
  const res = await fetch(`${API}/api/rankings/${encodeURIComponent(gameId)}?limit=${limit}`);
  if (!res.ok) throw new Error('Failed to fetch game rankings');
  const data = (await res.json()) as { gameId: string; rankings: GameRank[] };
  return data.rankings;
}

export async function fetchPlayerRank(playerId: string): Promise<PlayerRank | null> {
  const res = await fetch(`${API}/api/rankings/player/${encodeURIComponent(playerId)}`);
  if (res.status === 404) return null;
  if (!res.ok) throw new Error('Failed to fetch player rank');
  return res.json() as Promise<PlayerRank>;
}

// ---------------------------------------------------------------------------
// Rank tier helpers
// ---------------------------------------------------------------------------

export const TIER_COLORS: Record<RankTier, string> = {
  Bronze:   '#cd7f32',
  Silver:   '#c0c0c0',
  Gold:     '#ffd700',
  Platinum: '#00d4ff',
  Diamond:  '#b9f2ff',
};

export const TIER_ICONS: Record<RankTier, string> = {
  Bronze:   '🥉',
  Silver:   '🥈',
  Gold:     '🥇',
  Platinum: '💎',
  Diamond:  '💠',
};

/** Return the rank title that best represents a player's achievements.
 *  Used in Profile page to display a cosmetic title. */
export function playerTitle(tier: RankTier, gamesPlayed: number): string {
  if (gamesPlayed === 0) return 'Newcomer';
  switch (tier) {
    case 'Diamond':  return 'Diamond Legend';
    case 'Platinum': return 'Platinum Champion';
    case 'Gold':     return 'Gold Veteran';
    case 'Silver':   return 'Silver Contender';
    case 'Bronze':   return 'Bronze Rookie';
  }
}
