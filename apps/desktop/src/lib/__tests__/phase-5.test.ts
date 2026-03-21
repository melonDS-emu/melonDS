/**
 * Phase 5 — Curated Multiplayer UX
 *
 * Tests for:
 *  - SystemBadge: SYSTEM_COLORS map covers all expected systems
 *  - getGamePresets: generates correct presets for different game configurations
 *  - bestWithFriends filter logic (pure-data unit tests)
 *  - MODES list completeness
 */

import { describe, it, expect } from 'vitest';
import { getGamePresets } from '../session-presets';
import { SYSTEM_COLORS } from '../../components/SystemBadge';

// ---------------------------------------------------------------------------
// localStorage mock
// ---------------------------------------------------------------------------

const store: Record<string, string> = {};
Object.defineProperty(globalThis, 'localStorage', {
  value: {
    getItem: (key: string) => store[key] ?? null,
    setItem: (key: string, value: string) => { store[key] = value; },
    removeItem: (key: string) => { delete store[key]; },
    clear: () => { Object.keys(store).forEach((k) => delete store[k]); },
  },
  writable: true,
});

// ---------------------------------------------------------------------------
// SystemBadge — SYSTEM_COLORS coverage
// ---------------------------------------------------------------------------

describe('SYSTEM_COLORS', () => {
  const EXPECTED_SYSTEMS = [
    'NES', 'SNES', 'GB', 'GBC', 'GBA', 'N64', 'NDS',
    'GC', '3DS', 'Wii', 'Wii U', 'Genesis', 'Dreamcast',
    'PSX', 'PS2', 'PSP',
  ];

  for (const system of EXPECTED_SYSTEMS) {
    it(`has a colour entry for ${system}`, () => {
      expect(SYSTEM_COLORS[system]).toBeDefined();
      expect(typeof SYSTEM_COLORS[system]).toBe('string');
      expect(SYSTEM_COLORS[system].length).toBeGreaterThan(0);
    });
  }
});

// ---------------------------------------------------------------------------
// getGamePresets — session preset generation
// ---------------------------------------------------------------------------

describe('getGamePresets', () => {
  it('returns full-party + 4-player + 1-on-1 for a 4P party game', () => {
    const presets = getGamePresets({ maxPlayers: 4, tags: ['Party', 'Versus'], badges: [] });
    expect(presets).toHaveLength(3);
    const ids = presets.map((p) => p.id);
    expect(ids).toContain('full-party');
    expect(ids).toContain('four-player');
    expect(ids).toContain('two-player');
  });

  it('full-party preset playerCount equals game maxPlayers', () => {
    const presets = getGamePresets({ maxPlayers: 4, tags: ['Party'], badges: [] });
    const fp = presets.find((p) => p.id === 'full-party');
    expect(fp).toBeDefined();
    expect(fp!.playerCount).toBe(4);
  });

  it('does not include full-party or four-player for 2P-only game', () => {
    const presets = getGamePresets({ maxPlayers: 2, tags: ['Versus'], badges: [] });
    const ids = presets.map((p) => p.id);
    expect(ids).not.toContain('full-party');
    expect(ids).not.toContain('four-player');
    expect(ids).toContain('two-player');
  });

  it('returns only 1-on-1 for a strict 2P game', () => {
    const presets = getGamePresets({ maxPlayers: 2, tags: ['Link', 'Trade'], badges: [] });
    expect(presets).toHaveLength(1);
    expect(presets[0].id).toBe('two-player');
    expect(presets[0].playerCount).toBe(2);
  });

  it('each preset has all required fields', () => {
    const presets = getGamePresets({ maxPlayers: 4, tags: ['Party'], badges: [] });
    for (const p of presets) {
      expect(typeof p.id).toBe('string');
      expect(typeof p.label).toBe('string');
      expect(typeof p.description).toBe('string');
      expect(typeof p.playerCount).toBe('number');
      expect(typeof p.emoji).toBe('string');
    }
  });

  it('returns empty array for single-player games (maxPlayers < 2)', () => {
    const presets = getGamePresets({ maxPlayers: 1, tags: [], badges: [] });
    expect(presets).toHaveLength(0);
  });

  it('full-party only included when tags contain Party or Battle', () => {
    const withBattle = getGamePresets({ maxPlayers: 4, tags: ['Battle'], badges: [] });
    expect(withBattle.map((p) => p.id)).toContain('full-party');

    const noBattleOrParty = getGamePresets({ maxPlayers: 4, tags: ['Co-op'], badges: [] });
    expect(noBattleOrParty.map((p) => p.id)).not.toContain('full-party');
  });

  it('4-player preset included for any 4P+ game regardless of tags', () => {
    const presets = getGamePresets({ maxPlayers: 8, tags: ['Co-op'], badges: [] });
    expect(presets.map((p) => p.id)).toContain('four-player');
  });
});

