/**
 * Unit tests for PresenceClient — status management, server broadcasting,
 * friend list operations, and mock data helpers.
 */

import { describe, it, expect, vi } from 'vitest';
import { PresenceClient, type PresenceSocket } from '@retro-oasis/presence-client';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Create a minimal mock socket that captures sent messages.
 * `readyState` defaults to OPEN (1).
 */
function makeMockSocket(readyState = 1): PresenceSocket & { sent: string[] } {
  const sent: string[] = [];
  return {
    readyState,
    OPEN: 1,
    send(data: string) {
      sent.push(data);
    },
    sent,
  };
}

// ---------------------------------------------------------------------------
// Basic state management
// ---------------------------------------------------------------------------

describe('PresenceClient — initial state', () => {
  it('starts with status online', () => {
    const client = new PresenceClient('user1');
    expect(client.getState().status).toBe('online');
  });

  it('stores the userId', () => {
    const client = new PresenceClient('user-abc');
    expect(client.getState().userId).toBe('user-abc');
  });

  it('has no friends initially', () => {
    const client = new PresenceClient('u');
    expect(client.getAllFriends()).toHaveLength(0);
    expect(client.getOnlineFriends()).toHaveLength(0);
    expect(client.getJoinableSessions()).toHaveLength(0);
  });
});

// ---------------------------------------------------------------------------
// setStatus
// ---------------------------------------------------------------------------

describe('PresenceClient — setStatus', () => {
  it('updates the status field', () => {
    const client = new PresenceClient('u');
    client.setStatus('idle');
    expect(client.getState().status).toBe('idle');
  });

  it('broadcasts when a socket is connected', () => {
    const socket = makeMockSocket();
    const client = new PresenceClient('u');

    // connect() sends an initial broadcast
    client.connect(socket);
    const countAfterConnect = socket.sent.length;
    expect(countAfterConnect).toBe(1);

    client.setStatus('idle');
    expect(socket.sent).toHaveLength(countAfterConnect + 1);

    const msg = JSON.parse(socket.sent[socket.sent.length - 1]);
    expect(msg.type).toBe('presence-update-self');
    expect(msg.state.status).toBe('idle');
  });

  it('does not throw when no socket is attached', () => {
    const client = new PresenceClient('u');
    expect(() => client.setStatus('offline')).not.toThrow();
  });

  it('does not broadcast when socket is not open', () => {
    const socket = makeMockSocket(0); // CONNECTING state
    const client = new PresenceClient('u');
    client.connect(socket);
    client.setStatus('idle');
    // Socket is not open, so no messages should be sent
    expect(socket.sent).toHaveLength(0);
  });
});

// ---------------------------------------------------------------------------
// setCurrentGame
// ---------------------------------------------------------------------------

describe('PresenceClient — setCurrentGame', () => {
  it('sets status to in-game when a gameId is provided', () => {
    const client = new PresenceClient('u');
    client.setCurrentGame('mk64', 'lobby-1');
    const state = client.getState();
    expect(state.status).toBe('in-game');
    expect(state.currentGameId).toBe('mk64');
    expect(state.currentLobbyId).toBe('lobby-1');
  });

  it('resets to online when gameId is undefined', () => {
    const client = new PresenceClient('u');
    client.setCurrentGame('mk64');
    client.setCurrentGame(undefined);
    const state = client.getState();
    expect(state.status).toBe('online');
    expect(state.currentGameId).toBeUndefined();
  });

  it('broadcasts the updated state when connected', () => {
    const socket = makeMockSocket();
    const client = new PresenceClient('u');
    client.connect(socket);
    const before = socket.sent.length;

    client.setCurrentGame('ssb', 'lobby-2');
    expect(socket.sent.length).toBe(before + 1);

    const msg = JSON.parse(socket.sent[socket.sent.length - 1]);
    expect(msg.state.status).toBe('in-game');
    expect(msg.state.currentGameId).toBe('ssb');
  });
});

// ---------------------------------------------------------------------------
// connect / disconnect
// ---------------------------------------------------------------------------

