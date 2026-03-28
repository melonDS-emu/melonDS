export type OnlineStatus = 'online' | 'in-game' | 'idle' | 'offline';

export interface FriendInfo {
  userId: string;
  displayName: string;
  avatarUrl?: string;
  status: OnlineStatus;
  currentGameId?: string;
  currentGameTitle?: string;
  currentLobbyId?: string;
  /** Room code players can use to join this friend's lobby. */
  roomCode?: string;
  isJoinable: boolean;
  lastSeenAt: string;
}

export interface PresenceState {
  userId: string;
  status: OnlineStatus;
  currentGameId?: string;
  currentLobbyId?: string;
}

export type ActivityEventType = 'joined' | 'started' | 'finished' | 'online';

export interface RecentActivity {
  id: string;
  type: ActivityEventType;
  userId: string;
  displayName: string;
  gameTitle?: string;
  lobbyId?: string;
  roomCode?: string;
  timestamp: string;
}

/**
 * Minimal interface for a WebSocket-like object used by PresenceClient.
 * Compatible with both the browser WebSocket API and the 'ws' package.
 */
export interface PresenceSocket {
  readyState: number;
  send(data: string): void;
  /** OPEN state constant — 1 in both browser and ws */
  readonly OPEN: number;
}

/** Envelope sent to the server when presence changes. */
interface PresenceUpdateEnvelope {
  type: 'presence-update-self';
  state: PresenceState;
}

/**
 * PresenceClient manages the user's online status and friends list.
 * Call `connect(socket)` to attach a live WebSocket so that status changes
 * are immediately broadcast to the lobby server.
 */
export class PresenceClient {
  private friends: Map<string, FriendInfo> = new Map();
  private currentState: PresenceState;
  private socket: PresenceSocket | null = null;

  constructor(userId: string) {
    this.currentState = {
      userId,
      status: 'online',
    };
  }

  /**
   * Attach a WebSocket connection so presence updates are sent to the server.
   * Pass `null` to detach.
   */
  connect(socket: PresenceSocket | null): void {
    this.socket = socket;
    if (socket) {
      // Immediately broadcast current state on connect
      this.broadcast();
    }
  }

  /**
   * Detach the WebSocket (equivalent to `connect(null)`).
   */
  disconnect(): void {
    this.socket = null;
  }

  /**
   * Update own presence status.
   */
  setStatus(status: OnlineStatus): void {
    this.currentState.status = status;
    this.broadcast();
  }

  /**
   * Set the current game being played.
   */
  setCurrentGame(gameId: string | undefined, lobbyId?: string): void {
    this.currentState.currentGameId = gameId;
    this.currentState.currentLobbyId = lobbyId;
    this.currentState.status = gameId ? 'in-game' : 'online';
    this.broadcast();
  }

  /**
   * Get the current presence state.
   */
  getState(): PresenceState {
    return { ...this.currentState };
  }

  /**
   * Get the online friends list.
   */
  getOnlineFriends(): FriendInfo[] {
    return Array.from(this.friends.values()).filter(
      (f) => f.status !== 'offline'
    );
  }

  /**
   * Get all friends.
   */
  getAllFriends(): FriendInfo[] {
    return Array.from(this.friends.values());
  }

  /**
   * Get joinable sessions from friends.
   */
  getJoinableSessions(): FriendInfo[] {
    return Array.from(this.friends.values()).filter((f) => f.isJoinable);
  }

  /**
   * Update a friend's presence (called when receiving server updates).
   */
  updateFriendPresence(friend: FriendInfo): void {
    this.friends.set(friend.userId, friend);
  }

  /**
   * Remove a friend from the list.
   */
  removeFriend(userId: string): void {
    this.friends.delete(userId);
  }

