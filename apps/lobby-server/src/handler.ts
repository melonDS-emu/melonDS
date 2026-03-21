import type { WebSocket } from 'ws';
import { randomUUID } from 'crypto';
import { LobbyManager } from './lobby-manager';
import type { SqliteLobbyManager } from './sqlite-lobby-manager';
import type { ClientMessage, ServerMessage, ConnectionQuality, PresencePlayer } from './types';
import { NetplayRelay } from './netplay-relay';
import { normalizeDisplayName, validateDisplayName, generateOwnerToken, verifyOwnerToken } from './auth';
import { SessionHistory } from './session-history';
import type { SqliteSessionHistory } from './sqlite-session-history';
import type { SessionRecord } from './session-history';
import type { FriendStore } from './friend-store';
import type { MatchmakingQueue } from './matchmaking';
import type { PlayerIdentityStore } from './player-identity';
import type { AchievementStore } from './achievement-store';
import type { SqliteAchievementStore } from './sqlite-achievement-store';
import type { MessageStore } from './message-store';
import type { SqliteMessageStore } from './sqlite-message-store';
import { GlobalChatStore } from './global-chat-store';
import { NotificationStore } from './notification-store';

/** Map of playerId -> WebSocket for broadcasting */
const connections = new Map<string, WebSocket>();
/** Map of sessionPlayerId -> persistent player ID (Phase 8 identity) */
const sessionToPersistentId = new Map<string, string>();
/** Map of persistent player ID -> display name (Phase 8 identity) */
const persistentDisplayNames = new Map<string, string>();
/**
 * Best-effort map of display name → ephemeral playerId for achievement WS push.
 *
 * Updated when a player creates or joins a room (the first point at which their
 * display name is known).  Cleaned up on disconnect.  Stale entries (e.g. from
 * a player who left a room but is still connected) are harmless: the old
 * playerId will simply not be found in `connections`.
 */
const displayNameToPlayerId = new Map<string, string>();

/**
 * Per-player chat rate limiting: max CHAT_RATE_LIMIT_MAX messages per
 * CHAT_RATE_LIMIT_WINDOW_MS milliseconds.  Uses a simple sliding-window
 * counter that resets when the window expires.
 */
const CHAT_RATE_LIMIT_MAX = 5;
const CHAT_RATE_LIMIT_WINDOW_MS = 5_000;

interface ChatRateEntry {
  count: number;
  windowStart: number;
}

const chatRateMap = new Map<string, ChatRateEntry>();

function isChatRateLimited(playerId: string): boolean {
  const now = Date.now();
  const entry = chatRateMap.get(playerId);

  if (!entry || now - entry.windowStart >= CHAT_RATE_LIMIT_WINDOW_MS) {
    // Start a fresh window
    chatRateMap.set(playerId, { count: 1, windowStart: now });
    return false;
  }

  entry.count += 1;
  if (entry.count > CHAT_RATE_LIMIT_MAX) {
    return true;
  }

  return false;
}

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

/** Broadcast a friend-status-update to all friends of the given persistent player ID. */
function broadcastFriendStatus(
  persistentId: string,
  status: PresencePlayer['status'] | 'offline',
  friends: FriendStore,
  roomCode?: string,
  gameTitle?: string
): void {
  const friendEntries = friends.getFriends(persistentId);
  for (const entry of friendEntries) {
    // Find any session connected with this persistent friend ID
    for (const [sessionId, pid] of sessionToPersistentId.entries()) {
      if (pid === entry.friendId) {
        const ws = connections.get(sessionId);
        if (ws) {
          send(ws, {
            type: 'friend-status-update',
            friendId: persistentId,
            status,
            roomCode,
            gameTitle,
          });
        }
      }
    }
    // Also check if friendId IS a session id directly (for non-registered users)
    const directWs = connections.get(entry.friendId);
    if (directWs) {
      send(directWs, {
        type: 'friend-status-update',
        friendId: persistentId,
        status,
        roomCode,
        gameTitle,
      });
    }
  }
}

