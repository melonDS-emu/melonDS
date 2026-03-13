/**
 * Events Service — Phase 13
 *
 * Typed fetch helpers for the Seasonal Events and Featured Games REST API.
 */

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

// ---------------------------------------------------------------------------
// Shared types (mirrors apps/lobby-server/src/seasonal-events.ts)
// ---------------------------------------------------------------------------

export interface SeasonalEvent {
  id: string;
  name: string;
  description: string;
  emoji: string;
  theme: string;
  startDate: string;
  endDate: string;
  featuredGameIds: string[];
  xpMultiplier?: number;
}

export interface FeaturedGame {
  gameId: string;
  gameTitle: string;
  system: string;
  reason: string;
  emoji: string;
}

// ---------------------------------------------------------------------------
// Fetch helpers
// ---------------------------------------------------------------------------

/** Fetch all seasonal events plus the next upcoming event. */
export async function fetchAllEvents(): Promise<{
  events: SeasonalEvent[];
  nextEvent: SeasonalEvent | null;
}> {
  const res = await fetch(`${API}/api/events`);
  if (!res.ok) throw new Error('Failed to fetch events');
  return res.json() as Promise<{ events: SeasonalEvent[]; nextEvent: SeasonalEvent | null }>;
}

/** Fetch currently active events and the next upcoming event. */
export async function fetchCurrentEvents(): Promise<{
  active: SeasonalEvent[];
  nextEvent: SeasonalEvent | null;
}> {
  const res = await fetch(`${API}/api/events/current`);
  if (!res.ok) throw new Error('Failed to fetch current events');
  return res.json() as Promise<{ active: SeasonalEvent[]; nextEvent: SeasonalEvent | null }>;
}

/** Fetch the featured games for the current week. */
export async function fetchFeaturedGames(): Promise<FeaturedGame[]> {
  const res = await fetch(`${API}/api/games/featured`);
  if (!res.ok) throw new Error('Failed to fetch featured games');
  const data = (await res.json()) as { featured: FeaturedGame[] };
  return data.featured;
}

// ---------------------------------------------------------------------------
// Theme helpers
// ---------------------------------------------------------------------------

/** Map a theme id to its CSS gradient background string for UI cards. */
export function themeGradient(theme: string | undefined): string {
  switch (theme) {
    case 'spring':  return 'linear-gradient(135deg, #1a1a0a 0%, #0f1a08 60%, #0a1200 100%)';
    case 'summer':  return 'linear-gradient(135deg, #1a0e00 0%, #1a1200 60%, #120800 100%)';
    case 'fall':    return 'linear-gradient(135deg, #1a0800 0%, #1a0f00 60%, #0f0a00 100%)';
    case 'winter':  return 'linear-gradient(135deg, #00101a 0%, #000f1a 60%, #00081a 100%)';
    case 'neon':    return 'linear-gradient(135deg, #0a001a 0%, #10001a 60%, #000d1a 100%)';
    case 'classic': return 'linear-gradient(135deg, #1a0008 0%, #0f0f1b 60%, #001a0a 100%)';
    default:        return 'linear-gradient(135deg, #1a0008 0%, #0f0f1b 60%, #001a0a 100%)';
  }
}

/** Map a theme id to its accent colour for UI highlights. */
export function themeAccent(theme: string | undefined): string {
  switch (theme) {
    case 'spring':  return '#86efac'; // green-300
    case 'summer':  return '#fbbf24'; // amber-400
    case 'fall':    return '#fb923c'; // orange-400
    case 'winter':  return '#7dd3fc'; // sky-300
    case 'neon':    return '#e879f9'; // fuchsia-400
    case 'classic': return '#e60012'; // Nintendo Red
    default:        return '#e60012';
  }
}

/** Human-readable label for a theme id. */
export function themeLabel(theme: string): string {
  const labels: Record<string, string> = {
    spring:  '🌸 Spring',
    summer:  '☀️ Summer',
    fall:    '🍂 Fall',
    winter:  '❄️ Winter',
    neon:    '💜 Neon',
    classic: '🎮 Classic',
  };
  return labels[theme] ?? theme;
}

/** All available room themes (id → label). */
export const ROOM_THEMES: { id: string; label: string }[] = [
  { id: 'classic', label: '🎮 Classic' },
  { id: 'spring',  label: '🌸 Spring'  },
  { id: 'summer',  label: '☀️ Summer'  },
  { id: 'fall',    label: '🍂 Fall'    },
  { id: 'winter',  label: '❄️ Winter'  },
  { id: 'neon',    label: '💜 Neon'    },
];
