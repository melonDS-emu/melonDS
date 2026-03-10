import type { WebSocket } from 'ws';
import { randomUUID } from 'crypto';
import { LobbyManager } from './lobby-manager';
import type { ClientMessage, ServerMessage } from './types';

/** Map of playerId -> WebSocket for broadcasting */
const connections = new Map<string, WebSocket>();

function send(ws: WebSocket, message: ServerMessage): void {
  if (ws.readyState === ws.OPEN) {
    ws.send(JSON.stringify(message));
  }
}

function broadcast(playerIds: string[], message: ServerMessage, exclude?: string): void {
  for (const id of playerIds) {
    if (id === exclude) continue;
    const ws = connections.get(id);
    if (ws) send(ws, message);
  }
}

export function handleConnection(ws: WebSocket, lobby: LobbyManager): void {
  const playerId = randomUUID();
  connections.set(playerId, ws);

  send(ws, { type: 'welcome', playerId });

  ws.on('message', (raw) => {
    let msg: ClientMessage;
    try {
      msg = JSON.parse(raw.toString()) as ClientMessage;
    } catch {
      send(ws, { type: 'error', message: 'Invalid JSON' });
      return;
    }

    switch (msg.type) {
      case 'create-room': {
        const { name, gameId, gameTitle, system, isPublic, maxPlayers, displayName } = msg.payload;
        const room = lobby.createRoom(playerId, name, gameId, gameTitle, system, isPublic, maxPlayers, displayName);
        send(ws, { type: 'room-created', room });
        break;
      }

      case 'join-room': {
        const { roomId, roomCode, displayName } = msg.payload;
        let room;

        if (roomCode) {
          room = lobby.joinByCode(roomCode, playerId, displayName);
        } else if (roomId) {
          room = lobby.joinRoom(roomId, playerId, displayName);
        }

        if (!room) {
          send(ws, { type: 'error', message: 'Could not join room' });
          break;
        }

        send(ws, { type: 'room-joined', room });

        // Notify others in the room
        const playerIds = lobby.getRoomPlayerIds(room.id);
        broadcast(playerIds, { type: 'room-updated', room }, playerId);
        break;
      }

      case 'leave-room': {
        const room = lobby.leaveRoom(msg.payload.roomId, playerId);
        send(ws, { type: 'room-left', roomId: msg.payload.roomId });

        if (room) {
          const playerIds = lobby.getRoomPlayerIds(room.id);
          broadcast(playerIds, { type: 'room-updated', room });
        }
        break;
      }

      case 'toggle-ready': {
        const room = lobby.toggleReady(msg.payload.roomId, playerId);
        if (!room) {
          send(ws, { type: 'error', message: 'Could not toggle ready state' });
          break;
        }

        const playerIds = lobby.getRoomPlayerIds(room.id);
        broadcast(playerIds, { type: 'room-updated', room });
        break;
      }

      case 'start-game': {
        const room = lobby.startGame(msg.payload.roomId, playerId);
        if (!room) {
          send(ws, { type: 'error', message: 'Cannot start game. Ensure all players are ready and you are the host.' });
          break;
        }

        const playerIds = lobby.getRoomPlayerIds(room.id);
        broadcast(playerIds, { type: 'game-starting', roomId: room.id });
        break;
      }

      case 'list-rooms': {
        const rooms = lobby.listPublicRooms();
        send(ws, { type: 'room-list', rooms });
        break;
      }

      case 'chat': {
        const room = lobby.getRoom(msg.payload.roomId);
        if (!room) {
          send(ws, { type: 'error', message: 'Room not found' });
          break;
        }

        const player = room.players.find((p) => p.id === playerId);
        if (!player) {
          send(ws, { type: 'error', message: 'You are not in this room' });
          break;
        }

        const chatMsg: ServerMessage = {
          type: 'chat-broadcast',
          roomId: room.id,
          userId: playerId,
          displayName: player.displayName,
          content: msg.payload.content,
          sentAt: new Date().toISOString(),
        };

        const allPlayerIds = lobby.getRoomPlayerIds(room.id);
        broadcast(allPlayerIds, chatMsg);
        break;
      }

      default:
        send(ws, { type: 'error', message: 'Unknown message type' });
    }
  });

  ws.on('close', () => {
    connections.delete(playerId);
    // Clean up rooms where this player was participating
    const affected = lobby.disconnectPlayer(playerId);
    for (const [roomId, room] of affected.entries()) {
      if (room) {
        const remainingIds = lobby.getRoomPlayerIds(roomId);
        broadcast(remainingIds, { type: 'room-updated', room });
      }
    }
  });
}
