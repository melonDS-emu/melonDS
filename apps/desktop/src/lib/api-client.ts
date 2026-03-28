/**
 * Typed API client for the RetroOasis game catalog.
 *
 * Supports three data-source modes controlled by the `VITE_API_MODE`
 * environment variable (default: `"mock"`):
 *
 *   mock    – bundled mock data, no network required
 *   backend – remote HTTP backend at VITE_BACKEND_URL
 *   hybrid  – try backend first, fall back to mock on error
 *
 * Backend endpoints (relative to VITE_BACKEND_URL):
 *   GET /api/games           – list / filter games
 *   GET /api/games/:id       – single game
 *   GET /api/games/search    – text search (?q=)
 *   GET /api/systems         – list supported systems
 */

import type { ApiGame, ApiGameQuery, ApiMode } from '../types/api';
import { MOCK_GAMES } from '../data/mock-games';

// ---------------------------------------------------------------------------
// Constants & helpers
// ---------------------------------------------------------------------------

const SYSTEM_COLORS: Record<string, string> = {
  NES: '#E60012',
  SNES: '#7B5EA7',
  GB: '#8B956D',
  GBC: '#6A5ACD',
  GBA: '#4B0082',
  N64: '#009900',
  NDS: '#A0A0A0',
  GC: '#6A0DAD',
  '3DS': '#CC0000',
};

const COVER_EMOJI_FALLBACK: Record<string, string> = {
  NES: '🎮',
  SNES: '🎮',
  GB: '🎮',
  GBC: '🎮',
  GBA: '🎮',
  N64: '🎮',
  NDS: '📱',
  GC: '🎮',
  '3DS': '🎮',
};

/** Normalise a system string to the uppercase abbreviation used by the UI. */
function normalizeSystem(raw: string): string {
  return raw.toUpperCase();
}

/** Convert a MockGame (from mock-games.ts) into the canonical ApiGame shape. */
function normalizeMockGame(g: (typeof MOCK_GAMES)[number]): ApiGame {
  const sys = normalizeSystem(g.system);
  return {
    id: g.id,
    title: g.title,
    system: sys,
    systemColor: g.systemColor ?? SYSTEM_COLORS[sys] ?? '#7c5cbf',
    maxPlayers: g.maxPlayers,
    tags: g.tags,
    badges: g.badges,
    description: g.description,
    coverEmoji: g.coverEmoji ?? COVER_EMOJI_FALLBACK[sys] ?? '🎮',
    supportsPublicLobby: true,
    supportsPrivateLobby: true,
    onlineRecommended: g.badges.includes('Great Online'),
    compatibilityNotes: [],
    isDsiWare: g.isDsiWare,
  };
}

/**
 * Normalise a raw server game response into the canonical `ApiGame` shape.
 *
 * The lobby-server's `/api/games` endpoint returns `SeedGame` records that
 * use lowercase system keys and lack `coverEmoji`/`systemColor`. This function
 * fills in the missing fields so the client always receives a complete
 * `ApiGame` regardless of which data source is active.
 */
function normalizeServerGame(g: Record<string, unknown>): ApiGame {
  const sys = normalizeSystem((g.system as string | undefined) ?? '');
  return {
    id: (g.id as string) ?? '',
    title: (g.title as string) ?? '',
    system: sys,
    systemColor: (g.systemColor as string | undefined) ?? SYSTEM_COLORS[sys] ?? '#7c5cbf',
    maxPlayers: (g.maxPlayers as number | undefined) ?? 2,
    tags: (g.tags as string[] | undefined) ?? [],
    badges: (g.badges as string[] | undefined) ?? [],
    description: (g.description as string | undefined) ?? '',
    coverEmoji: (g.coverEmoji as string | undefined) ?? COVER_EMOJI_FALLBACK[sys] ?? '🎮',
    supportsPublicLobby: (g.supportsPublicLobby as boolean | undefined) ?? true,
    supportsPrivateLobby: (g.supportsPrivateLobby as boolean | undefined) ?? true,
    onlineRecommended: (g.onlineRecommended as boolean | undefined) ?? false,
    compatibilityNotes: (g.compatibilityNotes as string[] | undefined) ?? [],
    isDsiWare: (g.isDsiWare as boolean | undefined),
  };
}

// ---------------------------------------------------------------------------
// Retry helper
// ---------------------------------------------------------------------------

// Delay schedule for retries: 500 ms → 1 s → 2 s (3 attempts total after the first failure).
const RETRY_DELAYS_MS = [500, 1000, 2000];

async function fetchWithRetry(
  url: string,
  retries = RETRY_DELAYS_MS.length,
): Promise<Response> {
  let lastError: unknown;
  for (let attempt = 0; attempt <= retries; attempt++) {
    try {
      const res = await fetch(url);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return res;
    } catch (err) {
      lastError = err;
      if (attempt < retries) {
        await new Promise((r) => setTimeout(r, RETRY_DELAYS_MS[attempt]));
      }
    }
  }
  throw lastError;
}

