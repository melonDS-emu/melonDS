import type { SupportedSystem } from './systems';

export type LobbyStatus = 'waiting' | 'ready' | 'in-game' | 'closed';
export type PlayerReadyState = 'not-ready' | 'ready' | 'loading';

export interface Lobby {
  id: string;
  name: string;
  hostId: string;
  gameId: string;
  gameTitle: string;
  system: SupportedSystem;
  isPublic: boolean;
  roomCode: string;
  maxPlayers: number;
  players: LobbyPlayer[];
  status: LobbyStatus;
  createdAt: string;
  sessionTemplateId?: string;
  tags: string[];
}

export interface LobbyPlayer {
  id: string;
  displayName: string;
  avatarUrl?: string;
  readyState: PlayerReadyState;
  slot: number;
  isHost: boolean;
  connectionQuality: ConnectionQuality;
  joinedAt: string;
}

export type ConnectionQuality = 'excellent' | 'good' | 'fair' | 'poor' | 'unknown';

export interface CreateLobbyRequest {
  name: string;
  gameId: string;
  isPublic: boolean;
  maxPlayers: number;
  sessionTemplateId?: string;
}

export interface JoinLobbyRequest {
  lobbyId?: string;
  roomCode?: string;
}

export interface LobbyEvent {
  type: LobbyEventType;
  lobbyId: string;
  payload: unknown;
  timestamp: string;
}

export type LobbyEventType =
  | 'player-joined'
  | 'player-left'
  | 'player-ready'
  | 'player-not-ready'
  | 'game-starting'
  | 'game-started'
  | 'game-ended'
  | 'host-changed'
  | 'lobby-closed'
  | 'chat-message';
