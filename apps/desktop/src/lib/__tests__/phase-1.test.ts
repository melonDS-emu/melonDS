/**
 * Phase 1 — Single-player shell: unit tests
 *
 * Tests for:
 *  - favorites-store  (isFavorite, toggleFavorite, getFavoriteIds, clearFavorites)
 *  - recently-played  (recordRecentlyPlayed, getRecentlyPlayed, clearRecentlyPlayed, MAX_ENTRIES de-dupe)
 *  - emulator-settings (getEmulatorPath, setEmulatorPath, clearEmulatorPath,
 *                       getAllEmulatorPaths, getEmulatorPathForSystem, getBackendIdForSystem,
 *                       SYSTEM_BACKEND_MAP coverage for primary systems)
 */

import { describe, it, expect, beforeEach } from 'vitest';
import {
  isFavorite,
  toggleFavorite,
  getFavoriteIds,
  clearFavorites,
} from '../favorites-store';
import {
  recordRecentlyPlayed,
  getRecentlyPlayed,
  clearRecentlyPlayed,
} from '../recently-played';
import {
  getEmulatorPath,
  setEmulatorPath,
  clearEmulatorPath,
  getAllEmulatorPaths,
  getEmulatorPathForSystem,
  getBackendIdForSystem,
  SYSTEM_BACKEND_MAP,
  EMULATOR_NAMES,
  EMULATOR_DEFAULT_PATHS,
} from '../emulator-settings';

// ---------------------------------------------------------------------------
// Minimal localStorage mock for the Node test environment
// ---------------------------------------------------------------------------

const store: Record<string, string> = {};
const mockLocalStorage = {
  getItem: (key: string) => store[key] ?? null,
  setItem: (key: string, value: string) => { store[key] = value; },
  removeItem: (key: string) => { delete store[key]; },
  clear: () => { Object.keys(store).forEach((k) => delete store[k]); },
  get length() { return Object.keys(store).length; },
  key: (i: number) => Object.keys(store)[i] ?? null,
};
Object.defineProperty(globalThis, 'localStorage', {
  value: mockLocalStorage,
  writable: false,
});

// ---------------------------------------------------------------------------
// favorites-store
// ---------------------------------------------------------------------------

describe('favorites-store', () => {
  beforeEach(() => mockLocalStorage.clear());

  it('isFavorite returns false for an unknown game', () => {
    expect(isFavorite('gba-pokemon-fire-red')).toBe(false);
  });

  it('toggleFavorite adds a game and returns true', () => {
    const result = toggleFavorite('gba-pokemon-fire-red');
    expect(result).toBe(true);
    expect(isFavorite('gba-pokemon-fire-red')).toBe(true);
  });

  it('toggleFavorite removes a favorited game and returns false', () => {
    toggleFavorite('gba-pokemon-fire-red');
    const result = toggleFavorite('gba-pokemon-fire-red');
    expect(result).toBe(false);
    expect(isFavorite('gba-pokemon-fire-red')).toBe(false);
  });

  it('getFavoriteIds returns all favorited game IDs', () => {
    toggleFavorite('nes-tetris');
    toggleFavorite('snes-super-mario-world');
    const ids = getFavoriteIds().sort();
    expect(ids).toEqual(['nes-tetris', 'snes-super-mario-world']);
  });

  it('clearFavorites removes all favorites', () => {
    toggleFavorite('nes-tetris');
    toggleFavorite('snes-super-mario-world');
    clearFavorites();
    expect(getFavoriteIds()).toHaveLength(0);
  });

  it('getFavoriteIds returns empty array when no favorites are stored', () => {
    expect(getFavoriteIds()).toEqual([]);
  });
});

// ---------------------------------------------------------------------------
// recently-played
// ---------------------------------------------------------------------------

describe('recently-played', () => {
  beforeEach(() => mockLocalStorage.clear());

  it('getRecentlyPlayed returns empty array initially', () => {
    expect(getRecentlyPlayed()).toEqual([]);
  });

  it('recordRecentlyPlayed adds a game as most recent', () => {
    recordRecentlyPlayed('gba-pokemon-fire-red');
    const entries = getRecentlyPlayed();
    expect(entries).toHaveLength(1);
    expect(entries[0].gameId).toBe('gba-pokemon-fire-red');
    expect(entries[0].playedAt).toBeTruthy();
  });

  it('entries are ordered most-recent first', () => {
    recordRecentlyPlayed('game-a');
    recordRecentlyPlayed('game-b');
    recordRecentlyPlayed('game-c');
    const entries = getRecentlyPlayed();
    expect(entries.map((e) => e.gameId)).toEqual(['game-c', 'game-b', 'game-a']);
  });

  it('re-playing a game moves it to the front (de-dupe)', () => {
    recordRecentlyPlayed('game-a');
    recordRecentlyPlayed('game-b');
    recordRecentlyPlayed('game-a'); // replay
    const entries = getRecentlyPlayed();
    expect(entries[0].gameId).toBe('game-a');
    expect(entries).toHaveLength(2); // no duplicate
  });

  it('getRecentlyPlayed respects the limit parameter', () => {
    for (let i = 0; i < 10; i++) recordRecentlyPlayed(`game-${i}`);
    const entries = getRecentlyPlayed(3);
    expect(entries).toHaveLength(3);
    expect(entries[0].gameId).toBe('game-9');
  });

  it('clearRecentlyPlayed removes all entries', () => {
    recordRecentlyPlayed('game-a');
    recordRecentlyPlayed('game-b');
    clearRecentlyPlayed();
    expect(getRecentlyPlayed()).toHaveLength(0);
  });
});

