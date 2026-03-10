import { createContext, useContext, useRef, type ReactNode } from 'react';
import { PresenceClient } from '@retro-oasis/presence-client';
import type { FriendInfo, RecentActivity } from '@retro-oasis/presence-client';

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
    clientRef.current = new PresenceClient('local-user');
  }
  const client = clientRef.current;

  const friends = client.getAllFriends();
  const onlineFriends = client.getOnlineFriends();
  const joinableSessions = client.getJoinableSessions();
  const recentActivity: RecentActivity[] = [];

  const value: PresenceContextValue = { friends, onlineFriends, joinableSessions, recentActivity };

  return <PresenceContext.Provider value={value}>{children}</PresenceContext.Provider>;
}

export function usePresence(): PresenceContextValue {
  const ctx = useContext(PresenceContext);
  if (!ctx) throw new Error('usePresence must be used inside PresenceProvider');
  return ctx;
}
