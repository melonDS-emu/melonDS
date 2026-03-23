/**
 * Phase 38 Tests: Real ROM Flow and Game Catalog Unification (desktop).
 *
 * Covers:
 *  - rom-library: resolveGameRomPath no longer returns a directory as a ROM path
 *  - rom-library: verifyRomAssociation and pruneStaleAssociation helpers
 *  - api-client: default mode changed from 'mock' to 'hybrid'
 *  - api-client: normalizeServerGame handles lowercase systems and missing fields
 */

import { describe, it, expect, beforeEach, vi, afterEach } from 'vitest';
import {
  setRomAssociation,
  getRomAssociation,
  clearRomAssociation,
  getAllAssociations,
  resolveGameRomPath,
  verifyRomAssociation,
  pruneStaleAssociation,
} from '../rom-library';
import { GameApiClient } from '../api-client';

// ---------------------------------------------------------------------------
// Helpers / mocks
// ---------------------------------------------------------------------------

const mockLocalStorage = (() => {
  let store: Record<string, string> = {};
  return {
    getItem: (key: string) => store[key] ?? null,
    setItem: (key: string, value: string) => { store[key] = value; },
    removeItem: (key: string) => { delete store[key]; },
    clear: () => { store = {}; },
  };
})();

Object.defineProperty(globalThis, 'localStorage', { value: mockLocalStorage });

// ---------------------------------------------------------------------------
// rom-library — resolveGameRomPath
// ---------------------------------------------------------------------------

describe('resolveGameRomPath — phase 38', () => {
  beforeEach(() => mockLocalStorage.clear());

  it('returns null when no association is stored', () => {
    expect(resolveGameRomPath('n64-mario-kart-64')).toBeNull();
  });

  it('returns the stored ROM path when an association exists', () => {
    setRomAssociation('n64-mario-kart-64', '/roms/mario-kart.z64');
    expect(resolveGameRomPath('n64-mario-kart-64')).toBe('/roms/mario-kart.z64');
  });

  it('does NOT return a directory path when no association is stored', () => {
    // Even if a ROM directory is configured, resolveGameRomPath must not return
    // it as a ROM file path — passing a directory to an emulator always fails.
    mockLocalStorage.setItem('retro-oasis-rom-directory', '/home/user/roms');
    const result = resolveGameRomPath('snes-super-mario-world');
    expect(result).toBeNull();
  });

  it('returns null after the association is cleared', () => {
    setRomAssociation('gba-pokemon-ruby', '/roms/ruby.gba');
    clearRomAssociation('gba-pokemon-ruby');
    expect(resolveGameRomPath('gba-pokemon-ruby')).toBeNull();
  });

  it('returns the trimmed path stored by setRomAssociation', () => {
    setRomAssociation('nes-contra', '  /roms/contra.nes  ');
    expect(resolveGameRomPath('nes-contra')).toBe('/roms/contra.nes');
  });
});

// ---------------------------------------------------------------------------
// rom-library — verifyRomAssociation
// ---------------------------------------------------------------------------

describe('verifyRomAssociation — phase 38', () => {
  beforeEach(() => {
    mockLocalStorage.clear();
    vi.restoreAllMocks();
  });

  afterEach(() => vi.restoreAllMocks());

  it('returns true when no association is stored (nothing to verify)', async () => {
    const result = await verifyRomAssociation('unknown-game');
    expect(result).toBe(true);
  });

  it('returns true when the HTTP fallback reports exists: true', async () => {
    setRomAssociation('nes-contra', '/roms/contra.nes');
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
      ok: true,
      json: () => Promise.resolve({ exists: true }),
    }));
    expect(await verifyRomAssociation('nes-contra')).toBe(true);
  });

  it('returns false when the HTTP fallback reports exists: false', async () => {
    setRomAssociation('nes-contra', '/roms/missing.nes');
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
      ok: true,
      json: () => Promise.resolve({ exists: false }),
    }));
    expect(await verifyRomAssociation('nes-contra')).toBe(false);
  });

  it('returns false when the HTTP request fails', async () => {
    setRomAssociation('nes-contra', '/roms/gone.nes');
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ ok: false }));
    expect(await verifyRomAssociation('nes-contra')).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// rom-library — pruneStaleAssociation
