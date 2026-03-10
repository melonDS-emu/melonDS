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
} from '../services/lobby-types';

const WS_URL = 'ws://localhost:8080';
const RECONNECT_DELAY_MS = 3000;

export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'error';

interface LobbyContextValue {
  connectionState: ConnectionState;
  playerId: string | null;
  currentRoom: Room | null;
  publicRooms: Room[];
  chatMessages: ChatMessage[];
  error: string | null;

  // Actions
  createRoom: (payload: Omit<CreateRoomPayload, 'displayName'>, displayName: string) => void;
  joinByCode: (roomCode: string, displayName: string) => void;
  joinById: (roomId: string, displayName: string) => void;
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

  const wsRef = useRef<WebSocket | null>(null);
  const currentRoomRef = useRef<Room | null>(null);

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
            // Game is starting — could trigger emulator launch here
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

          case 'error':
            setError(msg.message);
            break;
        }
      };

      ws.onclose = () => {
        if (unmounted) return;
        setConnectionState('disconnected');
        setPlayerId(null);
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

  const leaveRoom = useCallback(() => {
    const room = currentRoomRef.current;
    if (room) {
      send({ type: 'leave-room', payload: { roomId: room.id } });
      setCurrentRoom(null);
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
        createRoom,
        joinByCode,
        joinById,
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
