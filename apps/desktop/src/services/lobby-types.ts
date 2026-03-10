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

/** ROM file metadata returned by the server's directory scanner. */
export interface RomFileInfo {
  filePath: string;
  fileName: string;
  system: string;
  inferredTitle: string;
  fileSizeBytes: number;
  lastModified: string;
}

export interface LaunchEmulatorPayload {
  roomId: string;
  romPath: string;
  system: string;
  backendId: string;
  saveDirectory?: string;
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
  | { type: 'scan-roms'; payload: { directory: string; recursive?: boolean } }
  | { type: 'launch-emulator'; payload: LaunchEmulatorPayload };

export interface CreateRoomPayload {
  name: string;
  gameId: string;
  gameTitle: string;
  system: string;
  isPublic: boolean;
  maxPlayers: number;
  displayName: string;
}

export interface JoinRoomPayload {
  roomId?: string;
  roomCode?: string;
  displayName: string;
}

/** Messages received from server */
export type ServerMessage =
  | { type: 'welcome'; playerId: string }
  | { type: 'room-created'; room: Room }
  | { type: 'room-joined'; room: Room }
  | { type: 'room-left'; roomId: string }
  | { type: 'room-updated'; room: Room }
  | { type: 'room-list'; rooms: Room[] }
  | { type: 'game-starting'; roomId: string; relayPort?: number; relayHost?: string; relayToken?: string }
  | { type: 'error'; message: string }
  | { type: 'chat-broadcast'; roomId: string; userId: string; displayName: string; content: string; sentAt: string }
  | { type: 'pong'; sentAt: number; serverAt: number }
  | { type: 'rom-scan-result'; roms: RomFileInfo[]; error?: string }
  | { type: 'emulator-launched'; sessionId: string; pid: number }
  | { type: 'emulator-launch-error'; message: string };
