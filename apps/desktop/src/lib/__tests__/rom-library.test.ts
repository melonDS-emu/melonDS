/**
 * Unit tests for the ROM library — per-game ROM file associations.
 *
 * Tests cover:
 *  - setRomAssociation / getRomAssociation  round-trips
 *  - clearRomAssociation  removes the entry
 *  - getAllAssociations   returns all stored entries
 *  - resolveGameRomPath  resolution priority (explicit assoc > derived path > null)
 */

import { describe, it, expect, beforeEach } from 'vitest';
import {
  setRomAssociation,
  getRomAssociation,
  clearRomAssociation,
  getAllAssociations,
  resolveGameRomPath,
} from '../rom-library';
import { setRomDirectory } from '../rom-settings';

// ---------------------------------------------------------------------------
// Minimal localStorage mock for the Node test environment
// ---------------------------------------------------------------------------

const store: Record<string, string> = {};
const mockLocalStorage = {
  getItem: (key: string) => store[key] ?? null,
  setItem: (key: string, value: string) => {
    store[key] = value;
  },
  removeItem: (key: string) => {
    delete store[key];
  },
  clear: () => {
    Object.keys(store).forEach((k) => delete store[k]);
  },
  get length() {
    return Object.keys(store).length;
  },
  key: (i: number) => Object.keys(store)[i] ?? null,
};
Object.defineProperty(globalThis, 'localStorage', {
  value: mockLocalStorage,
  writable: false,
});

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

describe('setRomAssociation / getRomAssociation', () => {
  beforeEach(() => {
    mockLocalStorage.clear();
  });

  it('stores and retrieves a ROM association', () => {
    setRomAssociation('n64-mario-kart-64', '/roms/mk64.z64');
    const assoc = getRomAssociation('n64-mario-kart-64');
    expect(assoc).not.toBeNull();
    expect(assoc!.gameId).toBe('n64-mario-kart-64');
    expect(assoc!.romPath).toBe('/roms/mk64.z64');
    expect(assoc!.setAt).toBeTruthy();
  });

  it('trims whitespace from the stored path', () => {
    setRomAssociation('gba-pokemon-fire-red', '  /roms/firered.gba  ');
    const assoc = getRomAssociation('gba-pokemon-fire-red');
    expect(assoc!.romPath).toBe('/roms/firered.gba');
  });

  it('overwrites an existing association for the same gameId', () => {
    setRomAssociation('nds-mario-kart', '/roms/old.nds');
    setRomAssociation('nds-mario-kart', '/roms/new.nds');
    const assoc = getRomAssociation('nds-mario-kart');
    expect(assoc!.romPath).toBe('/roms/new.nds');
  });

  it('returns null for an unknown gameId', () => {
    expect(getRomAssociation('does-not-exist')).toBeNull();
  });
});

describe('clearRomAssociation', () => {
  beforeEach(() => {
    mockLocalStorage.clear();
  });

  it('removes a stored association', () => {
    setRomAssociation('nes-tetris', '/roms/tetris.nes');
    clearRomAssociation('nes-tetris');
    expect(getRomAssociation('nes-tetris')).toBeNull();
  });

  it('is a no-op when the gameId is not stored', () => {
    expect(() => clearRomAssociation('non-existent')).not.toThrow();
  });
});

describe('getAllAssociations', () => {
  beforeEach(() => {
    mockLocalStorage.clear();
  });

  it('returns an empty array when nothing is stored', () => {
    expect(getAllAssociations()).toEqual([]);
  });

  it('returns all stored associations', () => {
    setRomAssociation('n64-mario-kart-64', '/roms/mk64.z64');
    setRomAssociation('gba-pokemon-fire-red', '/roms/firered.gba');
    const all = getAllAssociations();
    expect(all).toHaveLength(2);
    const ids = all.map((a) => a.gameId).sort();
    expect(ids).toEqual(['gba-pokemon-fire-red', 'n64-mario-kart-64']);
  });
});

describe('resolveGameRomPath', () => {
  beforeEach(() => {
    mockLocalStorage.clear();
  });

  it('returns the explicit per-game association when present', () => {
    setRomAssociation('n64-mario-kart-64', '/roms/mk64.z64');
    expect(resolveGameRomPath('n64-mario-kart-64')).toBe('/roms/mk64.z64');
  });

  it('falls back to the ROM directory when no association exists', () => {
    setRomDirectory('/home/user/roms');
    expect(resolveGameRomPath('n64-unknown-game')).toBe('/home/user/roms');
  });

  it('returns null when no association and no ROM directory is configured', () => {
    setRomDirectory('');
    expect(resolveGameRomPath('n64-unknown-game')).toBeNull();
  });

  it('prefers the per-game association over the ROM directory', () => {
    setRomDirectory('/home/user/roms');
    setRomAssociation('snes-super-mario-world', '/specific/smw.sfc');
    expect(resolveGameRomPath('snes-super-mario-world')).toBe('/specific/smw.sfc');
  });
});
