export type LobbyStatus = 'waiting' | 'ready' | 'in-game' | 'closed';
export type PlayerReadyState = 'not-ready' | 'ready';
export type PlayerRole = 'player' | 'spectator';
export type ConnectionQuality = 'excellent' | 'good' | 'fair' | 'poor' | 'unknown';

export interface Room {
  id: string;
  name: string;
  hostId: string;
  gameId: string;
  gameTitle: string;
  system: string;
  isPublic: boolean;
  roomCode: string;
  maxPlayers: number;
  players: RoomPlayer[];
  spectators: RoomSpectator[];
  status: LobbyStatus;
  createdAt: string;
  /** Netplay relay port assigned when game starts, if relay mode is used. */
  relayPort?: number;
  /** Optional visual theme chosen by the host (e.g. 'spring', 'summer', 'neon', 'classic'). */
  theme?: string;
}

export interface RoomPlayer {
  id: string;
  displayName: string;
  readyState: PlayerReadyState;
  slot: number;
  isHost: boolean;
  joinedAt: string;
  connectionQuality: ConnectionQuality;
  latencyMs?: number;
}

export interface RoomSpectator {
  id: string;
  displayName: string;
  joinedAt: string;
}

/** Messages from client to server */
export type ClientMessage =
  | { type: 'create-room'; payload: CreateRoomPayload }
  | { type: 'join-room'; payload: JoinRoomPayload }
  | { type: 'join-as-spectator'; payload: JoinRoomPayload }
  | { type: 'leave-room'; payload: { roomId: string } }
  | { type: 'toggle-ready'; payload: { roomId: string } }
  | { type: 'list-rooms' }
  | { type: 'start-game'; payload: { roomId: string } }
  | { type: 'kick-player'; payload: { roomId: string; targetPlayerId: string; ownerToken: string } }
  | { type: 'chat'; payload: { roomId: string; content: string } }
  | { type: 'ping'; payload: { sentAt: number } }
  /** Phase 8: player identity registration */
  | { type: 'register-identity'; payload: { identityToken: string; displayName: string } }
  /** Phase 8: friend request actions */
  | { type: 'friend-request'; payload: { toPlayerId: string } }
  | { type: 'friend-request-accept'; payload: { requestId: string } }
  | { type: 'friend-request-decline'; payload: { requestId: string } }
  | { type: 'friend-remove'; payload: { friendId: string } }
  /** Phase 8: matchmaking */
  | { type: 'matchmaking-join'; payload: MatchmakingJoinPayload }
  | { type: 'matchmaking-leave' };

export interface CreateRoomPayload {
  name: string;
  gameId: string;
  gameTitle: string;
  system: string;
  isPublic: boolean;
  maxPlayers: number;
  displayName: string;
  /** Optional visual theme for the room (e.g. 'spring', 'neon'). */
  theme?: string;
}

export interface JoinRoomPayload {
  roomId?: string;
  roomCode?: string;
  displayName: string;
}

/** Phase 8: matchmaking join payload. */
export interface MatchmakingJoinPayload {
  gameId: string;
  gameTitle: string;
  system: string;
  maxPlayers: number;
  displayName: string;
}

/** Lightweight presence record broadcast to all connected clients. */
export interface PresencePlayer {
  playerId: string;
  displayName: string;
  /** Room code if the player is currently in a lobby, undefined if just online. */
  roomCode?: string;
  /** Game title if in-game. */
  gameTitle?: string;
  status: 'online' | 'in-lobby' | 'in-game';
}

/** Messages from server to client */
export type ServerMessage =
  | { type: 'room-created'; room: Room; ownerToken: string }
  | { type: 'room-joined'; room: Room }
  | { type: 'room-left'; roomId: string }
  | { type: 'room-updated'; room: Room }
  | { type: 'room-list'; rooms: Room[] }
  | { type: 'game-starting'; roomId: string; relayPort?: number; relayHost?: string; sessionToken?: string }
  | { type: 'error'; message: string }
  | { type: 'chat-broadcast'; roomId: string; userId: string; displayName: string; content: string; sentAt: string }
  | { type: 'welcome'; playerId: string }
  | { type: 'pong'; sentAt: number; serverAt: number }
  | { type: 'presence-update'; players: PresencePlayer[] }
  /** Phase 8: player identity confirmed */
  | { type: 'identity-confirmed'; persistentId: string; displayName: string }
  /** Phase 8: friend events */
  | { type: 'friend-request-received'; requestId: string; fromId: string; fromDisplayName: string }
  | { type: 'friend-request-accepted'; requestId: string; byId: string; byDisplayName: string }
  | { type: 'friend-request-declined'; requestId: string }
  | { type: 'friend-status-update'; friendId: string; status: 'online' | 'in-lobby' | 'in-game' | 'offline'; roomCode?: string; gameTitle?: string }
  /** Phase 8: matchmaking */
  | { type: 'matchmaking-queued'; position: number }
  | { type: 'match-found'; room: Room }
  /** Phase 9: achievement unlocked */
  | { type: 'achievement-unlocked'; achievementId: string; name: string; description: string; icon: string }
  /** Phase 10: tournament bracket changed */
  | { type: 'tournament-updated'; tournamentId: string };
