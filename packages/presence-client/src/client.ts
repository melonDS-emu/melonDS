export type OnlineStatus = 'online' | 'in-game' | 'idle' | 'offline';

export interface FriendInfo {
  userId: string;
  displayName: string;
  avatarUrl?: string;
  status: OnlineStatus;
  currentGameId?: string;
  currentGameTitle?: string;
  currentLobbyId?: string;
  isJoinable: boolean;
  lastSeenAt: string;
}

export interface PresenceState {
  userId: string;
  status: OnlineStatus;
  currentGameId?: string;
  currentLobbyId?: string;
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
}
