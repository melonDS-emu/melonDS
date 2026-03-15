import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useRef,
  useState,
} from 'react';
import type { ReactNode } from 'react';
import type {
  ClientMessage,
  ServerMessage,
  Room,
  ChatMessage,
  CreateRoomPayload,
  JoinRoomPayload,
} from '../services/lobby-types';
import { getRomDirectory, getSaveDirectory } from '../lib/rom-settings';
import { resolveGameRomPath } from '../lib/rom-library';
import { useToast } from './ToastContext';

const WS_URL = import.meta.env.VITE_WS_URL ?? 'ws://localhost:8080';
/** Base URL for the local HTTP launch API (same host as the lobby server by default). */
const LAUNCH_API_BASE = import.meta.env.VITE_LAUNCH_API_URL ?? WS_URL.replace(/^ws/, 'http');
const RECONNECT_DELAY_MS = 3000;
const PING_INTERVAL_MS = 10_000;

/** Returns the default emulator backend ID for a given system, or null if unknown. */
function defaultBackendId(system: string): string | null {
  switch (system.toLowerCase()) {
    case 'n64': return 'mupen64plus';
    case 'nds': return 'melonds';
    case 'gba': return 'mgba';
    case 'gb':
    case 'gbc': return 'sameboy';
    case 'nes': return 'fceux';
    case 'snes': return 'snes9x';
    default: return null;
  }
}

export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'error';

export interface RelayInfo {
  port: number;
  host: string;
}

/** A pending inbound friend request. */
export interface PendingFriendRequest {
  requestId: string;
  fromId: string;
  fromDisplayName: string;
}

/** Live friend status entry from server push. */
export interface FriendStatus {
  friendId: string;
  status: string;
  roomCode?: string;
  gameTitle?: string;
}

/** Phase 14: an incoming direct message received via WS. */
export interface IncomingDm {
  id: string;
  fromPlayer: string;
  content: string;
  sentAt: string;
}

interface LobbyContextValue {
  connectionState: ConnectionState;
  playerId: string | null;
  currentRoom: Room | null;
  publicRooms: Room[];
  chatMessages: ChatMessage[];
  error: string | null;
  /** Latency to the lobby server in ms, updated every ping cycle. */
  latencyMs: number | null;
  /** Relay info set when a game starts. */
  relayInfo: RelayInfo | null;
  /** Session token for the relay TCP connection (set when game starts). */
  sessionToken: string | null;
  /** Room ownership token returned by the server on room creation. Required for host-only actions. */
  ownerToken: string | null;
  /** Live presence snapshot from the server (updated on join/leave/start events). */
  onlinePlayers: import('../services/lobby-types').PresencePlayer[];
  /** The active WebSocket connection (null if disconnected). Exposed for voice chat signaling. */
  ws: WebSocket | null;
  /** Inbound friend requests pending a response. */
  pendingFriendRequests: PendingFriendRequest[];
  /** Live friend statuses pushed by the server. */
  friendStatuses: FriendStatus[];

  // Actions
  createRoom: (payload: Omit<CreateRoomPayload, 'displayName'>, displayName: string) => void;
  joinByCode: (roomCode: string, displayName: string) => void;
  joinById: (roomId: string, displayName: string) => void;
  joinAsSpectator: (payload: Omit<JoinRoomPayload, 'displayName'>, displayName: string) => void;
  leaveRoom: () => void;
  toggleReady: () => void;
  startGame: () => void;
  sendChat: (content: string) => void;
  listRooms: () => void;
  clearError: () => void;
  /** Phase 8: register player identity (enables friend graph). */
  registerIdentity: (displayName: string, identityToken?: string) => void;
  sendFriendRequest: (toPlayerId: string) => void;
  acceptFriendRequest: (requestId: string) => void;
  declineFriendRequest: (requestId: string) => void;
  removeFriend: (friendId: string) => void;
  /** Phase 14: direct messages. */
  sendDm: (toPlayer: string, content: string) => void;
  markDmRead: (fromPlayer: string) => void;
  unreadDmCount: number;
  incomingDms: IncomingDm[];
}

const LobbyContext = createContext<LobbyContextValue | null>(null);

