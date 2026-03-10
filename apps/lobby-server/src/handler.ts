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
          send(ws, { type: 'error', message: 'Could not join room. The code may be wrong, the room may be full, or the game has already started.' });
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
        } else {
          // Room was deleted (last player left) — free the relay port if one was allocated.
          relay.deallocateSession(msg.payload.roomId);
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
        // Broadcast room-updated first so clients persist the in-game status and
        // relay port — this allows reconnecting clients to rehydrate correctly.
        broadcast(playerIds, { type: 'room-updated', room });
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
        const serverAt = Date.now();
        const rtt = Math.max(0, serverAt - sentAt);
        const quality = qualityFromLatency(rtt);

        // Persist and broadcast connection quality for any room this player is in.
        const rooms = lobby.getRoomsForPlayer(playerId);
        for (const room of rooms) {
          lobby.updateConnectionQuality(room.id, playerId, quality, rtt);
          const updated = lobby.getRoom(room.id);
          if (updated) {
            const ids = lobby.getRoomPlayerIds(room.id);
            broadcast(ids, { type: 'room-updated', room: updated });
          }
        }

        send(ws, { type: 'pong', sentAt, serverAt });
        break;
      }

      default:
        send(ws, { type: 'error', message: 'Unknown message type' });
    }
  });

  ws.on('close', () => {
    connections.delete(playerId);

    // Capture spectator IDs per room before rooms are potentially deleted.
    // We must do this before calling disconnectPlayer because leaveRoom deletes
    // the room when the last player leaves, making the data unavailable afterward.
    const spectatorsByRoom = new Map<string, string[]>();
    for (const room of lobby.getRoomsForPlayer(playerId)) {
      spectatorsByRoom.set(room.id, room.spectators.map((s) => s.id));
    }

    // Clean up rooms where this player was participating
    const affected = lobby.disconnectPlayer(playerId);
    for (const [roomId, room] of affected.entries()) {
      if (room) {
        const remainingIds = lobby.getRoomPlayerIds(roomId);
        broadcast(remainingIds, { type: 'room-updated', room });
      } else {
        // Room was closed (last player left) — free relay port and tell spectators.
        relay.deallocateSession(roomId);
        const spectators = spectatorsByRoom.get(roomId) ?? [];
        broadcast(spectators, { type: 'room-left', roomId });
      }
    }
  });
}
