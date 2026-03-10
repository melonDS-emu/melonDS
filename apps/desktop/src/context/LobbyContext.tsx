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

const WS_URL = 'ws://localhost:8080';
const RECONNECT_DELAY_MS = 3000;
const PING_INTERVAL_MS = 10_000;

export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'error';

export interface RelayInfo {
  port: number;
  host: string;
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
}

const LobbyContext = createContext<LobbyContextValue | null>(null);

export function LobbyProvider({ children }: { children: ReactNode }) {
  const [connectionState, setConnectionState] = useState<ConnectionState>('disconnected');
  const [playerId, setPlayerId] = useState<string | null>(null);
  const [currentRoom, setCurrentRoom] = useState<Room | null>(null);
  const [publicRooms, setPublicRooms] = useState<Room[]>([]);
  const [chatMessages, setChatMessages] = useState<ChatMessage[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [latencyMs, setLatencyMs] = useState<number | null>(null);
  const [relayInfo, setRelayInfo] = useState<RelayInfo | null>(null);

  const wsRef = useRef<WebSocket | null>(null);
  const currentRoomRef = useRef<Room | null>(null);
  const pingTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);

  // Keep ref in sync with state for use in closures
  currentRoomRef.current = currentRoom;

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

          case 'game-starting':
            if (msg.relayPort) {
              setRelayInfo({
                port: msg.relayPort,
                host: msg.relayHost ?? 'localhost',
              });
            }
            // Mark room as in-game so clients can show the correct status
            // even if they reload or reconnect.
            setCurrentRoom((prev) =>
              prev?.id === msg.roomId
                ? { ...prev, status: 'in-game', relayPort: msg.relayPort }
                : prev
            );
            break;

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
        setChatMessages([]);
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
