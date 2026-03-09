import type { SupportedSystem } from './systems';

export interface Game {
  id: string;
  title: string;
  system: SupportedSystem;
  romPath: string;
  coverArt?: string;
  multiplayerProfileId?: string;
  favorite: boolean;
  lastPlayedAt?: string;
  tags: GameTag[];
  region?: 'us' | 'eu' | 'jp' | 'other';
}

export type GameTag =
  | '2P'
  | '4P'
  | 'Link'
  | 'Trade'
  | 'Battle'
  | 'Co-op'
  | 'Party'
  | 'Versus'
  | 'Single Player';

export interface GameMetadata {
  id: string;
  gameId: string;
  title: string;
  system: SupportedSystem;
  region?: string;
  genres: string[];
  description: string;
  maxPlayers: number;
  multiplayerCategory: MultiplayerCategory;
  tags: GameTag[];
  recommendedLayout?: string;
  compatibilityNotes: string[];
  bestOnlineMode?: string;
  coverArtUrl?: string;
  /** NDS-specific fields */
  requiresTouch?: boolean;
  touchOptional?: boolean;
  screenLayoutRecommendation?: 'stacked' | 'side-by-side' | 'top-focus' | 'bottom-focus';
  multiplayerModeNotes?: string;
}

export type MultiplayerCategory =
  | 'local-coop'
  | 'local-versus'
  | 'online-coop'
  | 'online-versus'
  | 'link-cable'
  | 'trade'
  | 'battle'
  | 'party'
  | 'single-player';
