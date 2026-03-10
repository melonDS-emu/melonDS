/**
 * Tests for the custom controller profile persistence service.
 */
import { describe, it, expect, beforeEach } from 'vitest';
import type { InputProfile } from '@retro-oasis/multiplayer-profiles';
import {
  getCustomProfile,
  saveCustomProfile,
  resetCustomProfile,
  listCustomProfileIds,
  mergeWithCustomProfiles,
} from '../custom-profiles';

// Minimal mock of localStorage for the test environment
const store: Record<string, string> = {};
const mockLocalStorage = {
  getItem: (key: string) => store[key] ?? null,
  setItem: (key: string, value: string) => { store[key] = value; },
  removeItem: (key: string) => { delete store[key]; },
  clear: () => { Object.keys(store).forEach((k) => delete store[k]); },
  get length() { return Object.keys(store).length; },
  key: (i: number) => Object.keys(store)[i] ?? null,
};
Object.defineProperty(globalThis, 'localStorage', { value: mockLocalStorage, writable: false });

function makeProfile(id: string, action: string, key: string): InputProfile {
  return {
    id,
    name: `Test ${id}`,
    controllerType: 'keyboard',
    system: 'n64',
    isDefault: true,
    bindings: [{ action, key: `key(${key})` }],
  };
}

describe('custom-profiles', () => {
  beforeEach(() => {
    mockLocalStorage.clear();
  });

  it('returns null for a profile with no custom override', () => {
    expect(getCustomProfile('n64-keyboard-default')).toBeNull();
  });

  it('saves and retrieves a custom profile', () => {
    const profile = makeProfile('n64-keyboard-default', 'A', 'x');
    saveCustomProfile(profile);
    const loaded = getCustomProfile('n64-keyboard-default');
    expect(loaded).not.toBeNull();
    expect(loaded?.bindings[0].key).toBe('key(x)');
  });

  it('overwrites an existing override on re-save', () => {
    saveCustomProfile(makeProfile('n64-keyboard-default', 'A', 'x'));
    saveCustomProfile(makeProfile('n64-keyboard-default', 'A', 'z'));
    const loaded = getCustomProfile('n64-keyboard-default');
    expect(loaded?.bindings[0].key).toBe('key(z)');
  });

  it('resets a custom override', () => {
    saveCustomProfile(makeProfile('n64-keyboard-default', 'A', 'x'));
    resetCustomProfile('n64-keyboard-default');
    expect(getCustomProfile('n64-keyboard-default')).toBeNull();
  });

  it('lists all custom profile IDs', () => {
    saveCustomProfile(makeProfile('profile-a', 'A', 'x'));
    saveCustomProfile(makeProfile('profile-b', 'B', 'y'));
    const ids = listCustomProfileIds();
    expect(ids).toContain('profile-a');
    expect(ids).toContain('profile-b');
  });

  it('mergeWithCustomProfiles returns defaults when no overrides exist', () => {
    const defaults = [makeProfile('n64-xbox-default', 'A', 'a')];
    const merged = mergeWithCustomProfiles(defaults);
    expect(merged[0].bindings[0].key).toBe('key(a)');
  });

  it('mergeWithCustomProfiles replaces a default with its override', () => {
    const defaults = [makeProfile('n64-xbox-default', 'A', 'a')];
    const custom = makeProfile('n64-xbox-default', 'A', 'z');
    saveCustomProfile(custom);
    const merged = mergeWithCustomProfiles(defaults);
    expect(merged[0].bindings[0].key).toBe('key(z)');
  });

  it('mergeWithCustomProfiles leaves non-overridden profiles untouched', () => {
    const defaults = [
      makeProfile('n64-xbox-default', 'A', 'a'),
      makeProfile('n64-keyboard-default', 'A', 'x'),
    ];
    saveCustomProfile(makeProfile('n64-xbox-default', 'A', 'z'));
    const merged = mergeWithCustomProfiles(defaults);
    expect(merged[0].bindings[0].key).toBe('key(z)');  // overridden
    expect(merged[1].bindings[0].key).toBe('key(x)');  // untouched
  });
});