describe('PresenceClient — connect / disconnect', () => {
  it('sends an initial broadcast on connect', () => {
    const socket = makeMockSocket();
    const client = new PresenceClient('u');
    client.connect(socket);
    expect(socket.sent).toHaveLength(1);
    const msg = JSON.parse(socket.sent[0]);
    expect(msg.type).toBe('presence-update-self');
    expect(msg.state.userId).toBe('u');
  });

  it('stops broadcasting after disconnect', () => {
    const socket = makeMockSocket();
    const client = new PresenceClient('u');
    client.connect(socket);
    client.disconnect();
    const countBefore = socket.sent.length;

    client.setStatus('idle');
    expect(socket.sent.length).toBe(countBefore); // no new messages
  });

  it('connect(null) is equivalent to disconnect', () => {
    const socket = makeMockSocket();
    const client = new PresenceClient('u');
    client.connect(socket);
    client.connect(null);
    const countBefore = socket.sent.length;
    client.setStatus('idle');
    expect(socket.sent.length).toBe(countBefore);
  });

  it('switches to a new socket seamlessly', () => {
    const s1 = makeMockSocket();
    const s2 = makeMockSocket();
    const client = new PresenceClient('u');

    client.connect(s1);
    client.connect(s2);

    client.setStatus('idle');
    // Only s2 should receive the update
    expect(s2.sent.length).toBeGreaterThan(s1.sent.length);
  });
});

// ---------------------------------------------------------------------------
// Friend list management
// ---------------------------------------------------------------------------

describe('PresenceClient — friend management', () => {
  it('updateFriendPresence adds a friend', () => {
    const client = new PresenceClient('u');
    client.updateFriendPresence({
      userId: 'friend-1',
      displayName: 'Pal',
      status: 'online',
      isJoinable: false,
      lastSeenAt: new Date().toISOString(),
    });
    expect(client.getAllFriends()).toHaveLength(1);
  });

  it('getOnlineFriends excludes offline friends', () => {
    const client = new PresenceClient('u');
    client.updateFriendPresence({ userId: 'f1', displayName: 'A', status: 'online', isJoinable: false, lastSeenAt: '' });
    client.updateFriendPresence({ userId: 'f2', displayName: 'B', status: 'offline', isJoinable: false, lastSeenAt: '' });
    expect(client.getOnlineFriends()).toHaveLength(1);
    expect(client.getOnlineFriends()[0].userId).toBe('f1');
  });

  it('getJoinableSessions returns only joinable friends', () => {
    const client = new PresenceClient('u');
    client.updateFriendPresence({ userId: 'f1', displayName: 'A', status: 'in-game', isJoinable: true, lastSeenAt: '' });
    client.updateFriendPresence({ userId: 'f2', displayName: 'B', status: 'in-game', isJoinable: false, lastSeenAt: '' });
    const joinable = client.getJoinableSessions();
    expect(joinable).toHaveLength(1);
    expect(joinable[0].userId).toBe('f1');
  });

  it('removeFriend removes the friend', () => {
    const client = new PresenceClient('u');
    client.updateFriendPresence({ userId: 'f1', displayName: 'A', status: 'online', isJoinable: false, lastSeenAt: '' });
    client.removeFriend('f1');
    expect(client.getAllFriends()).toHaveLength(0);
  });
});

// ---------------------------------------------------------------------------
// Mock data helpers
// ---------------------------------------------------------------------------

describe('PresenceClient — seedMockFriends / getMockRecentActivity', () => {
  it('seedMockFriends populates 6 friends', () => {
    const client = new PresenceClient('me');
    client.seedMockFriends();
    expect(client.getAllFriends()).toHaveLength(6);
  });

  it('getMockRecentActivity returns 4 events', () => {
    const client = new PresenceClient('me');
    const activity = client.getMockRecentActivity();
    expect(activity).toHaveLength(4);
    expect(activity.every((a) => typeof a.id === 'string')).toBe(true);
  });

  it('broadcast envelope contains userId from constructor', () => {
    const socket = makeMockSocket();
    const client = new PresenceClient('my-user-id');
    client.connect(socket);

    const msg = JSON.parse(socket.sent[0]);
    expect(msg.state.userId).toBe('my-user-id');
  });
});
