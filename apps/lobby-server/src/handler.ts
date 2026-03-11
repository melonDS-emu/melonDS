import type { WebSocket } from 'ws';
import { randomUUID } from 'crypto';
import { LobbyManager } from './lobby-manager';
import type { ClientMessage, ServerMessage, ConnectionQuality, PresencePlayer } from './types';
import { NetplayRelay } from './netplay-relay';
import { normalizeDisplayName, validateDisplayName, generateOwnerToken, verifyOwnerToken } from './auth';
import { SessionHistory } from './session-history';

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

export function handleConnection(ws: WebSocket, lobby: LobbyManager, relay: NetplayRelay, relayHost = 'localhost', sessionHistory?: SessionHistory): void {
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
        const normalizedName = normalizeDisplayName(displayName);
        const nameError = validateDisplayName(normalizedName);
        if (nameError) {
          send(ws, { type: 'error', message: nameError });
          break;
        }
        const room = lobby.createRoom(playerId, name, gameId, gameTitle, system, isPublic, maxPlayers, normalizedName);
        const ownerToken = generateOwnerToken(room.id);
        send(ws, { type: 'room-created', room, ownerToken });
        broadcastPresence(lobby, connections);
        break;
      }

      case 'join-room': {
        const { roomId, roomCode, displayName } = msg.payload;
        const normalizedName = normalizeDisplayName(displayName);
        const nameError = validateDisplayName(normalizedName);
        if (nameError) {
          send(ws, { type: 'error', message: nameError });
          break;
        }
        let room;

        if (roomCode) {
          room = lobby.joinByCode(roomCode, playerId, normalizedName);
        } else if (roomId) {
          room = lobby.joinRoom(roomId, playerId, normalizedName);
        }

        if (!room) {
          send(ws, { type: 'error', message: 'Could not join room. The code may be wrong, the room may be full, or the game has already started.' });
          break;
        }

        send(ws, { type: 'room-joined', room });

        // Notify others in the room
        const playerIds = lobby.getRoomPlayerIds(room.id);
        broadcast(playerIds, { type: 'room-updated', room }, playerId);
        broadcastPresence(lobby, connections);
        break;
      }

      case 'join-as-spectator': {
        const { roomId, roomCode, displayName } = msg.payload;
        const normalizedSpectatorName = normalizeDisplayName(displayName);
        const spectatorNameError = validateDisplayName(normalizedSpectatorName);
        if (spectatorNameError) {
          send(ws, { type: 'error', message: spectatorNameError });
          break;
        }
        let room;

        if (roomCode) {
          room = lobby.joinByCodeAsSpectator(roomCode, playerId, normalizedSpectatorName);
        } else if (roomId) {
          room = lobby.joinAsSpectator(roomId, playerId, normalizedSpectatorName);
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
          sessionHistory?.endSession(msg.payload.roomId);
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

        // Generate a unique session token per player so the relay can identify each
        // emulator connection. Spectators receive the event without a token.
        for (const player of room.players) {
          const playerWs = connections.get(player.id);
          if (playerWs) {
            send(playerWs, {
              type: 'game-starting',
              roomId: room.id,
              relayPort,
              relayHost,
              sessionToken: randomUUID(),
            });
          }
        }
        const spectatorIds = room.spectators.map((s) => s.id);
        broadcast(spectatorIds, {
          type: 'game-starting',
          roomId: room.id,
          relayPort,
          relayHost,
        });

        // Record the session start in history
        sessionHistory?.startSession(
          room.id,
          room.gameId,
          room.gameTitle,
          room.system,
          room.players.map((p) => p.displayName)
        );

        broadcastPresence(lobby, connections);
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

      case 'kick-player': {
        const { roomId, targetPlayerId, ownerToken } = msg.payload;
        const room = lobby.getRoom(roomId);
        if (!room) {
          send(ws, { type: 'error', message: 'Room not found' });
          break;
        }
        if (room.hostId !== playerId) {
          send(ws, { type: 'error', message: 'Only the host can kick players.' });
          break;
        }
        if (!verifyOwnerToken(roomId, ownerToken)) {
          send(ws, { type: 'error', message: 'Invalid owner token.' });
          break;
        }
        if (targetPlayerId === playerId) {
          send(ws, { type: 'error', message: 'You cannot kick yourself.' });
          break;
        }
        const updatedRoom = lobby.leaveRoom(roomId, targetPlayerId);
        const targetWs = connections.get(targetPlayerId);
        if (targetWs) {
          send(targetWs, { type: 'room-left', roomId });
        }
        if (updatedRoom) {
          const kickedRoomPlayerIds = lobby.getRoomPlayerIds(roomId);
          broadcast(kickedRoomPlayerIds, { type: 'room-updated', room: updatedRoom });
        } else {
          relay.deallocateSession(roomId);
        }
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
        sessionHistory?.endSession(roomId);
        const spectators = spectatorsByRoom.get(roomId) ?? [];
        broadcast(spectators, { type: 'room-left', roomId });
      }
    }

    broadcastPresence(lobby, connections);
  });
}

/**
 * Build a lightweight presence snapshot and broadcast it to all connected clients.
 * Each client can then update their friends-list or lobby views without polling.
 */
function buildPresencePlayers(lobby: LobbyManager, connections: Map<string, WebSocket>): PresencePlayer[] {
  const players: PresencePlayer[] = [];
  const seenPlayers = new Set<string>();

  for (const [pid] of connections) {
    if (seenPlayers.has(pid)) continue;

    const rooms = lobby.getRoomsForPlayer(pid);
    if (rooms.length > 0) {
      const room = rooms[0];
      const playerRecord = room.players.find((p) => p.id === pid);
      if (playerRecord) {
        seenPlayers.add(pid);
        players.push({
          playerId: pid,
          displayName: playerRecord.displayName,
          roomCode: room.roomCode,
          gameTitle: room.status === 'in-game' ? room.gameTitle : undefined,
          status: room.status === 'in-game' ? 'in-game' : 'in-lobby',
        });
      }
    }
    // Players not yet in a room are omitted — they have no display name yet.
  }

  return players;
}

function broadcastPresence(lobby: LobbyManager, connections: Map<string, WebSocket>): void {
  const players = buildPresencePlayers(lobby, connections);
  const msg: ServerMessage = { type: 'presence-update', players };
  for (const [, clientWs] of connections) {
    send(clientWs, msg);
  }
}
