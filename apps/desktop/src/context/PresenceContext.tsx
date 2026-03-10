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

function buildMockActivity(friends: FriendInfo[]): RecentActivity[] {
  const now = new Date();
  const player2 = friends.find((f) => f.userId === 'mock-player2');
  const starfox = friends.find((f) => f.userId === 'mock-starfox99');
  const linkmaster = friends.find((f) => f.userId === 'mock-linkmaster');

  const items: RecentActivity[] = [
    {
      id: 'act-1',
      type: 'started',
      userId: starfox?.userId ?? 'mock-starfox99',
      displayName: starfox?.displayName ?? 'StarFox99',
      gameTitle: starfox?.currentGameTitle ?? 'Mario Party 2',
      lobbyId: starfox?.currentLobbyId,
      roomCode: starfox?.roomCode,
      timestamp: new Date(now.getTime() - 3 * 60 * 1000).toISOString(),
    },
    {
      id: 'act-2',
      type: 'joined',
      userId: player2?.userId ?? 'mock-player2',
      displayName: player2?.displayName ?? 'Player2',
      gameTitle: player2?.currentGameTitle ?? 'Mario Kart 64',
      lobbyId: player2?.currentLobbyId,
      roomCode: player2?.roomCode,
      timestamp: new Date(now.getTime() - 8 * 60 * 1000).toISOString(),
    },
    {
      id: 'act-3',
      type: 'started',
      userId: linkmaster?.userId ?? 'mock-linkmaster',
      displayName: linkmaster?.displayName ?? 'LinkMaster',
      gameTitle: linkmaster?.currentGameTitle ?? 'Super Smash Bros.',
      lobbyId: linkmaster?.currentLobbyId,
      timestamp: new Date(now.getTime() - 25 * 60 * 1000).toISOString(),
    },
    {
      id: 'act-4',
      type: 'finished',
      userId: 'mock-bomberx',
      displayName: 'Bomber_X',
      gameTitle: 'Super Bomberman',
      timestamp: new Date(now.getTime() - 2 * 60 * 60 * 1000).toISOString(),
    },
    {
      id: 'act-5',
      type: 'online',
      userId: 'mock-zeldaqueen',
      displayName: 'ZeldaQueen',
      timestamp: new Date(now.getTime() - 45 * 60 * 1000).toISOString(),
    },
  ];
  return items;
}

export function PresenceProvider({ children }: { children: ReactNode }) {
  // Initialise once per mount — useRef ensures the client is never recreated on re-renders.
  const clientRef = useRef<PresenceClient | null>(null);
  if (clientRef.current === null) {
    const client = new PresenceClient('local-user');
    client.seedMockFriends();
    clientRef.current = client;
  }
  const client = clientRef.current;

  const friends = client.getAllFriends();
  const onlineFriends = client.getOnlineFriends();
  const joinableSessions = client.getJoinableSessions();
  const recentActivity = buildMockActivity(friends);

  const value: PresenceContextValue = { friends, onlineFriends, joinableSessions, recentActivity };

  return <PresenceContext.Provider value={value}>{children}</PresenceContext.Provider>;
}

export function usePresence(): PresenceContextValue {
  const ctx = useContext(PresenceContext);
  if (!ctx) throw new Error('usePresence must be used inside PresenceProvider');
  return ctx;
}
