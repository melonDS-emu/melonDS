import type { SupportedSystem } from './systems';

export type CompatibilityBadge =
  | 'Great Online'
  | 'Good Local'
  | 'Best with Friends'
  | 'Link Mode'
  | 'Experimental'
  | 'Host Only Recommended'
  | 'Touch Heavy'
  | 'Party Favorite';

export interface CompatibilityNote {
  id: string;
  gameId: string;
  system: SupportedSystem;
  badges: CompatibilityBadge[];
  onlineRecommended: boolean;
  localOnlySafer: boolean;
  linkTradeSupported: boolean;
  knownIssues: string[];
  recommendedBackendId: string;
  bestSessionMode: string;
  notes: string;
  lastVerified?: string;
}

export interface MultiplayerProfile {
  id: string;
  gameId: string;
  maxPlayers: number;
  mode: MultiplayerMode;
  recommendedBackend: string;
  notes: string[];
  supportsPublicLobby: boolean;
  supportsPrivateLobby: boolean;
  badges: CompatibilityBadge[];
}

export type MultiplayerMode =
  | 'local'
  | 'online'
  | 'hybrid'
  | 'link'
  | 'trade'
  | 'battle';
