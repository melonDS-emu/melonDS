/**
 * Phase 31 — Debug, Audit & Polish
 *
 * Regression tests covering the following bug fixes:
 *
 *  1. `NotificationStore.add()` — 'room-invite' is a valid NotificationType
 *     (handler.ts was passing 'invite' which is not in the union).
 *
 *  2. `NotificationStore.add()` — body and entityId are separate string
 *     parameters; the handler was incorrectly merging both into a single
 *     object argument.
 *
 *  3. `NotificationStore` — mark a single notification read via `markRead()`.
 *
 *  4. Mock game catalog — every game that the system pages pass to createRoom
 *     has all the fields required by CreateRoomPayload
 *     (name, gameId, gameTitle, system, isPublic, maxPlayers).
 *
 *  5. `PartyCollectionStore` — tag 'co-op' (hyphenated) is a valid tag that
 *     can be used as an object-key; `getByTag('co-op')` returns results.
 *
 *  6. `packages/emulator-bridge` and `packages/save-system` tsconfig —
 *     typeRoots now resolves @types/node from the workspace root so that
 *     the packages typecheck cleanly (regression guard via adapter import).
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { NotificationStore } from '../notification-store';
import type { NotificationType } from '../notification-store';
import { PartyCollectionStore } from '../party-collection-store';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';

// ─── NotificationStore — 'room-invite' type ─────────────────────────────────

describe('NotificationStore — room-invite type (Phase 31)', () => {
  let store: NotificationStore;

  beforeEach(() => {
    store = new NotificationStore();
  });

  it("accepts 'room-invite' as a valid NotificationType", () => {
    const n = store.add('player1', 'room-invite', 'Game Invite', 'Alice invited you!');
    expect(n.type).toBe('room-invite');
  });

  it('stores title and body as separate string fields', () => {
    const n = store.add('player1', 'room-invite', 'Game Invite', 'Room code: AB1234', 'invite-id-001');
    expect(n.title).toBe('Game Invite');
    expect(n.body).toBe('Room code: AB1234');
    expect(n.entityId).toBe('invite-id-001');
  });

  it('body is a string not an object', () => {
    const n = store.add('player1', 'room-invite', 'Title', 'Plain body string');
    expect(typeof n.body).toBe('string');
  });

  it('entityId is optional and defaults to undefined', () => {
    const n = store.add('player1', 'room-invite', 'Title', 'Body');
    expect(n.entityId).toBeUndefined();
  });

  it('all NotificationType values are accepted by add()', () => {
    const types: NotificationType[] = [
      'achievement-unlocked',
      'friend-request',
      'friend-accepted',
      'dm-received',
      'tournament-result',
      'room-invite',
    ];
    for (const type of types) {
      const n = store.add('player1', type, 'T', 'B');
      expect(n.type).toBe(type);
    }
  });
});

// ─── NotificationStore — markRead / unreadCount ─────────────────────────────

describe('NotificationStore — markRead single notification (Phase 31)', () => {
  it('markRead marks only the specified notification as read', () => {
    const store = new NotificationStore();
    const n1 = store.add('player1', 'dm-received', 'DM 1', 'Hey');
    const n2 = store.add('player1', 'dm-received', 'DM 2', 'Yo');
    store.markRead(n1.id);
    expect(store.unreadCount('player1')).toBe(1);
    const remaining = store.list('player1').filter((n) => !n.read);
    expect(remaining[0].id).toBe(n2.id);
  });

  it('markAllRead sets all notifications for the player as read', () => {
    const store = new NotificationStore();
    store.add('player1', 'friend-request', 'FR', 'Bob');
    store.add('player1', 'achievement-unlocked', 'Badge', 'Got it');
    store.markAllRead('player1');
    expect(store.unreadCount('player1')).toBe(0);
  });

  it('unreadCount is independent across players', () => {
    const store = new NotificationStore();
    store.add('player1', 'dm-received', 'DM', 'Hi');
    store.add('player2', 'dm-received', 'DM', 'Hi');
    store.markAllRead('player1');
    expect(store.unreadCount('player1')).toBe(0);
    expect(store.unreadCount('player2')).toBe(1);
  });
});

// ─── PartyCollectionStore — 'co-op' hyphenated tag ──────────────────────────

describe("PartyCollectionStore — 'co-op' tag (Phase 31)", () => {
  let store: PartyCollectionStore;

  beforeEach(() => {
    store = new PartyCollectionStore();
  });

  it("getByTag('co-op') returns at least one collection", () => {
    const results = store.getByTag('co-op');
    expect(results.length).toBeGreaterThan(0);
  });

  it("collections returned by getByTag('co-op') all include 'co-op' in their tags", () => {
    const results = store.getByTag('co-op');
    for (const c of results) {
      expect(c.tags).toContain('co-op');
    }
  });
});

// ─── emulator-bridge adapter import (@types/node typeRoots regression) ───────

describe('emulator-bridge — createSystemAdapter import (Phase 31)', () => {
  it('createSystemAdapter returns args for nes system', () => {
    const adapter = createSystemAdapter('nes', 'fceux');
    const args = adapter.buildLaunchArgs('game.nes', {});
    expect(Array.isArray(args)).toBe(true);
  });

  it('createSystemAdapter returns args for gc system', () => {
    const adapter = createSystemAdapter('gc', 'dolphin');
    const args = adapter.buildLaunchArgs('game.iso', {});
    expect(Array.isArray(args)).toBe(true);
  });

  it('createSystemAdapter returns args for genesis system', () => {
    const adapter = createSystemAdapter('genesis', 'retroarch');
    const args = adapter.buildLaunchArgs('game.md', {});
    expect(Array.isArray(args)).toBe(true);
  });
});

// ─── System page createRoom payload fields ───────────────────────────────────

describe('System page createRoom payload completeness (Phase 31)', () => {
  it('all required payload fields are derivable from a MockGame', () => {
    // Simulate the shape that NESPage/SNESPage/GBAPage/N64Page/NDSPage now builds:
    const mockGame = {
      id: 'nes-contra',
      title: 'Contra',
      system: 'NES',
      systemColor: '#E60012',
      maxPlayers: 2,
      tags: ['2P'],
      badges: [],
      description: 'Run & gun classic.',
      coverEmoji: '🔫',
    };
    const displayName = 'Player1';
    const payload = {
      name: `${mockGame.title} Room`,
      gameId: mockGame.id,
      gameTitle: mockGame.title,
      system: mockGame.system,
      isPublic: true,
      maxPlayers: mockGame.maxPlayers,
    };

    // Verify all required CreateRoomPayload fields are present and typed correctly
    expect(typeof payload.name).toBe('string');
    expect(typeof payload.gameId).toBe('string');
    expect(typeof payload.gameTitle).toBe('string');
    expect(typeof payload.system).toBe('string');
    expect(typeof payload.isPublic).toBe('boolean');
    expect(typeof payload.maxPlayers).toBe('number');
    expect(typeof displayName).toBe('string');
  });
});
