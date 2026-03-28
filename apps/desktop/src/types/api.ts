/** Normalized game record returned by the API client. */
export interface ApiGame {
  id: string;
  title: string;
  /** Uppercase system abbreviation, e.g. "NES", "N64". */
  system: string;
  systemColor: string;
  maxPlayers: number;
  tags: string[];
  badges: string[];
  description: string;
  coverEmoji: string;
  supportsPublicLobby: boolean;
  supportsPrivateLobby: boolean;
  onlineRecommended: boolean;
  compatibilityNotes: string[];
  /** DSiWare title — requires DSi mode (DSi BIOS files) in melonDS */
  isDsiWare?: boolean;
}

/** Query parameters for filtering the game list. */
export interface ApiGameQuery {
  system?: string;
  tag?: string;
  maxPlayers?: number;
  search?: string;
}

/**
 * Data-source mode:
 * - `mock`    – always use bundled mock data (no network required)
 * - `backend` – always use the remote HTTP backend (fails if unreachable)
 * - `hybrid`  – try backend first, fall back to mock on error
 */
export type ApiMode = 'mock' | 'backend' | 'hybrid';

/** Generic loading-state envelope used by React hooks. */
export interface FetchState<T> {
  data: T;
  loading: boolean;
  error: string | null;
}