export function LobbyProvider({ children }: { children: ReactNode }) {
  const { addToast } = useToast();
  const [connectionState, setConnectionState] = useState<ConnectionState>('disconnected');
  const [playerId, setPlayerId] = useState<string | null>(null);
  const [currentRoom, setCurrentRoom] = useState<Room | null>(null);
  const [publicRooms, setPublicRooms] = useState<Room[]>([]);
  const [chatMessages, setChatMessages] = useState<ChatMessage[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [latencyMs, setLatencyMs] = useState<number | null>(null);
  const [relayInfo, setRelayInfo] = useState<RelayInfo | null>(null);
  const [sessionToken, setSessionToken] = useState<string | null>(null);
  const [ownerToken, setOwnerToken] = useState<string | null>(null);
  const [onlinePlayers, setOnlinePlayers] = useState<import('../services/lobby-types').PresencePlayer[]>([]);
  const [pendingFriendRequests, setPendingFriendRequests] = useState<PendingFriendRequest[]>([]);
  const [friendStatuses, setFriendStatuses] = useState<FriendStatus[]>([]);
  const [incomingDms, setIncomingDms] = useState<IncomingDm[]>([]);
  const [unreadDmCount, setUnreadDmCount] = useState(0);

  const wsRef = useRef<WebSocket | null>(null);
  const currentRoomRef = useRef<Room | null>(null);
  const playerIdRef = useRef<string | null>(null);
  const pingTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);

  // Keep refs in sync with state for use in closures
  currentRoomRef.current = currentRoom;
  playerIdRef.current = playerId;

  const send = useCallback((msg: ClientMessage) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(msg));
    }
  }, []);

  useEffect(() => {
    let ws: WebSocket;
    let reconnectTimeout: ReturnType<typeof setTimeout> | null = null;
    let unmounted = false;

    function startPing() {
      if (pingTimerRef.current) clearInterval(pingTimerRef.current);
      pingTimerRef.current = setInterval(() => {
        if (ws?.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({ type: 'ping', payload: { sentAt: Date.now() } } satisfies ClientMessage));
        }
      }, PING_INTERVAL_MS);
    }

    function stopPing() {
      if (pingTimerRef.current) {
        clearInterval(pingTimerRef.current);
        pingTimerRef.current = null;
      }
    }

    function connect() {
      if (unmounted) return;
      setConnectionState('connecting');
      ws = new WebSocket(WS_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        if (unmounted) return;
        setConnectionState('connected');
        // Request public rooms when connected
        ws.send(JSON.stringify({ type: 'list-rooms' } satisfies ClientMessage));
        startPing();
      };

      ws.onmessage = (event: MessageEvent<string>) => {
        if (unmounted) return;
        let msg: ServerMessage;
        try {
          msg = JSON.parse(event.data) as ServerMessage;
        } catch {
          return;
        }

        switch (msg.type) {
          case 'welcome':
            setPlayerId(msg.playerId);
            break;

          case 'room-created':
            setCurrentRoom(msg.room);
            setOwnerToken(msg.ownerToken);
            break;

          case 'room-joined':
            setCurrentRoom(msg.room);
            break;

          case 'room-updated':
            setCurrentRoom((prev) =>
              prev?.id === msg.room.id ? msg.room : prev
            );
            setPublicRooms((prev) =>
              prev.map((r) => (r.id === msg.room.id ? msg.room : r))
            );
            break;

          case 'room-left':
            setCurrentRoom((prev) =>
              prev?.id === msg.roomId ? null : prev
            );
            break;

          case 'room-list':
            setPublicRooms(msg.rooms);
            break;

          case 'game-starting': {
            const relayPort = msg.relayPort;
            const relayHost = msg.relayHost ?? 'localhost';
            if (relayPort) {
              setRelayInfo({ port: relayPort, host: relayHost });
            }
            if (msg.sessionToken) {
              setSessionToken(msg.sessionToken);
            }
            // Mark room as in-game so clients can show the correct status
            // even if they reload or reconnect.
            setCurrentRoom((prev) =>
              prev?.id === msg.roomId
                ? { ...prev, status: 'in-game', relayPort }
                : prev
            );

            // Attempt to auto-launch the emulator if a ROM path can be resolved
            // and a session token was provided (i.e. this client is a player, not a spectator).
            if (msg.sessionToken && relayPort) {
              const room = currentRoomRef.current;
              const romPath = resolveGameRomPath(room?.gameId ?? '');
              if (romPath) {
                const myPlayerId = playerIdRef.current;
                const mySlot = room?.players.find((p) => p.id === myPlayerId)?.slot ?? 0;
                const backendId = defaultBackendId(room?.system ?? '');

                if (backendId) {
                  const romDir = getRomDirectory();
                  // Derive a directory from the rom path in case it points to a
                  // specific file (e.g. from a per-game association) so the
                  // save directory is always a valid directory path.
                  const romFileDir = romPath.replace(/[/\\][^/\\]+$/, '') || romPath;
                  const saveDir = getSaveDirectory() || romDir || romFileDir;
                  fetch(`${LAUNCH_API_BASE}/api/launch`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                      romPath,
                      system: room?.system ?? '',
                      backendId,
                      saveDirectory: saveDir,
                      playerSlot: mySlot,
                      netplayHost: relayHost,
                      netplayPort: relayPort,
                      sessionToken: msg.sessionToken,
                    }),
                  }).catch(() => {
                    // Launch errors are non-fatal — the user can still start their
                    // emulator manually using the relay endpoint shown in the UI.
                  });
                }
              }
            }
            break;
          }

          case 'chat-broadcast':
            setChatMessages((prev) => [
              ...prev,
              {
                roomId: msg.roomId,
                userId: msg.userId,
                displayName: msg.displayName,
                content: msg.content,
                sentAt: msg.sentAt,
              },
            ]);
            break;

          case 'pong': {
            // Use serverAt to get a more accurate half-RTT estimate:
            // elapsed = (now - sentAt), half_rtt ≈ (now - serverAt)
            // We report the full round-trip but clamp to avoid negative values
            // caused by minor clock drift.
            const rtt = Math.max(0, Date.now() - msg.sentAt);
            setLatencyMs(rtt);
            break;
          }

          case 'presence-update':
            setOnlinePlayers(msg.players);
            break;

          case 'friend-request-received':
            setPendingFriendRequests((prev) => [
              ...prev.filter((r) => r.requestId !== msg.requestId),
              { requestId: msg.requestId, fromId: msg.fromId, fromDisplayName: msg.fromDisplayName },
            ]);
            addToast({
              message: `Friend Request from ${msg.fromDisplayName}`,
              detail: 'Check Friends to accept or decline.',
              icon: '👥',
              duration: 8000,
            });
            break;

          case 'friend-request-accepted':
            setPendingFriendRequests((prev) =>
              prev.filter((r) => r.requestId !== msg.requestId)
            );
            addToast({
              message: `${msg.byDisplayName} accepted your friend request`,
              icon: '🤝',
              duration: 5000,
            });
            break;

          case 'friend-request-declined':
            setPendingFriendRequests((prev) =>
              prev.filter((r) => r.requestId !== msg.requestId)
            );
            break;

          case 'friend-status-update':
            setFriendStatuses((prev) => [
              ...prev.filter((s) => s.friendId !== msg.friendId),
              { friendId: msg.friendId, status: msg.status, roomCode: msg.roomCode, gameTitle: msg.gameTitle },
            ]);
            break;

          case 'achievement-unlocked':
            addToast({
              message: `Achievement Unlocked: ${msg.name}`,
              detail: msg.description,
              icon: msg.icon,
              duration: 6000,
            });
            break;

          case 'dm-received':
            setIncomingDms((prev) => [...prev, msg.message]);
            setUnreadDmCount((prev) => prev + 1);
            addToast({
              message: `💬 Message from ${msg.message.fromPlayer}`,
              detail: msg.message.content.slice(0, 80),
              duration: 5000,
            });
            break;

          case 'dm-read-ack':
            // Sender's read-ack doesn't affect receiver unread count
            break;

          case 'error':
            setError(msg.message);
            break;
        }
      };

      ws.onclose = () => {
        if (unmounted) return;
        setConnectionState('disconnected');
        setPlayerId(null);
        setLatencyMs(null);
        // Clear room state so reconnecting clients start fresh and the
        // auto-join effect on LobbyPage can re-trigger correctly.
        setCurrentRoom(null);
        setRelayInfo(null);
        setSessionToken(null);
        setOwnerToken(null);
        setOnlinePlayers([]);
        setChatMessages([]);
        setPendingFriendRequests([]);
        setFriendStatuses([]);
        setIncomingDms([]);
        setUnreadDmCount(0);
        stopPing();
        // Attempt to reconnect after 3 seconds
        reconnectTimeout = setTimeout(connect, RECONNECT_DELAY_MS);
      };

      ws.onerror = () => {
        if (unmounted) return;
        setConnectionState('error');
      };
    }

    connect();

    return () => {
      unmounted = true;
      if (reconnectTimeout) clearTimeout(reconnectTimeout);
      stopPing();
      ws?.close();
    };
  }, []);

  const createRoom = useCallback(
    (payload: Omit<CreateRoomPayload, 'displayName'>, displayName: string) => {
      send({
        type: 'create-room',
        payload: { ...payload, displayName },
      });
    },
    [send]
  );

  const joinByCode = useCallback(
    (roomCode: string, displayName: string) => {
      send({ type: 'join-room', payload: { roomCode, displayName } });
    },
    [send]
  );

  const joinById = useCallback(
    (roomId: string, displayName: string) => {
      send({ type: 'join-room', payload: { roomId, displayName } });
    },
    [send]
  );

  const joinAsSpectator = useCallback(
    (payload: Omit<JoinRoomPayload, 'displayName'>, displayName: string) => {
      send({ type: 'join-as-spectator', payload: { ...payload, displayName } });
    },
    [send]
  );

  const leaveRoom = useCallback(() => {
    const room = currentRoomRef.current;
    if (room) {
      send({ type: 'leave-room', payload: { roomId: room.id } });
      setCurrentRoom(null);
      setRelayInfo(null);
      setSessionToken(null);
      setChatMessages([]);
    }
  }, [send]);

  const toggleReady = useCallback(() => {
    const room = currentRoomRef.current;
    if (room) {
      send({ type: 'toggle-ready', payload: { roomId: room.id } });
    }
  }, [send]);

  const startGame = useCallback(() => {
    const room = currentRoomRef.current;
    if (room) {
      send({ type: 'start-game', payload: { roomId: room.id } });
    }
  }, [send]);

  const sendChat = useCallback(
    (content: string) => {
      const room = currentRoomRef.current;
      if (room) {
        send({ type: 'chat', payload: { roomId: room.id, content } });
      }
    },
    [send]
  );

  const listRooms = useCallback(() => {
    send({ type: 'list-rooms' });
  }, [send]);

  const clearError = useCallback(() => setError(null), []);

  const registerIdentity = useCallback(
    (displayName: string, identityToken?: string) => {
      send({ type: 'register-identity', payload: { displayName, identityToken } });
    },
    [send]
  );

  const sendFriendRequest = useCallback(
    (toPlayerId: string) => {
      send({ type: 'friend-request', payload: { toPlayerId } });
    },
    [send]
  );

  const acceptFriendRequest = useCallback(
    (requestId: string) => {
      send({ type: 'friend-request-accept', payload: { requestId } });
      setPendingFriendRequests((prev) => prev.filter((r) => r.requestId !== requestId));
    },
    [send]
  );

  const declineFriendRequest = useCallback(
    (requestId: string) => {
      send({ type: 'friend-request-decline', payload: { requestId } });
      setPendingFriendRequests((prev) => prev.filter((r) => r.requestId !== requestId));
    },
    [send]
  );

  const removeFriend = useCallback(
    (friendId: string) => {
      send({ type: 'friend-remove', payload: { friendId } });
    },
    [send]
  );

  const sendDm = useCallback(
    (toPlayer: string, content: string) => {
      send({ type: 'send-dm', payload: { toPlayer, content } });
    },
    [send]
  );

  const markDmRead = useCallback(
    (fromPlayer: string) => {
      send({ type: 'mark-dm-read', payload: { fromPlayer } });
      setIncomingDms((prev) => prev.filter((dm) => dm.fromPlayer !== fromPlayer));
      setUnreadDmCount((prev) => {
        const cleared = incomingDms.filter((dm) => dm.fromPlayer === fromPlayer).length;
        return Math.max(0, prev - cleared);
      });
    },
    [send, incomingDms]
  );

  return (
    <LobbyContext.Provider
      value={{
        connectionState,
        playerId,
        currentRoom,
        publicRooms,
        chatMessages,
        error,
        latencyMs,
        relayInfo,
        sessionToken,
        ownerToken,
        onlinePlayers,
        ws: wsRef.current,
        pendingFriendRequests,
        friendStatuses,
        createRoom,
        joinByCode,
        joinById,
        joinAsSpectator,
        leaveRoom,
        toggleReady,
        startGame,
        sendChat,
        listRooms,
        clearError,
        registerIdentity,
        sendFriendRequest,
        acceptFriendRequest,
        declineFriendRequest,
        removeFriend,
        sendDm,
        markDmRead,
        unreadDmCount,
        incomingDms,
      }}
    >
      {children}
    </LobbyContext.Provider>
  );
}

export function useLobby(): LobbyContextValue {
  const ctx = useContext(LobbyContext);
  if (!ctx) throw new Error('useLobby must be used within a LobbyProvider');
  return ctx;
}
