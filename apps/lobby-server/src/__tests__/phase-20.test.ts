/**
 * Phase 20 — Debug & Polish
 *
 * Tests for:
 *  - GameCube page game definitions (9 games, correct metadata)
 *  - GameCube session templates (Phase 18 carry-over validation)
 *  - Duplicate-free mock-game catalog (regression: n64-mario-party-2)
 *  - GC system adapter launch args (Dolphin relay, no CLI netplay flags)
 *  - GC mock catalog completeness
 *  - Notification store error isolation (add() with invalid playerId)
 *  - GlobalChatStore error isolation (post() with empty text)
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import { SYSTEM_INFO } from '../../../../packages/shared-types/src/systems';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';
import { GlobalChatStore } from '../global-chat-store';
import { NotificationStore } from '../notification-store';

// ---------------------------------------------------------------------------
// Bug regression: no duplicate mock-game IDs
// ---------------------------------------------------------------------------

describe('Mock game catalog — no duplicates', () => {
  it('has no duplicate IDs across the entire catalog', () => {
    const ids = MOCK_GAMES.map((g) => g.id);
    const unique = new Set(ids);
    expect(unique.size).toBe(ids.length);
  });

  it('n64-mario-party-2 appears exactly once', () => {
    const count = MOCK_GAMES.filter((g) => g.id === 'n64-mario-party-2').length;
    expect(count).toBe(1);
  });
});

// ---------------------------------------------------------------------------
// GameCube system type
// ---------------------------------------------------------------------------

describe('GameCube system type', () => {
  it('includes gc in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['gc']).toBeDefined();
    expect(SYSTEM_INFO['gc'].name).toBe('Nintendo GameCube');
    expect(SYSTEM_INFO['gc'].shortName).toBe('GC');
    expect(SYSTEM_INFO['gc'].generation).toBe(6);
    expect(SYSTEM_INFO['gc'].maxLocalPlayers).toBe(4);
  });

  it('GC is generation 6', () => {
    expect(SYSTEM_INFO['gc'].generation).toBe(6);
  });
});

// ---------------------------------------------------------------------------
// Dolphin backend — GC coverage
// ---------------------------------------------------------------------------

describe('Dolphin backend covers GameCube', () => {
  it('Dolphin backend systems list includes gc', () => {
    const dolphin = KNOWN_BACKENDS.find((b) => b.id === 'dolphin');
    expect(dolphin).toBeDefined();
    expect(dolphin?.systems).toContain('gc');
  });

  it('Dolphin supports netplay', () => {
    const dolphin = KNOWN_BACKENDS.find((b) => b.id === 'dolphin');
    expect(dolphin?.supportsNetplay).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// GC system adapter launch args
// ---------------------------------------------------------------------------

describe('GameCube system adapter', () => {
  const adapter = createSystemAdapter('gc');

  it('uses dolphin as preferred backend', () => {
    expect(adapter.preferredBackendId).toBe('dolphin');
  });

  it('falls back to retroarch', () => {
    expect(adapter.fallbackBackendIds).toContain('retroarch');
  });

  it('builds Dolphin batch-mode launch args with rom path', () => {
    const args = adapter.buildLaunchArgs('/roms/gc/mario-kart-dd.iso', {});
    expect(args).toContain('-b');
    expect(args).toContain('-e');
    expect(args).toContain('/roms/gc/mario-kart-dd.iso');
  });

  it('adds --no-gui when fullscreen is requested', () => {
    const args = adapter.buildLaunchArgs('/roms/gc/melee.iso', { fullscreen: true });
    expect(args).toContain('--no-gui');
  });

  it('does NOT add netplay host/port flags (relay-handled)', () => {
    const args = adapter.buildLaunchArgs('/roms/gc/f-zero-gx.iso', {
      netplayHost: '127.0.0.1',
      netplayPort: 9100,
    });
    expect(args.join(' ')).not.toMatch(/--netplay/);
    expect(args.join(' ')).not.toMatch(/--connect/);
  });

  it('does NOT add --wii-motionplus for GC games', () => {
    const args = adapter.buildLaunchArgs('/roms/gc/melee.iso', {});
    expect(args).not.toContain('--wii-motionplus');
  });

  it('adds --debugger when debug is true', () => {
    const args = adapter.buildLaunchArgs('/roms/gc/mkdd.iso', { debug: true });
    expect(args).toContain('--debugger');
  });

  it('saves to gc/ subdirectory', () => {
    const path = adapter.getSavePath('gc-mario-kart-double-dash', '/saves');
    expect(path).toBe('/saves/gc/gc-mario-kart-double-dash');
  });
});

// ---------------------------------------------------------------------------
// Session templates — GameCube (Phase 18 carry-over)
// ---------------------------------------------------------------------------

describe('GameCube session templates', () => {
  const store = new SessionTemplateStore();

  it('includes a default 2P GC template', () => {
    const tpl = store.get('gc-default-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('gc');
    expect(tpl?.playerCount).toBe(2);
    expect(tpl?.emulatorBackendId).toBe('dolphin');
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes a default 4P GC template', () => {
    const tpl = store.get('gc-default-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('gc');
    expect(tpl?.playerCount).toBe(4);
    expect(tpl?.emulatorBackendId).toBe('dolphin');
  });

  it('includes Mario Kart: Double Dash 4P template', () => {
    const tpl = store.get('gc-mario-kart-double-dash-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('gc-mario-kart-double-dash');
    expect(tpl?.playerCount).toBe(4);
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes Super Smash Bros. Melee 4P template', () => {
    const tpl = store.get('gc-super-smash-bros-melee-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('gc-super-smash-bros-melee');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes F-Zero GX 4P template', () => {
    const tpl = store.get('gc-f-zero-gx-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('gc-f-zero-gx');
    expect(tpl?.playerCount).toBe(4);
  });

  it('getForGame resolves gc-default as fallback', () => {
    const tpl = store.getForGame('gc-unknown-game', 'gc');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('gc');
  });

  it('all GC relay templates have latencyTarget of 120ms', () => {
    const gcTemplates = store.listAll().filter((t) => t.system === 'gc' && t.netplayMode === 'online-relay');
    expect(gcTemplates.length).toBeGreaterThan(0);
    for (const tpl of gcTemplates) {
      expect(tpl.latencyTarget).toBe(120);
    }
  });
});

// ---------------------------------------------------------------------------
// Mock game catalog — GameCube games
// ---------------------------------------------------------------------------

describe('GameCube mock game catalog', () => {
  const gcGames = MOCK_GAMES.filter((g) => g.system === 'GC');

  it('includes at least 9 GC games in the catalog', () => {
    expect(gcGames.length).toBeGreaterThanOrEqual(9);
  });

  it('includes Mario Kart: Double Dash', () => {
    const game = gcGames.find((g) => g.id === 'gc-mario-kart-double-dash');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(4);
  });

  it('includes Super Smash Bros. Melee', () => {
    const game = gcGames.find((g) => g.id === 'gc-super-smash-bros-melee');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(4);
  });

  it('includes F-Zero GX', () => {
    const game = gcGames.find((g) => g.id === 'gc-f-zero-gx');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(4);
  });

  it('includes Luigi\'s Mansion', () => {
    const game = gcGames.find((g) => g.id === 'gc-luigi-mansion');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(1);
  });

  it('includes Pikmin 2 as 2-player co-op', () => {
    const game = gcGames.find((g) => g.id === 'gc-pikmin-2');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(2);
  });

  it('all GC games have the correct system label', () => {
    for (const game of gcGames) {
      expect(game.system).toBe('GC');
    }
  });

  it('includes Mario Party 4 through 7', () => {
    for (const n of [4, 5, 6, 7]) {
      const game = gcGames.find((g) => g.id === `gc-mario-party-${n}`);
      expect(game).toBeDefined();
      expect(game?.maxPlayers).toBe(4);
    }
  });
});

// ---------------------------------------------------------------------------
// NotificationStore — error isolation
// ---------------------------------------------------------------------------

describe('NotificationStore error isolation', () => {
  it('add() does not throw for a normal notification', () => {
    const store = new NotificationStore();
    expect(() =>
      store.add('player-1', 'friend-request', 'Friend Request', 'Alice wants to be your friend.', 'req-1')
    ).not.toThrow();
  });

  it('list() returns the notification just added', () => {
    const store = new NotificationStore();
    store.add('player-1', 'dm-received', 'DM', 'Hello!', 'dm-1');
    const all = store.list('player-1');
    expect(all.length).toBe(1);
    expect(all[0].type).toBe('dm-received');
    expect(all[0].body).toBe('Hello!');
    expect(all[0].read).toBe(false);
  });

  it('markAllRead() clears unread count for the player', () => {
    const store = new NotificationStore();
    store.add('player-2', 'friend-request', 'FR', 'Bob sent a request.', 'req-2');
    expect(store.unreadCount('player-2')).toBe(1);
    store.markAllRead('player-2');
    expect(store.unreadCount('player-2')).toBe(0);
  });

  it('markAllRead() on unknown player does not throw', () => {
    const store = new NotificationStore();
    expect(() => store.markAllRead('does-not-exist')).not.toThrow();
  });

  it('list() on unknown player returns empty array', () => {
    const store = new NotificationStore();
    const all = store.list('ghost-player');
    expect(all).toEqual([]);
  });

  it('unreadCount() on unknown player returns 0', () => {
    const store = new NotificationStore();
    expect(store.unreadCount('ghost-player')).toBe(0);
  });

  it('markRead() by notificationId marks only that notification', () => {
    const store = new NotificationStore();
    const n1 = store.add('player-3', 'friend-request', 'FR', 'Carol.', 'req-3');
    store.add('player-3', 'dm-received', 'DM', 'Hey!', 'dm-3');
    store.markRead(n1.id);
    expect(store.unreadCount('player-3')).toBe(1);
  });
});

// ---------------------------------------------------------------------------
// GlobalChatStore — error isolation
// ---------------------------------------------------------------------------

describe('GlobalChatStore error isolation', () => {
  it('post() stores a message and returns it', () => {
    const store = new GlobalChatStore();
    const msg = store.post('pid-1', 'Alice', 'Hello world!');
    expect(msg.displayName).toBe('Alice');
    expect(msg.text).toBe('Hello world!');
    expect(msg.id).toBeTruthy();
  });

  it('getRecent() returns messages in insertion order', () => {
    const store = new GlobalChatStore();
    store.post('pid-1', 'Alice', 'First');
    store.post('pid-2', 'Bob', 'Second');
    const msgs = store.getRecent(10);
    expect(msgs[0].text).toBe('First');
    expect(msgs[1].text).toBe('Second');
  });

  it('ring buffer caps at maxMessages (custom limit)', () => {
    const store = new GlobalChatStore(10);
    for (let i = 0; i < 15; i++) {
      store.post(`pid-${i}`, `User${i}`, `msg ${i}`);
    }
    const msgs = store.getRecent(100);
    expect(msgs.length).toBe(10);
    // Oldest 5 were evicted
    expect(msgs[0].text).toBe('msg 5');
    expect(msgs[9].text).toBe('msg 14');
  });

  it('getRecent(n) returns only the last n messages', () => {
    const store = new GlobalChatStore();
    for (let i = 0; i < 20; i++) {
      store.post('pid', 'User', `msg ${i}`);
    }
    const last5 = store.getRecent(5);
    expect(last5.length).toBe(5);
    expect(last5[0].text).toBe('msg 15');
    expect(last5[4].text).toBe('msg 19');
  });

  it('clear() removes all messages', () => {
    const store = new GlobalChatStore();
    store.post('pid-1', 'Alice', 'Hello');
    store.clear();
    expect(store.getRecent(100).length).toBe(0);
  });
});