// ---------------------------------------------------------------------------
// Best-with-Friends filter logic (pure data, no DOM)
// ---------------------------------------------------------------------------

describe('bestWithFriends filter', () => {
  const mockGames = [
    { id: 'g1', system: 'N64', tags: ['Party'], badges: ['Best with Friends', 'Party Favorite'] },
    { id: 'g2', system: 'SNES', tags: ['Versus'], badges: ['Great Online'] },
    { id: 'g3', system: 'NDS', tags: ['Party', 'WFC'], badges: ['Best with Friends'] },
    { id: 'g4', system: 'GBA', tags: ['Link'], badges: [] },
  ];

  it('filters to games with "Best with Friends" badge', () => {
    const result = mockGames.filter((g) => g.badges.includes('Best with Friends'));
    expect(result).toHaveLength(2);
    expect(result.map((g) => g.id)).toEqual(['g1', 'g3']);
  });

  it('intersects with system filter', () => {
    const result = mockGames.filter(
      (g) => g.badges.includes('Best with Friends') && g.system === 'N64',
    );
    expect(result).toHaveLength(1);
    expect(result[0].id).toBe('g1');
  });

  it('intersects with tag filter', () => {
    const result = mockGames.filter(
      (g) => g.badges.includes('Best with Friends') && g.tags.includes('WFC'),
    );
    expect(result).toHaveLength(1);
    expect(result[0].id).toBe('g3');
  });

  it('returns all matching when system is All and tag is All', () => {
    const selectedSystem = 'All';
    const selectedTag = 'All';
    const result = mockGames.filter(
      (g) =>
        g.badges.includes('Best with Friends') &&
        (selectedSystem === 'All' || g.system === selectedSystem) &&
        (selectedTag === 'All' || g.tags.includes(selectedTag)),
    );
    expect(result).toHaveLength(2);
  });
});

// ---------------------------------------------------------------------------
// Multiplayer mode list completeness (plain-data mirror of LibraryPage MODES)
// ---------------------------------------------------------------------------

describe('multiplayer mode list', () => {
  const MODES = [
    { tag: 'All',     label: 'All Games', icon: '🎮' },
    { tag: 'Party',   label: 'Party',     icon: '🎉' },
    { tag: 'Co-op',   label: 'Co-op',     icon: '🤝' },
    { tag: 'Versus',  label: 'Versus',    icon: '⚔️' },
    { tag: 'Battle',  label: 'Battle',    icon: '💥' },
    { tag: 'Link',    label: 'Link',      icon: '🔗' },
    { tag: 'Trade',   label: 'Trade',     icon: '🔄' },
  ];

  it('contains all expected mode tags', () => {
    const tags = MODES.map((m) => m.tag);
    for (const expected of ['All', 'Party', 'Co-op', 'Versus', 'Battle', 'Link', 'Trade']) {
      expect(tags).toContain(expected);
    }
  });

  it('every mode has a non-empty label and icon', () => {
    for (const mode of MODES) {
      expect(mode.label.length).toBeGreaterThan(0);
      expect(mode.icon.length).toBeGreaterThan(0);
    }
  });
});
