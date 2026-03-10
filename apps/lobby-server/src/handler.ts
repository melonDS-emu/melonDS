import type { WebSocket } from 'ws';
import { randomUUID } from 'crypto';
import { LobbyManager } from './lobby-manager';
import type { ClientMessage, ServerMessage, ConnectionQuality } from './types';
import { NetplayRelay } from './netplay-relay';

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

/** Derive connection quality from round-trip latency. */
function qualityFromLatency(latencyMs: number): ConnectionQuality {
  if (latencyMs < 50) return 'excellent';
  if (latencyMs < 120) return 'good';
  if (latencyMs < 250) return 'fair';
  return 'poor';
}

export function handleConnection(ws: WebSocket, lobby: LobbyManager, relay: NetplayRelay): void {
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

      case 'join-as-spectator': {
        const { roomId, roomCode, displayName } = msg.payload;
        let room;

        if (roomCode) {
          room = lobby.joinByCodeAsSpectator(roomCode, playerId, displayName);
        } else if (roomId) {
          room = lobby.joinAsSpectator(roomId, playerId, displayName);
        }

        if (!room) {
          send(ws, { type: 'error', message: 'Could not join room as spectator' });
          break;
        }

        send(ws, { type: 'room-joined', room });

        const allIds = lobby.getRoomPlayerIds(room.id);
        broadcast(allIds, { type: 'room-updated', room }, playerId);
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

        // Allocate a relay session for this room
        const relayPort = relay.allocateSession(room.id, room.players.map((p) => p.id));
        if (relayPort === null) {
          send(ws, { type: 'error', message: 'Relay server is at capacity — no free ports. Try again shortly.' });
          // Roll back room status so players can retry
          room.status = 'waiting';
          break;
        }

        room.relayPort = relayPort;

        const playerIds = lobby.getRoomPlayerIds(room.id);
        broadcast(playerIds, {
          type: 'game-starting',
          roomId: room.id,
          relayPort,
        });
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
        const spectator = room.spectators.find((s) => s.id === playerId);
        const sender = player ?? spectator;

        if (!sender) {
          send(ws, { type: 'error', message: 'You are not in this room' });
          break;
        }

        const chatMsg: ServerMessage = {
          type: 'chat-broadcast',
          roomId: room.id,
          userId: playerId,
          displayName: sender.displayName,
          content: msg.payload.content,
          sentAt: new Date().toISOString(),
        };

        const allPlayerIds = lobby.getRoomPlayerIds(room.id);
        broadcast(allPlayerIds, chatMsg);
        break;
      }

      case 'ping': {
        const { sentAt } = msg.payload;
        send(ws, { type: 'pong', sentAt, serverAt: Date.now() });

        // Update connection quality based on the ping
        // The client will send another ping to measure RTT; here we simply respond
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
