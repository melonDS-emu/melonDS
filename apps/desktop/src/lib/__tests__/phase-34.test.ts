/**
 * Phase 34 — RetroAchievements Polish & Premium UX
 *
 * Tests for:
 *  - achievement-capability.ts — ACHIEVEMENT_CAPABILITIES registry,
 *    getAchievementCapability, systemSupportsAchievements
 *  - retro-achievements-settings.ts — getRACredentials, setRACredentials,
 *    clearRACredentials, isRAConnected (tested via manual localStorage mock)
 */

import { describe, it, expect, beforeEach } from 'vitest';
import {
  ACHIEVEMENT_CAPABILITIES,
  getAchievementCapability,
  systemSupportsAchievements,
} from '../achievement-capability';
import {
  getRACredentials,
  setRACredentials,
  clearRACredentials,
  isRAConnected,
} from '../retro-achievements-settings';

// ---------------------------------------------------------------------------
// localStorage mock — follows the same pattern as phase-29.test.ts
// ---------------------------------------------------------------------------

const storageData: Record<string, string> = {};
const mockLocalStorage = {
  getItem: (key: string) => storageData[key] ?? null,
  setItem: (key: string, value: string) => { storageData[key] = value; },
  removeItem: (key: string) => { delete storageData[key]; },
  clear: () => { Object.keys(storageData).forEach((k) => delete storageData[k]); },
  get length() { return Object.keys(storageData).length; },
  key: (i: number) => Object.keys(storageData)[i] ?? null,
};
Object.defineProperty(globalThis, 'localStorage', {
  value: mockLocalStorage,
  writable: true,
  configurable: true,
});

beforeEach(() => {
  mockLocalStorage.clear();
});

// ---------------------------------------------------------------------------
// ACHIEVEMENT_CAPABILITIES registry
// ---------------------------------------------------------------------------

describe('ACHIEVEMENT_CAPABILITIES — registry coverage', () => {
  it('has at least 17 entries', () => {
    expect(Object.keys(ACHIEVEMENT_CAPABILITIES).length).toBeGreaterThanOrEqual(17);
  });

  it('covers all "full" systems: nes, snes, gb, gbc, gba, n64, psx, sms, genesis', () => {
    const full = ['nes', 'snes', 'gb', 'gbc', 'gba', 'n64', 'psx', 'sms', 'genesis'];
    for (const sys of full) {
      expect(ACHIEVEMENT_CAPABILITIES[sys]?.level, `${sys} should be full`).toBe('full');
    }
  });

  it('covers all "partial" systems: nds, dreamcast, ps2, psp', () => {
    const partial = ['nds', 'dreamcast', 'ps2', 'psp'];
    for (const sys of partial) {
      expect(ACHIEVEMENT_CAPABILITIES[sys]?.level, `${sys} should be partial`).toBe('partial');
    }
  });

  it('covers all "none" systems: gc, wii, wiiu, 3ds', () => {
    const none = ['gc', 'wii', 'wiiu', '3ds'];
    for (const sys of none) {
      expect(ACHIEVEMENT_CAPABILITIES[sys]?.level, `${sys} should be none`).toBe('none');
    }
  });

  it('every entry has a non-empty notes string', () => {
    for (const [key, cap] of Object.entries(ACHIEVEMENT_CAPABILITIES)) {
      expect(cap.notes.length, `${key} should have notes`).toBeGreaterThan(0);
    }
  });
});

// ---------------------------------------------------------------------------
// getAchievementCapability
// ---------------------------------------------------------------------------

describe('getAchievementCapability', () => {
  it('returns full for nes', () => {
    const cap = getAchievementCapability('nes');
    expect(cap.level).toBe('full');
    expect(cap.system).toBe('nes');
  });

  it('returns none for gc', () => {
    const cap = getAchievementCapability('gc');
    expect(cap.level).toBe('none');
  });

  it('returns partial for nds', () => {
    const cap = getAchievementCapability('nds');
    expect(cap.level).toBe('partial');
  });

  it('is case-insensitive — GBA and gba both return full', () => {
    expect(getAchievementCapability('GBA').level).toBe('full');
    expect(getAchievementCapability('gba').level).toBe('full');
  });

  it('returns none for unknown system', () => {
    const cap = getAchievementCapability('saturn');
    expect(cap.level).toBe('none');
  });
});

// ---------------------------------------------------------------------------
// systemSupportsAchievements
// ---------------------------------------------------------------------------

describe('systemSupportsAchievements', () => {
  it('returns true for nes (full)', () => {
    expect(systemSupportsAchievements('nes')).toBe(true);
  });

  it('returns true for nds (partial)', () => {
    expect(systemSupportsAchievements('nds')).toBe(true);
  });

  it('returns false for gc (none)', () => {
    expect(systemSupportsAchievements('gc')).toBe(false);
  });

  it('returns false for 3ds (none)', () => {
    expect(systemSupportsAchievements('3ds')).toBe(false);
  });

  it('returns false for unknown system', () => {
    expect(systemSupportsAchievements('virtualboy')).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// retro-achievements-settings (localStorage)
// ---------------------------------------------------------------------------

describe('getRACredentials — defaults when nothing stored', () => {
  it('returns empty strings and false by default', () => {
    const creds = getRACredentials();
    expect(creds.username).toBe('');
    expect(creds.token).toBe('');
    expect(creds.hardcoreMode).toBe(false);
  });
});

describe('setRACredentials', () => {
  it('stores and retrieves username', () => {
    setRACredentials({ username: 'TestPlayer' });
    expect(getRACredentials().username).toBe('TestPlayer');
  });

  it('stores and retrieves token', () => {
    setRACredentials({ token: 'abc123' });
    expect(getRACredentials().token).toBe('abc123');
  });

  it('stores and retrieves hardcoreMode = true', () => {
    setRACredentials({ hardcoreMode: true });
    expect(getRACredentials().hardcoreMode).toBe(true);
  });

  it('partial update does not overwrite unrelated keys', () => {
    setRACredentials({ username: 'Hero', token: 'xyz' });
    setRACredentials({ hardcoreMode: true });
    const creds = getRACredentials();
    expect(creds.username).toBe('Hero');
    expect(creds.token).toBe('xyz');
    expect(creds.hardcoreMode).toBe(true);
  });
});

describe('isRAConnected', () => {
  it('returns false with empty credentials', () => {
    expect(isRAConnected()).toBe(false);
  });

  it('returns false when only username is set', () => {
    setRACredentials({ username: 'Hero' });
    expect(isRAConnected()).toBe(false);
  });

  it('returns false when only token is set', () => {
    setRACredentials({ token: 'abc123' });
    expect(isRAConnected()).toBe(false);
  });

  it('returns true when both username and token are set', () => {
    setRACredentials({ username: 'Hero', token: 'abc123' });
    expect(isRAConnected()).toBe(true);
  });
});

describe('clearRACredentials', () => {
  it('removes stored credentials', () => {
    setRACredentials({ username: 'Hero', token: 'abc123', hardcoreMode: true });
    clearRACredentials();
    const creds = getRACredentials();
    expect(creds.username).toBe('');
    expect(creds.token).toBe('');
    expect(creds.hardcoreMode).toBe(false);
  });

  it('isRAConnected returns false after clear', () => {
    setRACredentials({ username: 'Hero', token: 'abc123' });
    clearRACredentials();
    expect(isRAConnected()).toBe(false);
  });
});
