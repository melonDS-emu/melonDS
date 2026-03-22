/**
 * Phase 35 — Debug, Audit & Stabilization
 *
 * Tests that verify the fixes introduced in this pass:
 *  1. getEmulatorPath treats empty/blank strings as unconfigured (null)
 *  2. getAchievementCapability('dreamcast') resolves the partial entry
 *     (key was previously 'dc', now corrected to 'dreamcast')
 *  3. systemSupportsAchievements returns true for dreamcast (partial)
 *  4. Achievement capability registry has no stale 'dc' key
 */

import { describe, it, expect, beforeEach } from 'vitest';
import {
  getEmulatorPath,
  setEmulatorPath,
  clearEmulatorPath,
  getEmulatorPathForSystem,
} from '../emulator-settings';
import {
  ACHIEVEMENT_CAPABILITIES,
  getAchievementCapability,
  systemSupportsAchievements,
} from '../achievement-capability';

// ---------------------------------------------------------------------------
// localStorage mock (same pattern as phase-1.test.ts / phase-34.test.ts)
// ---------------------------------------------------------------------------

const storageData: Record<string, string> = {};
const mockLocalStorage = {
  getItem: (key: string) => storageData[key] ?? null,
  setItem: (key: string, value: string) => {
    storageData[key] = value;
  },
  removeItem: (key: string) => {
    delete storageData[key];
  },
  clear: () => {
    Object.keys(storageData).forEach((k) => delete storageData[k]);
  },
  get length() {
    return Object.keys(storageData).length;
  },
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
// Fix 1 — getEmulatorPath: empty/blank strings treated as null
// ---------------------------------------------------------------------------

describe('getEmulatorPath — empty-string guard', () => {
  it('returns null for a blank string stored in localStorage', () => {
    // Simulate a corrupted localStorage entry containing an empty string.
    storageData['emulator-paths'] = JSON.stringify({ retroarch: '' });
    expect(getEmulatorPath('retroarch')).toBeNull();
  });

  it('returns null for a whitespace-only string stored in localStorage', () => {
    storageData['emulator-paths'] = JSON.stringify({ retroarch: '   ' });
    expect(getEmulatorPath('retroarch')).toBeNull();
  });

  it('returns the path when a valid non-empty path is stored', () => {
    setEmulatorPath('retroarch', '/usr/bin/retroarch');
    expect(getEmulatorPath('retroarch')).toBe('/usr/bin/retroarch');
  });

  it('returns null when path is not configured at all', () => {
    expect(getEmulatorPath('retroarch')).toBeNull();
  });

  it('clearEmulatorPath leaves a null result', () => {
    setEmulatorPath('retroarch', '/usr/bin/retroarch');
    clearEmulatorPath('retroarch');
    expect(getEmulatorPath('retroarch')).toBeNull();
  });
});

describe('getEmulatorPathForSystem — SMS uses retroarch backend', () => {
  it('returns null for sms when retroarch is not configured', () => {
    expect(getEmulatorPathForSystem('sms')).toBeNull();
  });

  it('returns retroarch path for sms when configured', () => {
    setEmulatorPath('retroarch', '/usr/bin/retroarch');
    expect(getEmulatorPathForSystem('sms')).toBe('/usr/bin/retroarch');
  });

  it('returns null for sms when retroarch path is blank', () => {
    storageData['emulator-paths'] = JSON.stringify({ retroarch: '' });
    expect(getEmulatorPathForSystem('sms')).toBeNull();
  });
});

// ---------------------------------------------------------------------------
// Fix 2 — Dreamcast achievement capability key corrected to 'dreamcast'
// ---------------------------------------------------------------------------

describe('ACHIEVEMENT_CAPABILITIES — dreamcast key fix', () => {
  it('has a "dreamcast" key (not "dc")', () => {
    expect(ACHIEVEMENT_CAPABILITIES['dreamcast']).toBeDefined();
    expect(ACHIEVEMENT_CAPABILITIES['dc']).toBeUndefined();
  });

  it('dreamcast capability has level "partial"', () => {
    expect(ACHIEVEMENT_CAPABILITIES['dreamcast'].level).toBe('partial');
  });

  it('dreamcast system field is "dreamcast"', () => {
    expect(ACHIEVEMENT_CAPABILITIES['dreamcast'].system).toBe('dreamcast');
  });
});

describe('getAchievementCapability — dreamcast resolves correctly', () => {
  it('returns partial for "dreamcast"', () => {
    const cap = getAchievementCapability('dreamcast');
    expect(cap.level).toBe('partial');
    expect(cap.system).toBe('dreamcast');
  });

  it('returns partial for "DREAMCAST" (case-insensitive)', () => {
    const cap = getAchievementCapability('DREAMCAST');
    expect(cap.level).toBe('partial');
  });

  it('no longer returns partial for the old "dc" key (it falls back to "none")', () => {
    // After the rename, 'dc' is an unknown system and should return none.
    const cap = getAchievementCapability('dc');
    expect(cap.level).toBe('none');
  });
});

describe('systemSupportsAchievements — dreamcast', () => {
  it('returns true for dreamcast (partial support)', () => {
    expect(systemSupportsAchievements('dreamcast')).toBe(true);
  });

  it('returns true for sms (full support)', () => {
    expect(systemSupportsAchievements('sms')).toBe(true);
  });

  it('returns false for wii (no support)', () => {
    expect(systemSupportsAchievements('wii')).toBe(false);
  });
});