// ---------------------------------------------------------------------------
// emulator-settings
// ---------------------------------------------------------------------------

describe('emulator-settings', () => {
  beforeEach(() => mockLocalStorage.clear());

  it('getEmulatorPath returns null when nothing is configured', () => {
    expect(getEmulatorPath('mgba')).toBeNull();
  });

  it('setEmulatorPath persists and getEmulatorPath retrieves it', () => {
    setEmulatorPath('mgba', '/usr/bin/mgba');
    expect(getEmulatorPath('mgba')).toBe('/usr/bin/mgba');
  });

  it('setEmulatorPath trims whitespace', () => {
    setEmulatorPath('mgba', '  /usr/bin/mgba  ');
    expect(getEmulatorPath('mgba')).toBe('/usr/bin/mgba');
  });

  it('clearEmulatorPath removes a configured path', () => {
    setEmulatorPath('mgba', '/usr/bin/mgba');
    clearEmulatorPath('mgba');
    expect(getEmulatorPath('mgba')).toBeNull();
  });

  it('clearEmulatorPath is a no-op for an unconfigured backend', () => {
    expect(() => clearEmulatorPath('mgba')).not.toThrow();
  });

  it('getAllEmulatorPaths returns all configured paths', () => {
    setEmulatorPath('mgba', '/usr/bin/mgba');
    setEmulatorPath('fceux', '/usr/bin/fceux');
    const all = getAllEmulatorPaths();
    expect(all['mgba']).toBe('/usr/bin/mgba');
    expect(all['fceux']).toBe('/usr/bin/fceux');
    expect(Object.keys(all)).toHaveLength(2);
  });

  it('getEmulatorPathForSystem resolves via SYSTEM_BACKEND_MAP', () => {
    setEmulatorPath('mgba', '/usr/bin/mgba');
    expect(getEmulatorPathForSystem('gba')).toBe('/usr/bin/mgba');
    expect(getEmulatorPathForSystem('GBA')).toBe('/usr/bin/mgba'); // case-insensitive
  });

  it('getEmulatorPathForSystem returns null for unrecognised system', () => {
    expect(getEmulatorPathForSystem('unknown-system')).toBeNull();
  });

  it('getEmulatorPathForSystem returns null when path is not configured', () => {
    expect(getEmulatorPathForSystem('nes')).toBeNull();
  });

  it('getBackendIdForSystem returns the correct backend ID', () => {
    expect(getBackendIdForSystem('gba')).toBe('mgba');
    expect(getBackendIdForSystem('nes')).toBe('fceux');
    expect(getBackendIdForSystem('snes')).toBe('snes9x');
    expect(getBackendIdForSystem('nds')).toBe('melonds');
    expect(getBackendIdForSystem('n64')).toBe('mupen64plus');
  });

  it('getBackendIdForSystem returns null for unknown system', () => {
    expect(getBackendIdForSystem('atari')).toBeNull();
  });


  it('includes Windows executable hints for key desktop emulators', () => {
    expect(EMULATOR_DEFAULT_PATHS['mgba']).toContain('C:\\Program Files\\mGBA\\mGBA.exe');
    expect(EMULATOR_DEFAULT_PATHS['melonds']).toContain('C:\\Program Files\\melonDS\\melonDS.exe');
    expect(EMULATOR_DEFAULT_PATHS['dolphin']).toContain('C:\\Program Files\\Dolphin\\Dolphin.exe');
  });

  it('SYSTEM_BACKEND_MAP covers all primary Phase-1 systems', () => {
    const required = ['gba', 'nes', 'snes', 'gb', 'gbc'];
    for (const sys of required) {
      expect(SYSTEM_BACKEND_MAP[sys]).toBeTruthy();
    }
  });

  it('EMULATOR_NAMES provides a display name for every SYSTEM_BACKEND_MAP backend', () => {
    const backends = new Set(Object.values(SYSTEM_BACKEND_MAP));
    for (const b of backends) {
      expect(EMULATOR_NAMES[b]).toBeTruthy();
    }
  });

  it('EMULATOR_DEFAULT_PATHS includes at least one path for mgba', () => {
    expect((EMULATOR_DEFAULT_PATHS['mgba'] ?? []).length).toBeGreaterThan(0);
  });
});