export interface Phase8Stores {
  friends?: FriendStore;
  matchmaking?: MatchmakingQueue;
  playerIdentity?: PlayerIdentityStore;
  achievements?: AchievementStore | SqliteAchievementStore;
  /** Phase 14: direct message store */
  messages?: MessageStore | SqliteMessageStore;
  /** Phase 17: global chat store */
  globalChat?: GlobalChatStore;
  /** Phase 17: notification store */
  notifications?: NotificationStore;
}

/**
 * Push a retro-achievement-unlocked event to a connected player's WebSocket.
 * Also adds a notification via the notification store if provided.
 * Returns true if the player was found and the message was sent.
 */
export function pushRetroAchievementUnlocked(
  playerId: string,
  achievementId: string,
  title: string,
  description: string,
  badge: string,
  points: number,
  notificationStore?: NotificationStore
): boolean {
  const playerWs = connections.get(playerId);
  if (!playerWs) return false;
  send(playerWs, {
    type: 'retro-achievement-unlocked',
    achievementId,
    title,
    description,
    badge,
    points,
  });
  notificationStore?.add(
    playerId,
    'achievement-unlocked',
    `🏅 Retro Achievement: ${title}`,
    description,
    achievementId
  );
  return true;
}

/**
 * After a session ends, check and push any newly unlocked achievements to
 * each player who is still connected (identified by their display name).
 */
function pushAchievementUnlocks(
  endedSession: SessionRecord | null,
  sessionHistory: SessionHistory | SqliteSessionHistory | undefined,
  achievementStore: AchievementStore | SqliteAchievementStore | undefined,
  notificationStore: NotificationStore | undefined
): void {
  if (!endedSession || !sessionHistory || !achievementStore) return;
  const allSessions = sessionHistory.getAll();
  for (const displayName of endedSession.players) {
    const playerSessions = allSessions.filter((s) => s.players.includes(displayName));
    const newlyUnlocked = achievementStore.checkAndUnlock(displayName, displayName, playerSessions);
    if (newlyUnlocked.length === 0) continue;
    const pid = displayNameToPlayerId.get(displayName);
    if (!pid) continue;
    const playerWs = connections.get(pid);
    if (!playerWs) continue;
    for (const def of newlyUnlocked) {
      send(playerWs, {
        type: 'achievement-unlocked',
        achievementId: def.id,
        name: def.name,
        description: def.description,
        icon: def.icon,
      });
      notificationStore?.add(
        pid,
        'achievement-unlocked',
        `Achievement Unlocked: ${def.name}`,
        def.description,
        def.id
      );
    }
  }
}

