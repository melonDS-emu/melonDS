/**
 * Phase 27 — Nintendo Wii U support (Cemu emulator)
 *
 * Tests for:
 *  - wiiu system type in SYSTEM_INFO (name, shortName, generation, maxLocalPlayers, color)
 *  - Cemu backend in KNOWN_BACKENDS
 *  - RetroArch backend now includes wiiu in systems list
 *  - System adapter for wiiu (Cemu launch args)
 *  - Session templates (10 Wii U templates)
 *  - Mock games (9 Wii U games)
 *  - No duplicate IDs introduced by Phase 27
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import { SYSTEM_INFO } from '../../../../packages/shared-types/src/systems';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';

// ---------------------------------------------------------------------------
// Wii U system type
// ---------------------------------------------------------------------------

describe('Wii U system type', () => {
  it('includes wiiu in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['wiiu']).toBeDefined();
    expect(SYSTEM_INFO['wiiu'].name).toBe('Nintendo Wii U');
    expect(SYSTEM_INFO['wiiu'].shortName).toBe('Wii U');
  });

  it('Wii U is generation 8', () => {
    expect(SYSTEM_INFO['wiiu'].generation).toBe(8);
  });

  it('Wii U supports up to 5 local players (GamePad + 4 Wiimotes)', () => {
    expect(SYSTEM_INFO['wiiu'].maxLocalPlayers).toBe(5);
  });

  it('Wii U does not support link cable', () => {
    expect(SYSTEM_INFO['wiiu'].supportsLink).toBe(false);
  });

  it('Wii U color is Wii U blue', () => {
    expect(SYSTEM_INFO['wiiu'].color).toBe('#009AC7');
  });
});

// ---------------------------------------------------------------------------
// Cemu backend
// ---------------------------------------------------------------------------

describe('Cemu backend', () => {
  it('includes cemu in KNOWN_BACKENDS', () => {
    const cemu = KNOWN_BACKENDS.find((b) => b.id === 'cemu');
    expect(cemu).toBeDefined();
  });

  it('Cemu supports the wiiu system', () => {
    const cemu = KNOWN_BACKENDS.find((b) => b.id === 'cemu');
    expect(cemu?.systems).toContain('wiiu');
  });

  it('Cemu executable is named Cemu', () => {
    const cemu = KNOWN_BACKENDS.find((b) => b.id === 'cemu');
    expect(cemu?.executableName).toBe('Cemu');
  });

  it('Cemu supports save states', () => {
    const cemu = KNOWN_BACKENDS.find((b) => b.id === 'cemu');
    expect(cemu?.supportsSaveStates).toBe(true);
  });

  it('RetroArch includes wiiu in its systems list', () => {
    const retroarch = KNOWN_BACKENDS.find((b) => b.id === 'retroarch');
    expect(retroarch?.systems).toContain('wiiu');
  });
});

// ---------------------------------------------------------------------------
// Wii U system adapter
// ---------------------------------------------------------------------------

describe('Wii U system adapter', () => {
  it('creates a Wii U adapter without error', () => {
    expect(() => createSystemAdapter('wiiu')).not.toThrow();
  });

  it('adapter uses cemu as preferred backend', () => {
    const adapter = createSystemAdapter('wiiu');
    expect(adapter.preferredBackendId).toBe('cemu');
  });

  it('adapter falls back to retroarch', () => {
    const adapter = createSystemAdapter('wiiu');
    expect(adapter.fallbackBackendIds).toContain('retroarch');
  });

  it('builds Cemu launch args with -g flag', () => {
    const adapter = createSystemAdapter('wiiu');
    const args = adapter.buildLaunchArgs('/roms/wiiu/mk8.wux', {});
    expect(args).toContain('-g');
    expect(args).toContain('/roms/wiiu/mk8.wux');
  });

  it('adds -f flag when fullscreen requested', () => {
    const adapter = createSystemAdapter('wiiu');
    const args = adapter.buildLaunchArgs('/roms/wiiu/mk8.wux', { fullscreen: true });
    expect(args).toContain('-f');
  });

  it('adds --verbose flag when debug requested', () => {
    const adapter = createSystemAdapter('wiiu');
    const args = adapter.buildLaunchArgs('/roms/wiiu/mk8.wux', { debug: true });
    expect(args).toContain('--verbose');
  });

  it('returns retroarch args when retroarch backend specified', () => {
    const adapter = createSystemAdapter('wiiu', 'retroarch');
    const args = adapter.buildLaunchArgs('/roms/wiiu/mk8.wux', {});
    // RetroArch args start with -L (core) or the rom path
    expect(args.length).toBeGreaterThan(0);
  });

  it('getSavePath returns wiiu subdirectory', () => {
    const adapter = createSystemAdapter('wiiu');
    const savePath = adapter.getSavePath('wiiu-mario-kart-8', '/saves');
    expect(savePath).toContain('wiiu');
  });
});

// ---------------------------------------------------------------------------
// Wii U session templates
// ---------------------------------------------------------------------------

describe('Wii U session templates', () => {
  const store = new SessionTemplateStore();

  it('includes wiiu-default-2p template', () => {
    expect(store.get('wiiu-default-2p')).not.toBeNull();
  });

  it('includes wiiu-default-4p template', () => {
    expect(store.get('wiiu-default-4p')).not.toBeNull();
  });

  it('includes wiiu-mario-kart-8-4p template', () => {
    expect(store.get('wiiu-mario-kart-8-4p')).not.toBeNull();
  });

  it('includes wiiu-super-smash-bros-wiiu-4p template', () => {
    expect(store.get('wiiu-super-smash-bros-wiiu-4p')).not.toBeNull();
  });

  it('includes wiiu-super-smash-bros-wiiu-8p with 8 players', () => {
    const tpl = store.get('wiiu-super-smash-bros-wiiu-8p');
    expect(tpl).not.toBeNull();
    expect(tpl?.playerCount).toBe(8);
  });

  it('includes wiiu-new-super-mario-bros-u-4p template', () => {
    expect(store.get('wiiu-new-super-mario-bros-u-4p')).not.toBeNull();
  });

  it('includes wiiu-nintendo-land-4p template', () => {
    expect(store.get('wiiu-nintendo-land-4p')).not.toBeNull();
  });

  it('includes wiiu-splatoon-4p template', () => {
    expect(store.get('wiiu-splatoon-4p')).not.toBeNull();
  });

  it('includes wiiu-pikmin-3-2p template', () => {
    expect(store.get('wiiu-pikmin-3-2p')).not.toBeNull();
  });

  it('includes wiiu-mario-party-10-4p template', () => {
    expect(store.get('wiiu-mario-party-10-4p')).not.toBeNull();
  });

  it('all Wii U templates use cemu as emulatorBackendId', () => {
    const wiiuTemplates = store.listAll().filter((t) => t.system === 'wiiu');
    expect(wiiuTemplates.length).toBeGreaterThan(0);
    for (const tpl of wiiuTemplates) {
      expect(tpl.emulatorBackendId).toBe('cemu');
    }
  });

  it('all Wii U templates use online-relay netplay mode', () => {
    const wiiuTemplates = store.listAll().filter((t) => t.system === 'wiiu');
    for (const tpl of wiiuTemplates) {
      expect(tpl.netplayMode).toBe('online-relay');
    }
  });

  it('getForGame returns template for wiiu-mario-kart-8', () => {
    const tpl = store.getForGame('wiiu-mario-kart-8');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('wiiu');
  });
});

// ---------------------------------------------------------------------------
// Wii U mock games
// ---------------------------------------------------------------------------

describe('Wii U mock games', () => {
  const wiiuGames = MOCK_GAMES.filter((g) => g.system === 'Wii U');

  it('includes 9 Wii U games in the catalog', () => {
    expect(wiiuGames.length).toBe(9);
  });

  it('includes Mario Kart 8', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-mario-kart-8')).toBeDefined();
  });

  it('includes Super Smash Bros. for Wii U', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-super-smash-bros-wiiu')).toBeDefined();
  });

  it('includes New Super Mario Bros. U', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-new-super-mario-bros-u')).toBeDefined();
  });

  it('includes Splatoon', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-splatoon')).toBeDefined();
  });

  it('all Wii U games use correct systemColor', () => {
    for (const game of wiiuGames) {
      expect(game.systemColor).toBe('#009AC7');
    }
  });

  it('Super Smash Bros. supports 8 players', () => {
    const smash = wiiuGames.find((g) => g.id === 'wiiu-super-smash-bros-wiiu');
    expect(smash?.maxPlayers).toBe(8);
  });

  it('Mario Kart 8 supports 4 players', () => {
    const mk8 = wiiuGames.find((g) => g.id === 'wiiu-mario-kart-8');
    expect(mk8?.maxPlayers).toBe(4);
  });
});

// ---------------------------------------------------------------------------
// Regression: no duplicate IDs introduced by Phase 27
// ---------------------------------------------------------------------------

describe('Mock game catalog — no new duplicates (Phase 27)', () => {
  it('has no duplicate IDs across the entire catalog', () => {
    const ids = MOCK_GAMES.map((g) => g.id);
    const unique = new Set(ids);
    expect(unique.size).toBe(ids.length);
  });
});
