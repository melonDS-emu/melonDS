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
 * PresenceClient manages the user's online status and friends list.
 * In production, this would connect to the lobby server via WebSocket.
 */
export class PresenceClient {
  private friends: Map<string, FriendInfo> = new Map();
  private currentState: PresenceState;

  constructor(userId: string) {
    this.currentState = {
      userId,
      status: 'online',
    };
  }

  /**
   * Update own presence status.
   */
  setStatus(status: OnlineStatus): void {
    this.currentState.status = status;
    // TODO: Broadcast to server
  }

  /**
   * Set the current game being played.
   */
  setCurrentGame(gameId: string | undefined, lobbyId?: string): void {
    this.currentState.currentGameId = gameId;
    this.currentState.currentLobbyId = lobbyId;
    this.currentState.status = gameId ? 'in-game' : 'online';
    // TODO: Broadcast to server
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
}
