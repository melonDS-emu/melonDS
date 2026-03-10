import { randomUUID } from 'crypto';
import type { Room, RoomPlayer, LobbyStatus } from './types';

function generateRoomCode(): string {
  const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  let code = '';
  for (let i = 0; i < 6; i++) {
    code += chars[Math.floor(Math.random() * chars.length)];
  }
  return code;
}

/**
 * In-memory lobby/room manager.
 * In production, this would be backed by Redis or a database.
 */
export class LobbyManager {
  private rooms: Map<string, Room> = new Map();

  createRoom(
    hostId: string,
    name: string,
    gameId: string,
    gameTitle: string,
    system: string,
    isPublic: boolean,
    maxPlayers: number,
    displayName: string
  ): Room {
    const roomId = randomUUID();
    const roomCode = generateRoomCode();

    const host: RoomPlayer = {
      id: hostId,
      displayName,
      readyState: 'ready',
      slot: 0,
      isHost: true,
      joinedAt: new Date().toISOString(),
    };

    const room: Room = {
      id: roomId,
      name,
      hostId,
      gameId,
      gameTitle,
      system,
      isPublic,
      roomCode,
      maxPlayers,
      players: [host],
      status: 'waiting',
      createdAt: new Date().toISOString(),
    };

    this.rooms.set(roomId, room);
    return room;
  }

  joinRoom(roomId: string, playerId: string, displayName: string): Room | null {
    const room = this.rooms.get(roomId);
    if (!room) return null;
    if (room.status !== 'waiting') return null;
    if (room.players.length >= room.maxPlayers) return null;
    if (room.players.some((p) => p.id === playerId)) return room;

    const player: RoomPlayer = {
      id: playerId,
      displayName,
      readyState: 'not-ready',
      slot: room.players.length,
      isHost: false,
      joinedAt: new Date().toISOString(),
    };

    room.players.push(player);
    return room;
  }

  joinByCode(roomCode: string, playerId: string, displayName: string): Room | null {
    for (const room of this.rooms.values()) {
      if (room.roomCode === roomCode) {
        return this.joinRoom(room.id, playerId, displayName);
      }
    }
    return null;
  }

  leaveRoom(roomId: string, playerId: string): Room | null {
    const room = this.rooms.get(roomId);
    if (!room) return null;

    room.players = room.players.filter((p) => p.id !== playerId);

    if (room.players.length === 0) {
      this.rooms.delete(roomId);
      return null;
    }

    // Transfer host if host left
    if (room.hostId === playerId) {
      room.hostId = room.players[0].id;
      room.players[0].isHost = true;
    }

    return room;
  }

  toggleReady(roomId: string, playerId: string): Room | null {
    const room = this.rooms.get(roomId);
    if (!room) return null;

    const player = room.players.find((p) => p.id === playerId);
    if (!player) return null;

    player.readyState = player.readyState === 'ready' ? 'not-ready' : 'ready';
    return room;
  }

  startGame(roomId: string, playerId: string): Room | null {
    const room = this.rooms.get(roomId);
    if (!room) return null;
    if (room.hostId !== playerId) return null;

    const allReady = room.players.every((p) => p.readyState === 'ready');
    if (!allReady) return null;

    room.status = 'in-game';
    return room;
  }

  listPublicRooms(): Room[] {
    return Array.from(this.rooms.values()).filter(
      (r) => r.isPublic && r.status === 'waiting'
    );
  }

  getRoom(roomId: string): Room | null {
    return this.rooms.get(roomId) ?? null;
  }

  /** Get all player IDs in a room (for broadcasting). */
  getRoomPlayerIds(roomId: string): string[] {
    const room = this.rooms.get(roomId);
    if (!room) return [];
    return room.players.map((p) => p.id);
  }
}