// ---------------------------------------------------------------------------
// GameApiClient
// ---------------------------------------------------------------------------

export class GameApiClient {
  private readonly mode: ApiMode;
  private readonly backendUrl: string;

  constructor(mode?: ApiMode, backendUrl?: string) {
    this.mode =
      (mode as ApiMode | undefined) ??
      (import.meta.env?.VITE_API_MODE as ApiMode | undefined) ??
      'hybrid';
    this.backendUrl =
      backendUrl ??
      (import.meta.env?.VITE_BACKEND_URL as string | undefined) ??
      '';
  }

  // -------------------------------------------------------------------------
  // Public API methods
  // -------------------------------------------------------------------------

  /** Return all games, optionally filtered by system, tag, maxPlayers, or free-text search. */
  async getGames(query?: ApiGameQuery): Promise<ApiGame[]> {
    return this.resolve(() => this.fetchGames(query), () => this.mockGames(query));
  }

  /** Return a single game by ID, or `null` if not found. */
  async getGameById(id: string): Promise<ApiGame | null> {
    return this.resolve(() => this.fetchGameById(id), () => this.mockGameById(id));
  }

  /** Full-text search across titles and descriptions. */
  async searchGames(text: string): Promise<ApiGame[]> {
    return this.resolve(
      () => this.fetchSearchGames(text),
      () => this.mockSearchGames(text),
    );
  }

  /** Return the list of supported system abbreviations (e.g. ["NES","SNES",…]). */
  async getSystems(): Promise<string[]> {
    return this.resolve(() => this.fetchSystems(), () => this.mockSystems());
  }

  // -------------------------------------------------------------------------
  // Mode dispatcher
  // -------------------------------------------------------------------------

  private async resolve<T>(
    backendFn: () => Promise<T>,
    mockFn: () => T,
  ): Promise<T> {
    if (this.mode === 'mock') return mockFn();
    if (this.mode === 'backend') return backendFn();

    // hybrid: try backend, fall back to mock
    try {
      return await backendFn();
    } catch {
      return mockFn();
    }
  }

  // -------------------------------------------------------------------------
  // Mock implementations
  // -------------------------------------------------------------------------

  private mockGames(query?: ApiGameQuery): ApiGame[] {
    let games = MOCK_GAMES.map(normalizeMockGame);

    if (query?.system) {
      const sys = normalizeSystem(query.system);
      games = games.filter((g) => g.system === sys);
    }
    if (query?.tag) {
      games = games.filter((g) => g.tags.includes(query.tag!));
    }
    if (query?.maxPlayers !== undefined) {
      games = games.filter((g) => g.maxPlayers >= query.maxPlayers!);
    }
    if (query?.search) {
      const q = query.search.toLowerCase();
      games = games.filter(
        (g) =>
          g.title.toLowerCase().includes(q) ||
          g.description.toLowerCase().includes(q),
      );
    }
    return games;
  }

  private mockGameById(id: string): ApiGame | null {
    const raw = MOCK_GAMES.find((g) => g.id === id);
    return raw ? normalizeMockGame(raw) : null;
  }

  private mockSearchGames(text: string): ApiGame[] {
    return this.mockGames({ search: text });
  }

  private mockSystems(): string[] {
    const systems = new Set(MOCK_GAMES.map((g) => normalizeSystem(g.system)));
    return Array.from(systems).sort();
  }

  // -------------------------------------------------------------------------
  // Backend HTTP implementations
  // -------------------------------------------------------------------------

  private async fetchGames(query?: ApiGameQuery): Promise<ApiGame[]> {
    const params = new URLSearchParams();
    if (query?.system) params.set('system', query.system);
    if (query?.tag) params.set('tag', query.tag);
    if (query?.maxPlayers !== undefined)
      params.set('maxPlayers', String(query.maxPlayers));
    if (query?.search) params.set('search', query.search);

    const qs = params.toString();
    const res = await fetchWithRetry(
      `${this.backendUrl}/api/games${qs ? `?${qs}` : ''}`,
    );
    const raw = (await res.json()) as Record<string, unknown>[];
    return raw.map(normalizeServerGame);
  }

  private async fetchGameById(id: string): Promise<ApiGame | null> {
    try {
      const res = await fetchWithRetry(
        `${this.backendUrl}/api/games/${encodeURIComponent(id)}`,
      );
      return normalizeServerGame((await res.json()) as Record<string, unknown>);
    } catch {
      return null;
    }
  }

  private async fetchSearchGames(text: string): Promise<ApiGame[]> {
    const res = await fetchWithRetry(
      `${this.backendUrl}/api/games/search?q=${encodeURIComponent(text)}`,
    );
    const raw = (await res.json()) as Record<string, unknown>[];
    return raw.map(normalizeServerGame);
  }

  private async fetchSystems(): Promise<string[]> {
    const res = await fetchWithRetry(`${this.backendUrl}/api/systems`);
    return (await res.json()) as string[];
  }
}

/** Singleton client — configured from Vite env vars at module load time. */
export const apiClient = new GameApiClient();
