/**
 * Phase 6 — Presence, Invites & Social Glue
 *
 * Tests cover:
 *  - RecentPlayersStore (in-memory)
 *  - PlayerPrivacyStore (in-memory)
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { RecentPlayersStore } from '../recent-players-store';
import { PlayerPrivacyStore } from '../player-privacy-store';

// ---------------------------------------------------------------------------
// RecentPlayersStore — in-memory
// ---------------------------------------------------------------------------

describe('RecentPlayersStore (in-memory)', () => {
  let store: RecentPlayersStore;

  beforeEach(() => {
    store = new RecentPlayersStore();
  });

  it('records a recent player entry', () => {
    const entry = store.record('alice', 'bob', 'Bob', 'ROOM01', 'Mario Kart 64');
    expect(entry.ownerId).toBe('alice');
    expect(entry.metPlayerId).toBe('bob');
    expect(entry.metDisplayName).toBe('Bob');
    expect(entry.roomCode).toBe('ROOM01');
    expect(entry.gameTitle).toBe('Mario Kart 64');
  });

  it('deduplicates entries for the same session/player pair', () => {
    store.record('alice', 'bob', 'Bob', 'ROOM01', 'Mario Kart 64');
    store.record('alice', 'bob', 'Bob', 'ROOM01', 'Mario Kart 64');
    expect(store.getRecent('alice')).toHaveLength(1);
  });

  it('returns newest entries first', () => {
    store.record('alice', 'bob', 'Bob', 'ROOM01');
    store.record('alice', 'carol', 'Carol', 'ROOM02');
    const recent = store.getRecent('alice');
    expect(recent[0].metPlayerId).toBe('carol');
    expect(recent[1].metPlayerId).toBe('bob');
  });

  it('respects the limit parameter', () => {
    for (let i = 0; i < 10; i++) {
      store.record('alice', `player${i}`, `Player${i}`, `ROOM0${i}`);
    }
    expect(store.getRecent('alice', 3)).toHaveLength(3);
  });

  it('returns empty list for unknown owner', () => {
    expect(store.getRecent('nobody')).toEqual([]);
  });

  it('removes a single entry by ID', () => {
    const e1 = store.record('alice', 'bob', 'Bob', 'ROOM01');
    store.record('alice', 'carol', 'Carol', 'ROOM02');
    const removed = store.remove('alice', e1.id);
    expect(removed).toBe(true);
    const remaining = store.getRecent('alice');
    expect(remaining.every((e) => e.id !== e1.id)).toBe(true);
  });

  it('returns false when removing a non-existent entry', () => {
    expect(store.remove('alice', 'no-such-id')).toBe(false);
  });

  it('clears all entries for an owner', () => {
    store.record('alice', 'bob', 'Bob', 'ROOM01');
    store.clear('alice');
    expect(store.getRecent('alice')).toHaveLength(0);
  });

  it('does not bleed entries between owners', () => {
    store.record('alice', 'carol', 'Carol', 'ROOM01');
    store.record('bob', 'carol', 'Carol', 'ROOM01');
    expect(store.getRecent('alice')).toHaveLength(1);
    expect(store.getRecent('bob')).toHaveLength(1);
  });

  it('records entry without gameTitle (optional field)', () => {
    const entry = store.record('alice', 'dave', 'Dave', 'ROOM03');
    expect(entry.gameTitle).toBeUndefined();
  });

  it('updates playedAt on deduplicated entries', () => {
    const e1 = store.record('alice', 'bob', 'Bob', 'ROOM01');
    const e2 = store.record('alice', 'bob', 'Bob', 'ROOM01', 'Updated');
    // e2.playedAt should be >= e1.playedAt (ISO strings sort lexicographically)
    expect(e2.playedAt >= e1.playedAt).toBe(true);
    expect(e2.gameTitle).toBe('Updated');
  });
});

// ---------------------------------------------------------------------------
// PlayerPrivacyStore — in-memory
// ---------------------------------------------------------------------------

describe('PlayerPrivacyStore (in-memory)', () => {
  let store: PlayerPrivacyStore;

  beforeEach(() => {
    store = new PlayerPrivacyStore();
  });

  it('returns defaults for a new player', () => {
    const s = store.get('alice');
    expect(s.showOnline).toBe(true);
    expect(s.allowInvites).toBe(true);
    expect(s.showActivity).toBe(true);
  });

  it('updates settings partially', () => {
    const updated = store.update('alice', { showOnline: false });
    expect(updated.showOnline).toBe(false);
    expect(updated.allowInvites).toBe(true); // unchanged
  });

  it('persists updates across get calls', () => {
    store.update('alice', { allowInvites: false });
    expect(store.get('alice').allowInvites).toBe(false);
  });

  it('does not bleed between players', () => {
    store.update('alice', { showOnline: false });
    expect(store.get('bob').showOnline).toBe(true);
  });

  it('supports updating all three fields at once', () => {
    const s = store.update('alice', { showOnline: false, allowInvites: false, showActivity: false });
    expect(s.showOnline).toBe(false);
    expect(s.allowInvites).toBe(false);
    expect(s.showActivity).toBe(false);
  });

  it('updates playerId correctly', () => {
    const s = store.update('alice', {});
    expect(s.playerId).toBe('alice');
  });

  it('records an updatedAt timestamp', () => {
    const before = Date.now();
    const s = store.update('alice', { showOnline: false });
    const after = Date.now();
    const ts = new Date(s.updatedAt).getTime();
    expect(ts).toBeGreaterThanOrEqual(before);
    expect(ts).toBeLessThanOrEqual(after);
  });

  it('subsequent updates replace previous values', () => {
    store.update('alice', { showOnline: false });
    store.update('alice', { showOnline: true });
    expect(store.get('alice').showOnline).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Integration: privacy guard for invites
// ---------------------------------------------------------------------------

describe('Phase 6 privacy guard logic', () => {
  it('blocks invite when allowInvites is false', () => {
    const privacy = new PlayerPrivacyStore();
    privacy.update('bob', { allowInvites: false });
    const bobPrivacy = privacy.get('bob');
    expect(bobPrivacy.allowInvites).toBe(false);
    // Simulate the guard: if !allowInvites and not friends → block
    const isFriend = false;
    const shouldBlock = !bobPrivacy.allowInvites && !isFriend;
    expect(shouldBlock).toBe(true);
  });

  it('allows invite when allowInvites is true', () => {
    const privacy = new PlayerPrivacyStore();
    const bobPrivacy = privacy.get('bob');
    expect(bobPrivacy.allowInvites).toBe(true);
    const shouldBlock = !bobPrivacy.allowInvites;
    expect(shouldBlock).toBe(false);
  });

  it('allows invite from friend even when allowInvites is false', () => {
    const privacy = new PlayerPrivacyStore();
    privacy.update('bob', { allowInvites: false });
    const bobPrivacy = privacy.get('bob');
    const isFriend = true;
    const shouldBlock = !bobPrivacy.allowInvites && !isFriend;
    expect(shouldBlock).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// Integration: recent players mutual recording
// ---------------------------------------------------------------------------

describe('Phase 6 recent players — mutual recording', () => {
  it('records entry for every participant pair', () => {
    const store = new RecentPlayersStore();
    const players = [
      { id: 'alice', name: 'Alice' },
      { id: 'bob', name: 'Bob' },
      { id: 'carol', name: 'Carol' },
    ];
    const roomCode = 'TESTROOM';
    const gameTitle = 'Mario Kart 64';

    // Simulate start-game recording logic from handler
    for (const player of players) {
      for (const other of players) {
        if (other.id === player.id) continue;
        store.record(player.id, other.id, other.name, roomCode, gameTitle);
      }
    }

    // Each player should have 2 recent entries (the other two)
    expect(store.getRecent('alice')).toHaveLength(2);
    expect(store.getRecent('bob')).toHaveLength(2);
    expect(store.getRecent('carol')).toHaveLength(2);
  });
});