  /**
   * Seed with mock friends for development/demo purposes.
   */
  seedMockFriends(): void {
    const now = new Date();
    const mock: FriendInfo[] = [
      {
        userId: 'mock-player2',
        displayName: 'Player2',
        status: 'in-game',
        currentGameId: 'n64-mario-kart-64',
        currentGameTitle: 'Mario Kart 64',
        currentLobbyId: 'mock-lobby-mk64',
        roomCode: 'MK6401',
        isJoinable: true,
        lastSeenAt: new Date(now.getTime() - 8 * 60 * 1000).toISOString(),
      },
      {
        userId: 'mock-linkmaster',
        displayName: 'LinkMaster',
        status: 'in-game',
        currentGameId: 'n64-super-smash-bros',
        currentGameTitle: 'Super Smash Bros.',
        currentLobbyId: 'mock-lobby-ssb',
        isJoinable: false,
        lastSeenAt: new Date(now.getTime() - 25 * 60 * 1000).toISOString(),
      },
      {
        userId: 'mock-starfox99',
        displayName: 'StarFox99',
        status: 'in-game',
        currentGameId: 'n64-mario-party-2',
        currentGameTitle: 'Mario Party 2',
        currentLobbyId: 'mock-lobby-mp2',
        roomCode: 'PARTY2',
        isJoinable: true,
        lastSeenAt: new Date(now.getTime() - 3 * 60 * 1000).toISOString(),
      },
      {
        userId: 'mock-retrofan',
        displayName: 'RetroFan',
        status: 'online',
        isJoinable: false,
        lastSeenAt: new Date(now.getTime() - 2 * 60 * 1000).toISOString(),
      },
      {
        userId: 'mock-zeldaqueen',
        displayName: 'ZeldaQueen',
        status: 'idle',
        isJoinable: false,
        lastSeenAt: new Date(now.getTime() - 45 * 60 * 1000).toISOString(),
      },
      {
        userId: 'mock-bomberx',
        displayName: 'Bomber_X',
        status: 'offline',
        isJoinable: false,
        lastSeenAt: new Date(now.getTime() - 2 * 60 * 60 * 1000).toISOString(),
      },
    ];
    mock.forEach((f) => this.friends.set(f.userId, f));
  }

  /**
   * Return a mock recent-activity feed for development/demo purposes.
   * Derived from the mock friends seeded by `seedMockFriends()`.
   */
  getMockRecentActivity(): RecentActivity[] {
    const now = new Date();
    return [
      {
        id: 'act-1',
        type: 'started',
        userId: 'mock-player2',
        displayName: 'Player2',
        gameTitle: 'Mario Kart 64',
        lobbyId: 'mock-lobby-mk64',
        roomCode: 'MK6401',
        timestamp: new Date(now.getTime() - 8 * 60 * 1000).toISOString(),
      },
      {
        id: 'act-2',
        type: 'joined',
        userId: 'mock-starfox99',
        displayName: 'StarFox99',
        gameTitle: 'Mario Party 2',
        lobbyId: 'mock-lobby-mp2',
        roomCode: 'PARTY2',
        timestamp: new Date(now.getTime() - 12 * 60 * 1000).toISOString(),
      },
      {
        id: 'act-3',
        type: 'online',
        userId: 'mock-retrofan',
        displayName: 'RetroFan',
        timestamp: new Date(now.getTime() - 2 * 60 * 1000).toISOString(),
      },
      {
        id: 'act-4',
        type: 'finished',
        userId: 'mock-linkmaster',
        displayName: 'LinkMaster',
        gameTitle: 'Super Smash Bros.',
        lobbyId: 'mock-lobby-ssb',
        timestamp: new Date(now.getTime() - 30 * 60 * 1000).toISOString(),
      },
    ];
  }

  /**
   * Send the current presence state to the server via the attached socket.
   * No-ops if no socket is connected or socket is not open.
   */
  private broadcast(): void {
    if (!this.socket || this.socket.readyState !== this.socket.OPEN) return;
    const envelope: PresenceUpdateEnvelope = {
      type: 'presence-update-self',
      state: { ...this.currentState },
    };
    this.socket.send(JSON.stringify(envelope));
  }
}
