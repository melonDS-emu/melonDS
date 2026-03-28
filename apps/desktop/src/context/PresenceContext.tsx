import { createContext, useContext, useMemo, useRef, type ReactNode } from 'react';
import { PresenceClient } from '@retro-oasis/presence-client';
import type { FriendInfo, RecentActivity } from '@retro-oasis/presence-client';
import { useLobby } from './LobbyContext';

interface PresenceContextValue {
  friends: FriendInfo[];
  onlineFriends: FriendInfo[];
  joinableSessions: FriendInfo[];
  recentActivity: RecentActivity[];
}

const PresenceContext = createContext<PresenceContextValue | null>(null);

export function PresenceProvider({ children }: { children: ReactNode }) {
  // Initialise once per mount — useRef ensures the client is never recreated on re-renders.
  const clientRef = useRef<PresenceClient | null>(null);
  if (clientRef.current === null) {
    const c = new PresenceClient('local-user');
    c.seedMockFriends();
    clientRef.current = c;
  }
  const client = clientRef.current;

  const { friendStatuses } = useLobby();

  // When the server has pushed live friend status data, overlay it on top of
  // the mock friends list so the UI reflects real server state.
  const friends = useMemo<FriendInfo[]>(() => {
    const base = client.getAllFriends();
    if (friendStatuses.length === 0) return base;

    // Build a lookup of live statuses by friendId
    const liveMap = new Map(friendStatuses.map((s) => [s.friendId, s]));

    // Update existing mock friends where we have a live status
    const updated = base.map((f): FriendInfo => {
      const live = liveMap.get(f.userId);
      if (!live) return f;
      return {
        ...f,
        status: (live.status === 'offline' ? 'offline' : live.status === 'in-game' ? 'in-game' : 'online') as FriendInfo['status'],
        roomCode: live.roomCode ?? f.roomCode,
        currentGameTitle: live.gameTitle ?? f.currentGameTitle,
        isJoinable: live.status === 'in-lobby' || live.status === 'in-game',
      };
    });

    // Add any live friends not already in the mock list
    for (const live of friendStatuses) {
      if (!base.some((f) => f.userId === live.friendId)) {
        updated.push({
          userId: live.friendId,
          displayName: live.friendId, // best effort — server doesn't send name here
          status: (live.status === 'offline' ? 'offline' : live.status === 'in-game' ? 'in-game' : 'online') as FriendInfo['status'],
          roomCode: live.roomCode,
          currentGameTitle: live.gameTitle,
          isJoinable: live.status === 'in-lobby' || live.status === 'in-game',
          lastSeenAt: new Date().toISOString(),
        });
      }
    }
    return updated;
  }, [client, friendStatuses]);

  const onlineFriends = useMemo(
    () => friends.filter((f) => f.status !== 'offline'),
    [friends]
  );
  const joinableSessions = useMemo(
    () => friends.filter((f) => f.isJoinable),
    [friends]
  );
  const recentActivity: RecentActivity[] = client.getMockRecentActivity();

  const value: PresenceContextValue = { friends, onlineFriends, joinableSessions, recentActivity };

  return <PresenceContext.Provider value={value}>{children}</PresenceContext.Provider>;
}

export function usePresence(): PresenceContextValue {
  const ctx = useContext(PresenceContext);
  if (!ctx) throw new Error('usePresence must be used inside PresenceProvider');
  return ctx;
}
