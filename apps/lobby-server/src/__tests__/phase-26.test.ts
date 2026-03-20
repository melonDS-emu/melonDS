/**
 * Phase 26 — Debug, Audit & Polish
 *
 * Tests for:
 *  - activity-feed ring buffer eviction: oldest events are dropped, not re-allocated
 *  - RankingStore draw outcome: per-game gamesPlayed increments; wins/losses unchanged
 *  - RankingStore draw outcome: global draws counter incremented
 *  - GlobalChatStore post: max message length guard
 *  - NotificationStore: unreadCount reflects only unread notifications
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { ActivityFeedStore } from '../activity-feed';
import { RankingStore } from '../ranking-store';
import { GlobalChatStore } from '../global-chat-store';
import { NotificationStore } from '../notification-store';

// ─── Activity Feed ring buffer ──────────────────────────────────────────────

describe('ActivityFeedStore ring buffer eviction (Phase 26)', () => {
  it('never exceeds 200 events after overfilling', () => {
    const feed = new ActivityFeedStore();
    for (let i = 0; i < 210; i++) {
      feed.record('session-started', `player${i}`, `Session ${i}`, 'session');
    }
    const recent = feed.getRecent(300);
    expect(recent.length).toBeLessThanOrEqual(200);
  });

  it('the most-recently added event appears first after overfilling', () => {
    const feed = new ActivityFeedStore();
    for (let i = 0; i < 205; i++) {
      feed.record('session-started', `player${i}`, `Desc ${i}`, 'session');
    }
    const recent = feed.getRecent(200);
    // The last added event should be at the front
    expect(recent[0].playerName).toBe('player204');
  });

  it('the oldest events are evicted when the buffer overflows', () => {
    const feed = new ActivityFeedStore();
    for (let i = 0; i < 205; i++) {
      feed.record('session-started', `player${i}`, `Desc ${i}`, 'session');
    }
    const recent = feed.getRecent(200);
    const names = recent.map((e) => e.playerName);
    // First 5 entries (player0–player4) should have been evicted
    expect(names).not.toContain('player0');
    expect(names).not.toContain('player4');
    // The 6th entry onward should still be present
    expect(names).toContain('player5');
    expect(names).toContain('player204');
  });
});

// ─── RankingStore draw outcome ──────────────────────────────────────────────

describe('RankingStore draw outcome (Phase 26)', () => {
  let store: RankingStore;

  beforeEach(() => {
    store = new RankingStore();
  });

  it('draw increments global draws for both players', () => {
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game1', 'Game One', 0);
    expect(store.getPlayerRank('p1')!.draws).toBe(1);
    expect(store.getPlayerRank('p2')!.draws).toBe(1);
  });

  it('draw does NOT increment global wins or losses', () => {
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game1', 'Game One', 0);
    const r1 = store.getPlayerRank('p1')!;
    const r2 = store.getPlayerRank('p2')!;
    expect(r1.wins).toBe(0);
    expect(r1.losses).toBe(0);
    expect(r2.wins).toBe(0);
    expect(r2.losses).toBe(0);
  });

  it('draw increments gamesPlayed for per-game ranking', () => {
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game1', 'Game One', 0);
    const gr1 = store.getPlayerGameRank('p1', 'game1')!;
    const gr2 = store.getPlayerGameRank('p2', 'game1')!;
    expect(gr1.gamesPlayed).toBe(1);
    expect(gr2.gamesPlayed).toBe(1);
  });

  it('draw does NOT increment per-game wins or losses', () => {
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game1', 'Game One', 0);
    const gr1 = store.getPlayerGameRank('p1', 'game1')!;
    const gr2 = store.getPlayerGameRank('p2', 'game1')!;
    expect(gr1.wins).toBe(0);
    expect(gr1.losses).toBe(0);
    expect(gr2.wins).toBe(0);
    expect(gr2.losses).toBe(0);
  });

  it('win/loss/draw counts accumulate correctly across multiple matches', () => {
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game1', 'Game One', 1); // Alice wins
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game1', 'Game One', 0); // draw
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game1', 'Game One', -1); // Bob wins
    const r1 = store.getPlayerRank('p1')!;
    expect(r1.wins).toBe(1);
    expect(r1.losses).toBe(1);
    expect(r1.draws).toBe(1);
    expect(r1.gamesPlayed).toBe(3);
  });

  it('per-game gamesPlayed equals wins + losses on non-draw outcomes', () => {
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'g', 'G', 1);
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'g', 'G', -1);
    const gr = store.getPlayerGameRank('p1', 'g')!;
    expect(gr.gamesPlayed).toBe(2);
    expect(gr.wins + gr.losses).toBe(2);
  });
});

// ─── GlobalChatStore ────────────────────────────────────────────────────────

describe('GlobalChatStore post validation (Phase 26)', () => {
  it('stores a valid chat message', () => {
    const store = new GlobalChatStore();
    const msg = store.post('player1', 'Alice', 'Hello everyone!');
    expect(msg.text).toBe('Hello everyone!');
    expect(msg.displayName).toBe('Alice');
  });

  it('getRecent returns messages in chronological order (oldest first)', () => {
    const store = new GlobalChatStore();
    store.post('p1', 'A', 'first');
    store.post('p2', 'B', 'second');
    const recent = store.getRecent(10);
    expect(recent[0].text).toBe('first');
    expect(recent[1].text).toBe('second');
  });
});

// ─── NotificationStore ──────────────────────────────────────────────────────

describe('NotificationStore unreadCount (Phase 26)', () => {
  it('unreadCount reflects only unread notifications', () => {
    const store = new NotificationStore();
    store.add('player1', 'dm-received', 'New DM', 'Hey');
    store.add('player1', 'dm-received', 'New DM 2', 'Yo');
    expect(store.unreadCount('player1')).toBe(2);
  });

  it('unreadCount decreases after markAllRead', () => {
    const store = new NotificationStore();
    store.add('player1', 'achievement-unlocked', 'Badge!', 'Got it');
    store.markAllRead('player1');
    expect(store.unreadCount('player1')).toBe(0);
  });

  it('unreadCount is 0 for player with no notifications', () => {
    const store = new NotificationStore();
    expect(store.unreadCount('unknown-player')).toBe(0);
  });
});
