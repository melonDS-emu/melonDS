export type OnlineStatus = 'online' | 'in-game' | 'idle' | 'offline';

export interface FriendPresence {
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

export interface InviteNotification {
  id: string;
  fromUserId: string;
  fromDisplayName: string;
  lobbyId: string;
  gameTitle: string;
  roomCode: string;
  sentAt: string;
  expiresAt: string;
  status: 'pending' | 'accepted' | 'declined' | 'expired';
}

export interface ChatMessage {
  id: string;
  lobbyId: string;
  userId: string;
  displayName: string;
  content: string;
  sentAt: string;
}