export function handleConnection(
  ws: WebSocket,
  lobby: LobbyManager | SqliteLobbyManager,
  relay: NetplayRelay,
  relayHost = 'localhost',
  sessionHistory?: SessionHistory | SqliteSessionHistory,
  phase8?: Phase8Stores
): void {
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
        const { name, gameId, gameTitle, system, isPublic, maxPlayers, displayName, theme, rankMode } = msg.payload;
        const normalizedName = normalizeDisplayName(displayName);
        const nameError = validateDisplayName(normalizedName);
        if (nameError) {
          send(ws, { type: 'error', message: nameError });
          break;
        }
        displayNameToPlayerId.set(normalizedName, playerId);
        const room = lobby.createRoom(playerId, name, gameId, gameTitle, system, isPublic, maxPlayers, normalizedName, theme, rankMode as 'casual' | 'ranked' | undefined);
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
        displayNameToPlayerId.set(normalizedName, playerId);
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

      case 'rejoin-room': {
        const { roomId, roomCode, displayName } = msg.payload;
        const normalizedName = normalizeDisplayName(displayName);
        const nameError = validateDisplayName(normalizedName);
        if (nameError) {
          send(ws, { type: 'error', message: nameError });
          break;
        }
        displayNameToPlayerId.set(normalizedName, playerId);
        let room;

        if (roomCode) {
          room = lobby.rejoinByCode(roomCode, playerId, normalizedName);
        } else if (roomId) {
          room = lobby.rejoinRoom(roomId, playerId, normalizedName);
        }

        if (!room) {
          send(ws, { type: 'error', message: 'Could not rejoin room. The session may have ended or the room is at capacity.' });
          break;
        }

        send(ws, {
          type: 'room-rejoined',
          room,
          relayPort: room.relayPort,
          relayHost,
          sessionToken: randomUUID(),
        });

        const rejoinPlayerIds = lobby.getRoomPlayerIds(room.id);
        broadcast(rejoinPlayerIds, { type: 'room-updated', room }, playerId);
        broadcastPresence(lobby, connections);
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
          const ended = sessionHistory?.endSession(msg.payload.roomId) ?? null;
          pushAchievementUnlocks(ended, sessionHistory, phase8?.achievements, phase8?.notifications);
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
        if (isChatRateLimited(playerId)) {
          send(ws, { type: 'error', message: 'You are sending messages too fast — please slow down.' });
          break;
        }

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

      // -----------------------------------------------------------------------
      // Phase 8: Player identity registration
      // -----------------------------------------------------------------------

      case 'register-identity': {
        const { identityToken, displayName } = msg.payload;
        if (!phase8?.playerIdentity) break;
        const normalizedName = normalizeDisplayName(displayName);
        const nameError = validateDisplayName(normalizedName);
        if (nameError) {
          send(ws, { type: 'error', message: nameError });
          break;
        }
        const identity = phase8.playerIdentity.resolve(identityToken, normalizedName);
        sessionToPersistentId.set(playerId, identity.id);
        persistentDisplayNames.set(identity.id, identity.displayName);
        send(ws, { type: 'identity-confirmed', persistentId: identity.id, displayName: identity.displayName });

        // Notify friends that this player came online
        if (phase8.friends) {
          broadcastFriendStatus(identity.id, 'online', phase8.friends);
        }
        break;
      }

      // -----------------------------------------------------------------------
      // Phase 8: Friend request flow
      // -----------------------------------------------------------------------

      case 'friend-request': {
        if (!phase8?.friends) {
          send(ws, { type: 'error', message: 'Friend system not available.' });
          break;
        }
        const fromPersistentId = sessionToPersistentId.get(playerId);
        if (!fromPersistentId) {
          send(ws, { type: 'error', message: 'Register your identity before sending friend requests.' });
          break;
        }
        const { toPlayerId } = msg.payload;
        const request = phase8.friends.addRequest(fromPersistentId, toPlayerId);
        if (!request) {
          send(ws, { type: 'error', message: 'Could not send friend request.' });
          break;
        }
        // Notify the recipient if they are online
        const fromDisplayName = persistentDisplayNames.get(fromPersistentId) ?? fromPersistentId;
        for (const [sid, pid] of sessionToPersistentId.entries()) {
          if (pid === toPlayerId) {
            const recipientWs = connections.get(sid);
            if (recipientWs) {
              send(recipientWs, {
                type: 'friend-request-received',
                requestId: request.id,
                fromId: fromPersistentId,
                fromDisplayName,
              });
            }
            // Phase 17: create a notification for the recipient
            phase8.notifications?.add(
              sid,
              'friend-request',
              'Friend Request',
              `${fromDisplayName} sent you a friend request.`,
              request.id
            );
          }
        }
        break;
      }

      case 'friend-request-accept': {
        if (!phase8?.friends) {
          send(ws, { type: 'error', message: 'Friend system not available.' });
          break;
        }
        const acceptorId = sessionToPersistentId.get(playerId);
        if (!acceptorId) {
          send(ws, { type: 'error', message: 'Register your identity before accepting friend requests.' });
          break;
        }
        const { requestId } = msg.payload;
        const accepted = phase8.friends.acceptRequest(requestId, acceptorId);
        if (!accepted) {
          send(ws, { type: 'error', message: 'Friend request not found or cannot be accepted.' });
          break;
        }
        // Notify the requester
        const acceptorName = persistentDisplayNames.get(acceptorId) ?? acceptorId;
        for (const [sid, pid] of sessionToPersistentId.entries()) {
          if (pid === accepted.fromId) {
            const requesterWs = connections.get(sid);
            if (requesterWs) {
              send(requesterWs, {
                type: 'friend-request-accepted',
                requestId,
                byId: acceptorId,
                byDisplayName: acceptorName,
              });
            }
          }
        }
        // Broadcast current online status to both parties
        if (phase8.friends) {
          broadcastFriendStatus(acceptorId, 'online', phase8.friends);
          broadcastFriendStatus(accepted.fromId, 'online', phase8.friends);
        }
        break;
      }

      case 'friend-request-decline': {
        if (!phase8?.friends) {
          send(ws, { type: 'error', message: 'Friend system not available.' });
          break;
        }
        const decliningId = sessionToPersistentId.get(playerId);
        if (!decliningId) {
          send(ws, { type: 'error', message: 'Register your identity before declining friend requests.' });
          break;
        }
        const { requestId: declineRequestId } = msg.payload;
        const declined = phase8.friends.declineRequest(declineRequestId, decliningId);
        if (!declined) {
          send(ws, { type: 'error', message: 'Friend request not found or cannot be declined.' });
          break;
        }
        // Notify the requester
        for (const [sid, pid] of sessionToPersistentId.entries()) {
          if (pid === declined.fromId) {
            const requesterWs = connections.get(sid);
            if (requesterWs) {
              send(requesterWs, { type: 'friend-request-declined', requestId: declineRequestId });
            }
          }
        }
        break;
      }

      case 'friend-remove': {
        if (!phase8?.friends) {
          send(ws, { type: 'error', message: 'Friend system not available.' });
          break;
        }
        const removingId = sessionToPersistentId.get(playerId);
        if (!removingId) {
          send(ws, { type: 'error', message: 'Register your identity before managing friends.' });
          break;
        }
        const { friendId: removeFriendId } = msg.payload;
        phase8.friends.removeFriend(removingId, removeFriendId);
        break;
      }

      // -----------------------------------------------------------------------
      // Phase 8: Matchmaking
      // -----------------------------------------------------------------------

      case 'matchmaking-join': {
        if (!phase8?.matchmaking) {
          send(ws, { type: 'error', message: 'Matchmaking not available.' });
          break;
        }
        const { gameId, gameTitle, system, maxPlayers, displayName: mmName } = msg.payload;
        const normalizedMmName = normalizeDisplayName(mmName);
        const mmNameError = validateDisplayName(normalizedMmName);
        if (mmNameError) {
          send(ws, { type: 'error', message: mmNameError });
          break;
        }

        phase8.matchmaking.join({
          playerId,
          displayName: normalizedMmName,
          gameId,
          gameTitle,
          system,
          maxPlayers,
        });

        const queueAll = phase8.matchmaking.getAll();
        const position = queueAll.findIndex((e) => e.playerId === playerId) + 1;
        send(ws, { type: 'matchmaking-queued', position });

        // Check for matches
        const matches = phase8.matchmaking.flushMatches();
        for (const match of matches) {
          // Create a room for the matched players
          const firstPlayer = match.players[0];
          const room = lobby.createRoom(
            firstPlayer.playerId,
            `${match.gameTitle} — Matchmaking`,
            match.gameId,
            match.gameTitle,
            match.system,
            false,
            match.maxPlayers,
            firstPlayer.displayName
          );

          // Add remaining players to the room
          for (let i = 1; i < match.players.length; i++) {
            const p = match.players[i];
            lobby.joinRoom(room.id, p.playerId, p.displayName);
          }

          const updatedRoom = lobby.getRoom(room.id) ?? room;

          // Notify matched players
          for (const matchedPlayer of match.players) {
            const matchedWs = connections.get(matchedPlayer.playerId);
            if (matchedWs) {
              send(matchedWs, { type: 'match-found', room: updatedRoom });
            }
          }
        }
        break;
      }

      case 'matchmaking-leave': {
        if (!phase8?.matchmaking) break;
        phase8.matchmaking.leave(playerId);
        break;
      }

      // -----------------------------------------------------------------------
      // Phase 14: direct messages
      // -----------------------------------------------------------------------
      case 'send-dm': {
        const { toPlayer, content } = msg.payload;
        if (!phase8?.messages) break;
        if (!content || typeof content !== 'string' || content.trim() === '') break;
        if (content.length > 2000) {
          send(ws, { type: 'error', message: 'Message too long (max 2000 characters)' });
          break;
        }
        // Resolve sender display name from displayNameToPlayerId reverse map
        const senderName =
          Array.from(displayNameToPlayerId.entries()).find(([, pid]) => pid === playerId)?.[0] ??
          'Unknown';
        try {
          const dm = phase8.messages.sendMessage(senderName, toPlayer, content.trim());
          // Deliver to recipient if online
          const recipientPid = displayNameToPlayerId.get(toPlayer);
          if (recipientPid) {
            const recipientWs = connections.get(recipientPid);
            if (recipientWs) {
              send(recipientWs, {
                type: 'dm-received',
                message: { id: dm.id, fromPlayer: dm.fromPlayer, content: dm.content, sentAt: dm.sentAt },
              });
            }
            // Phase 17: create a notification for the recipient
            phase8.notifications?.add(
              recipientPid,
              'dm-received',
              `Message from ${senderName}`,
              dm.content.length > 80 ? dm.content.slice(0, 77) + '…' : dm.content,
              dm.id
            );
          }
        } catch {
          send(ws, { type: 'error', message: 'Failed to send message.' });
        }
        break;
      }

      case 'mark-dm-read': {
        const { fromPlayer } = msg.payload;
        if (!phase8?.messages) break;
        const readerName =
          Array.from(displayNameToPlayerId.entries()).find(([, pid]) => pid === playerId)?.[0] ??
          'Unknown';
        try {
          phase8.messages.markRead(fromPlayer, readerName);
          // Notify sender their message was read
          const senderPid = displayNameToPlayerId.get(fromPlayer);
          if (senderPid) {
            const senderWs = connections.get(senderPid);
            if (senderWs) {
              send(senderWs, { type: 'dm-read-ack', fromPlayer: readerName });
            }
          }
        } catch {
          // Best-effort: ignore read-receipt errors
        }
        break;
      }

      // -----------------------------------------------------------------------
      // Phase 17: global chat
      // -----------------------------------------------------------------------
      case 'send-global-chat': {
        const chatText = msg.text;
        if (!phase8?.globalChat) break;
        if (!chatText || typeof chatText !== 'string' || chatText.trim() === '') break;
        const senderDisplayName =
          Array.from(displayNameToPlayerId.entries()).find(([, pid]) => pid === playerId)?.[0] ??
          'Unknown';
        try {
          const chatMsg = phase8.globalChat.post(playerId, senderDisplayName, chatText.trim());
          // Broadcast to ALL connected clients
          for (const [, clientWs] of connections) {
            send(clientWs, { type: 'global-chat-message', ...chatMsg });
          }
        } catch {
          send(ws, { type: 'error', message: 'Failed to post chat message.' });
        }
        break;
      }

      default:
        send(ws, { type: 'error', message: 'Unknown message type' });
    }
  });

  ws.on('close', () => {
    connections.delete(playerId);
    chatRateMap.delete(playerId);

    // Remove from matchmaking queue if applicable
    phase8?.matchmaking?.leave(playerId);

    // Notify friends that this player went offline
    const persistentId = sessionToPersistentId.get(playerId);
    if (persistentId && phase8?.friends) {
      broadcastFriendStatus(persistentId, 'offline', phase8.friends);
    }
    sessionToPersistentId.delete(playerId);

    // Remove displayName → playerId mapping for this connection.
    // Remove ALL entries for this playerId to avoid stale ghost entries in the
    // map (a player could theoretically register under more than one display name
    // during their session lifetime).
    for (const [dn, pid] of displayNameToPlayerId.entries()) {
      if (pid === playerId) {
        displayNameToPlayerId.delete(dn);
      }
    }

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
        const ended = sessionHistory?.endSession(roomId) ?? null;
        pushAchievementUnlocks(ended, sessionHistory, phase8?.achievements, phase8?.notifications);
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
function buildPresencePlayers(lobby: LobbyManager | SqliteLobbyManager, connections: Map<string, WebSocket>): PresencePlayer[] {
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

function broadcastPresence(lobby: LobbyManager | SqliteLobbyManager, connections: Map<string, WebSocket>): void {
  const players = buildPresencePlayers(lobby, connections);
  const msg: ServerMessage = { type: 'presence-update', players };
  for (const [, clientWs] of connections) {
    send(clientWs, msg);
  }
}