// ---------------------------------------------------------------------------

describe('pruneStaleAssociation — phase 38', () => {
  beforeEach(() => {
    mockLocalStorage.clear();
    vi.restoreAllMocks();
  });

  it('returns false and keeps the association when the file exists', async () => {
    setRomAssociation('gba-emerald', '/roms/emerald.gba');
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
      ok: true,
      json: () => Promise.resolve({ exists: true }),
    }));
    const pruned = await pruneStaleAssociation('gba-emerald');
    expect(pruned).toBe(false);
    expect(getRomAssociation('gba-emerald')).not.toBeNull();
  });

  it('returns true and removes the association when the file is missing', async () => {
    setRomAssociation('gba-emerald', '/roms/gone.gba');
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
      ok: true,
      json: () => Promise.resolve({ exists: false }),
    }));
    const pruned = await pruneStaleAssociation('gba-emerald');
    expect(pruned).toBe(true);
    expect(getRomAssociation('gba-emerald')).toBeNull();
  });

  it('treats a failed HTTP check as missing and prunes', async () => {
    setRomAssociation('snes-zelda', '/roms/zelda.sfc');
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ ok: false }));
    const pruned = await pruneStaleAssociation('snes-zelda');
    expect(pruned).toBe(true);
    expect(getRomAssociation('snes-zelda')).toBeNull();
  });
});

// ---------------------------------------------------------------------------
// rom-library — getAllAssociations
// ---------------------------------------------------------------------------

describe('getAllAssociations — phase 38', () => {
  beforeEach(() => mockLocalStorage.clear());

  it('returns an empty array when nothing is stored', () => {
    expect(getAllAssociations()).toEqual([]);
  });

  it('returns all stored associations', () => {
    setRomAssociation('nes-contra', '/roms/contra.nes');
    setRomAssociation('snes-mario', '/roms/mario.sfc');
    const all = getAllAssociations();
    expect(all).toHaveLength(2);
    expect(all.map((a) => a.gameId).sort()).toEqual(['nes-contra', 'snes-mario'].sort());
  });

  it('each association has the required fields', () => {
    setRomAssociation('gba-metroid', '/roms/metroid.gba');
    const [assoc] = getAllAssociations();
    expect(assoc).toHaveProperty('gameId', 'gba-metroid');
    expect(assoc).toHaveProperty('romPath', '/roms/metroid.gba');
    expect(assoc).toHaveProperty('setAt');
    expect(typeof assoc.setAt).toBe('string');
  });
});

// ---------------------------------------------------------------------------
// api-client: default mode and normalizeServerGame
// ---------------------------------------------------------------------------

