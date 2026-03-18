/**
 * Shared protocol types for the lobby WebSocket connection.
 * Mirrors apps/lobby-server/src/types.ts.
 */

export type LobbyStatus = 'waiting' | 'ready' | 'in-game' | 'closed';
export type PlayerReadyState = 'not-ready' | 'ready';
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
  relayPort?: number;
  /** Optional visual theme chosen by the host (e.g. 'spring', 'summer', 'neon', 'classic'). */
  theme?: string;
  /** Ranked vs casual mode. */
  rankMode?: 'casual' | 'ranked';
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

export interface ChatMessage {
  roomId: string;
  userId: string;
  displayName: string;
  content: string;
  sentAt: string;
}

/** Messages sent from client to server */
export type ClientMessage =
  | { type: 'create-room'; payload: CreateRoomPayload }
  | { type: 'join-room'; payload: JoinRoomPayload }
  | { type: 'join-as-spectator'; payload: JoinRoomPayload }
  | { type: 'leave-room'; payload: { roomId: string } }
  | { type: 'toggle-ready'; payload: { roomId: string } }
  | { type: 'list-rooms' }
  | { type: 'start-game'; payload: { roomId: string } }
  | { type: 'chat'; payload: { roomId: string; content: string } }
  | { type: 'ping'; payload: { sentAt: number } }
  /** Phase 8: identity + social graph */
  | { type: 'register-identity'; payload: { identityToken?: string; displayName: string } }
  | { type: 'friend-request'; payload: { toPlayerId: string } }
  | { type: 'friend-request-accept'; payload: { requestId: string } }
  | { type: 'friend-request-decline'; payload: { requestId: string } }
  | { type: 'friend-remove'; payload: { friendId: string } }
  /** Phase 14: direct messages */
  | { type: 'send-dm'; payload: { toPlayer: string; content: string } }
  | { type: 'mark-dm-read'; payload: { fromPlayer: string } };

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
  /** Ranked vs casual mode. Defaults to 'casual'. */
  rankMode?: 'casual' | 'ranked';
}

export interface JoinRoomPayload {
  roomId?: string;
  roomCode?: string;
  displayName: string;
}

/** Messages received from server */
export type ServerMessage =
  | { type: 'welcome'; playerId: string }
  | { type: 'room-created'; room: Room; ownerToken: string }
  | { type: 'room-joined'; room: Room }
  | { type: 'room-left'; roomId: string }
  | { type: 'room-updated'; room: Room }
  | { type: 'room-list'; rooms: Room[] }
  | { type: 'game-starting'; roomId: string; relayPort?: number; relayHost?: string; sessionToken?: string }
  | { type: 'error'; message: string }
  | { type: 'chat-broadcast'; roomId: string; userId: string; displayName: string; content: string; sentAt: string }
  | { type: 'pong'; sentAt: number; serverAt: number }
  | { type: 'presence-update'; players: PresencePlayer[] }
  /** Phase 9: achievement unlocked */
  | { type: 'achievement-unlocked'; achievementId: string; name: string; description: string; icon: string }
  /** Phase 8: social graph events */
  | { type: 'friend-request-received'; requestId: string; fromId: string; fromDisplayName: string }
  | { type: 'friend-request-accepted'; requestId: string; byId: string; byDisplayName: string }
  | { type: 'friend-request-declined'; requestId: string }
  | { type: 'friend-status-update'; friendId: string; status: string; roomCode?: string; gameTitle?: string }
  | { type: 'identity-registered'; persistentId: string; displayName: string }
  /** Phase 10: tournament bracket update */
  | { type: 'tournament-updated'; tournamentId: string }
  /** Phase 14: direct messages */
  | { type: 'dm-received'; message: { id: string; fromPlayer: string; content: string; sentAt: string } }
  | { type: 'dm-read-ack'; fromPlayer: string };

export interface PresencePlayer {
  playerId: string;
  displayName: string;
  roomCode?: string;
  gameTitle?: string;
  status: 'online' | 'in-lobby' | 'in-game';
}
