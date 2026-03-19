/**
 * Phase 17 — ROM Intelligence, Global Chat & Notification Center
 *
 * Tests for: GlobalChatStore, NotificationStore
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { GlobalChatStore } from '../global-chat-store';
import { NotificationStore } from '../notification-store';

// ---------------------------------------------------------------------------
// GlobalChatStore
// ---------------------------------------------------------------------------

describe('GlobalChatStore', () => {
  let store: GlobalChatStore;

  beforeEach(() => {
    store = new GlobalChatStore();
  });

  it('posts a message and returns it', () => {
    const msg = store.post('player-1', 'Alice', 'Hello world');
    expect(msg.playerId).toBe('player-1');
    expect(msg.displayName).toBe('Alice');
    expect(msg.text).toBe('Hello world');
    expect(msg.id).toBeTruthy();
    expect(msg.timestamp).toBeTruthy();
  });

  it('getRecent returns messages in posting order', () => {
    store.post('p1', 'Alice', 'first');
    store.post('p2', 'Bob', 'second');
    store.post('p3', 'Carol', 'third');
    const recent = store.getRecent();
    expect(recent).toHaveLength(3);
    expect(recent[0].text).toBe('first');
    expect(recent[2].text).toBe('third');
  });

  it('getRecent respects the limit parameter', () => {
    for (let i = 0; i < 10; i++) {
      store.post('p', 'Player', `msg ${i}`);
    }
    const recent = store.getRecent(3);
    expect(recent).toHaveLength(3);
    // Should be the last 3 messages
    expect(recent[0].text).toBe('msg 7');
    expect(recent[2].text).toBe('msg 9');
  });

  it('evicts oldest messages when maxMessages is exceeded', () => {
    const small = new GlobalChatStore(3);
    small.post('p', 'P', 'a');
    small.post('p', 'P', 'b');
    small.post('p', 'P', 'c');
    small.post('p', 'P', 'd'); // should evict 'a'
    const msgs = small.getRecent(100);
    expect(msgs).toHaveLength(3);
    expect(msgs.map((m) => m.text)).toEqual(['b', 'c', 'd']);
  });

  it('clear removes all messages', () => {
    store.post('p1', 'Alice', 'hello');
    store.post('p2', 'Bob', 'hi');
    store.clear();
    expect(store.getRecent()).toHaveLength(0);
  });

  it('getRecent with default 500 maxMessages retains all 500', () => {
    const big = new GlobalChatStore(500);
    for (let i = 0; i < 500; i++) {
      big.post('p', 'P', `msg ${i}`);
    }
    expect(big.getRecent(500)).toHaveLength(500);
  });
});

// ---------------------------------------------------------------------------
// NotificationStore
// ---------------------------------------------------------------------------

describe('NotificationStore', () => {
  let store: NotificationStore;

  beforeEach(() => {
    store = new NotificationStore();
  });

  it('adds a notification and returns it', () => {
    const n = store.add('player-1', 'achievement-unlocked', 'First Win!', 'You won your first game.', 'ach-1');
    expect(n.playerId).toBe('player-1');
    expect(n.type).toBe('achievement-unlocked');
    expect(n.title).toBe('First Win!');
    expect(n.body).toBe('You won your first game.');
    expect(n.entityId).toBe('ach-1');
    expect(n.read).toBe(false);
    expect(n.id).toBeTruthy();
    expect(n.createdAt).toBeTruthy();
  });

  it('list returns notifications newest-first for a player', () => {
    store.add('p1', 'dm-received', 'Message', 'Hello');
    store.add('p1', 'friend-request', 'Friend Request', 'Alice wants to add you');
    const list = store.list('p1');
    expect(list).toHaveLength(2);
    // newest first: friend-request was added second
    expect(list[0].type).toBe('friend-request');
    expect(list[1].type).toBe('dm-received');
  });

  it('list returns empty array for unknown player', () => {
    expect(store.list('nobody')).toHaveLength(0);
  });

  it('list is isolated per player', () => {
    store.add('p1', 'dm-received', 'msg', 'hi');
    store.add('p2', 'friend-request', 'req', 'from p1');
    expect(store.list('p1')).toHaveLength(1);
    expect(store.list('p2')).toHaveLength(1);
  });

  it('markRead marks a single notification as read and returns true', () => {
    const n = store.add('p1', 'dm-received', 'msg', 'hi');
    const ok = store.markRead(n.id);
    expect(ok).toBe(true);
    expect(store.list('p1')[0].read).toBe(true);
  });

  it('markRead returns false for unknown ID', () => {
    expect(store.markRead('non-existent-id')).toBe(false);
  });

  it('markAllRead marks all notifications for a player', () => {
    store.add('p1', 'dm-received', 'a', 'body');
    store.add('p1', 'friend-request', 'b', 'body');
    const count = store.markAllRead('p1');
    expect(count).toBe(2);
    expect(store.list('p1').every((n) => n.read)).toBe(true);
  });

  it('markAllRead returns 0 for player with no notifications', () => {
    expect(store.markAllRead('nobody')).toBe(0);
  });

  it('markAllRead returns 0 when all already read', () => {
    const n = store.add('p1', 'dm-received', 'a', 'body');
    store.markRead(n.id);
    expect(store.markAllRead('p1')).toBe(0);
  });

  it('unreadCount returns correct count', () => {
    store.add('p1', 'dm-received', 'a', 'body');
    store.add('p1', 'friend-request', 'b', 'body');
    expect(store.unreadCount('p1')).toBe(2);
    store.markAllRead('p1');
    expect(store.unreadCount('p1')).toBe(0);
  });

  it('unreadCount returns 0 for unknown player', () => {
    expect(store.unreadCount('nobody')).toBe(0);
  });

  it('does not exceed MAX_PER_PLAYER (200)', () => {
    for (let i = 0; i < 210; i++) {
      store.add('p1', 'dm-received', `title ${i}`, 'body');
    }
    const list = store.list('p1');
    expect(list.length).toBeLessThanOrEqual(200);
    // Most recent should still be present
    expect(list[0].title).toBe('title 209');
  });
});