describe('GameApiClient — phase 38', () => {
  afterEach(() => vi.restoreAllMocks());

  it('defaults to hybrid mode when no VITE_API_MODE is set', async () => {
    // In hybrid mode with no reachable backend the mock data must still be returned.
    const client = new GameApiClient();
    const games = await client.getGames();
    expect(games).toBeInstanceOf(Array);
    expect(games.length).toBeGreaterThan(0);
  });

  it('mock mode returns MOCK_GAMES when explicitly requested', async () => {
    const client = new GameApiClient('mock');
    const games = await client.getGames();
    expect(games.length).toBeGreaterThan(0);
    expect(games[0]).toHaveProperty('id');
    expect(games[0]).toHaveProperty('system');
    expect(games[0]).toHaveProperty('coverEmoji');
    expect(games[0]).toHaveProperty('systemColor');
  });

  it('all mock games have uppercase system strings', async () => {
    const client = new GameApiClient('mock');
    const games = await client.getGames();
    for (const game of games) {
      expect(game.system).toBe(game.system.toUpperCase());
    }
  });

  it('normalizes server response: uppercase system + missing coverEmoji/systemColor', async () => {
    const mockServerGame = {
      id: 'nes-contra',
      title: 'Contra',
      system: 'nes',         // lowercase — as server seed data sends it
      maxPlayers: 2,
      tags: ['2P', 'Co-op'],
      badges: ['Best with Friends'],
      description: 'Classic co-op.',
      supportsPublicLobby: true,
      supportsPrivateLobby: true,
      compatibilityNotes: [],
      onlineRecommended: true,
      // no coverEmoji, no systemColor, no isDsiWare
    };

    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
      ok: true,
      json: () => Promise.resolve([mockServerGame]),
    }));

    const client = new GameApiClient('backend', 'http://localhost:8080');
    const games = await client.getGames();

    expect(games).toHaveLength(1);
    expect(games[0].system).toBe('NES');
    expect(games[0].coverEmoji).toBeTruthy();
    expect(games[0].systemColor).toBeTruthy();
    expect(games[0].id).toBe('nes-contra');
  });

  it('normalizeServerGame fills defaults for minimal server response', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
      ok: true,
      json: () => Promise.resolve([{ id: 'x', title: 'X', system: 'gba', maxPlayers: 1 }]),
    }));

    const client = new GameApiClient('backend', 'http://localhost:8080');
    const [game] = await client.getGames();

    expect(game.system).toBe('GBA');
    expect(game.tags).toEqual([]);
    expect(game.badges).toEqual([]);
    expect(game.compatibilityNotes).toEqual([]);
    expect(game.coverEmoji).toBe('🎮');
    expect(game.isDsiWare).toBeUndefined();
  });

  it('hybrid mode falls back to mock when backend is unreachable', async () => {
    vi.stubGlobal('fetch', vi.fn().mockRejectedValue(new Error('Network error')));

    const client = new GameApiClient('hybrid', 'http://localhost:9999');
    const games = await client.getGames();
    expect(games.length).toBeGreaterThan(0);
  });

  it('getGameById normalizes the server response', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
      ok: true,
      json: () => Promise.resolve({
        id: 'n64-mario-kart-64',
        title: 'Mario Kart 64',
        system: 'n64',
        maxPlayers: 4,
        tags: ['4P', 'Party'],
        badges: ['Party Favorite'],
        description: 'Kart racing.',
        supportsPublicLobby: true,
        supportsPrivateLobby: true,
        compatibilityNotes: [],
        onlineRecommended: true,
      }),
    }));

    const client = new GameApiClient('backend', 'http://localhost:8080');
    const game = await client.getGameById('n64-mario-kart-64');

    expect(game).not.toBeNull();
    expect(game!.system).toBe('N64');
    expect(game!.coverEmoji).toBeTruthy();
  });

  it('normalizes all common lowercase system names from server', async () => {
    const systems = ['nes', 'snes', 'gb', 'gbc', 'gba', 'n64', 'nds', 'gc'];
    for (const sys of systems) {
      vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
        ok: true,
        json: () => Promise.resolve([{ id: `${sys}-game`, title: 'Game', system: sys, maxPlayers: 2 }]),
      }));
      const client = new GameApiClient('backend', 'http://localhost:8080');
      const [game] = await client.getGames();
      expect(game.system).toBe(sys.toUpperCase());
      vi.restoreAllMocks();
    }
  });

  it('isDsiWare is preserved when set on server response', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
      ok: true,
      json: () => Promise.resolve([{
        id: 'nds-flipnote',
        title: 'Flipnote Studio',
        system: 'nds',
        maxPlayers: 1,
        tags: [],
        badges: [],
        description: '',
        supportsPublicLobby: false,
        supportsPrivateLobby: false,
        compatibilityNotes: [],
        onlineRecommended: false,
        isDsiWare: true,
      }]),
    }));

    const client = new GameApiClient('backend', 'http://localhost:8080');
    const [game] = await client.getGames();
    expect(game.isDsiWare).toBe(true);
  });
});
