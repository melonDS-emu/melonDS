/**
 * Phase 19 — Nintendo Wii (Dolphin) emulator integration
 *
 * Tests for: session templates (wii), system type definitions,
 * emulator adapter launch args, Dolphin backend Wii coverage,
 * and mock game catalog entries.
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import { SYSTEM_INFO } from '../../../../packages/shared-types/src/systems';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';

// ---------------------------------------------------------------------------
// System type definitions
// ---------------------------------------------------------------------------

describe('Wii system type', () => {
  it('includes wii in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['wii']).toBeDefined();
    expect(SYSTEM_INFO['wii'].name).toBe('Nintendo Wii');
    expect(SYSTEM_INFO['wii'].shortName).toBe('Wii');
    expect(SYSTEM_INFO['wii'].generation).toBe(7);
    expect(SYSTEM_INFO['wii'].maxLocalPlayers).toBe(4);
  });

  it('Wii is generation 7 (same as NDS)', () => {
    expect(SYSTEM_INFO['wii'].generation).toBe(7);
    expect(SYSTEM_INFO['nds'].generation).toBe(7);
  });
});

// ---------------------------------------------------------------------------
// Dolphin backend — Wii coverage
// ---------------------------------------------------------------------------

describe('Dolphin backend covers Wii', () => {
  it('Dolphin backend systems list includes wii', () => {
    const dolphin = KNOWN_BACKENDS.find((b) => b.id === 'dolphin');
    expect(dolphin).toBeDefined();
    expect(dolphin?.systems).toContain('wii');
    expect(dolphin?.systems).toContain('gc');
  });

  it('RetroArch backend systems list includes wii', () => {
    const retroarch = KNOWN_BACKENDS.find((b) => b.id === 'retroarch');
    expect(retroarch).toBeDefined();
    expect(retroarch?.systems).toContain('wii');
  });

  it('Dolphin supports netplay', () => {
    const dolphin = KNOWN_BACKENDS.find((b) => b.id === 'dolphin');
    expect(dolphin?.supportsNetplay).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Wii system adapter launch args
// ---------------------------------------------------------------------------

describe('Wii system adapter', () => {
  const adapter = createSystemAdapter('wii');

  it('uses dolphin as preferred backend', () => {
    expect(adapter.preferredBackendId).toBe('dolphin');
  });

  it('falls back to retroarch', () => {
    expect(adapter.fallbackBackendIds).toContain('retroarch');
  });

  it('builds Dolphin batch-mode launch args with rom path', () => {
    const args = adapter.buildLaunchArgs('/roms/wii/mario-kart-wii.iso', {});
    expect(args).toContain('-b');
    expect(args).toContain('-e');
    expect(args).toContain('/roms/wii/mario-kart-wii.iso');
  });

  it('adds --no-gui when fullscreen is requested', () => {
    const args = adapter.buildLaunchArgs('/roms/wii/smash-brawl.iso', { fullscreen: true });
    expect(args).toContain('--no-gui');
  });

  it('does NOT add netplay host/port flags (relay-handled)', () => {
    const args = adapter.buildLaunchArgs('/roms/wii/mkwii.iso', {
      netplayHost: '127.0.0.1',
      netplayPort: 9100,
    });
    // Wii relay is TCP-level; no CLI netplay flags
    expect(args.join(' ')).not.toMatch(/--netplay/);
    expect(args.join(' ')).not.toMatch(/--connect/);
  });

  it('adds --wii-motionplus when wiiMotionPlus is true', () => {
    const args = adapter.buildLaunchArgs('/roms/wii/sports-resort.iso', { wiiMotionPlus: true });
    expect(args).toContain('--wii-motionplus');
  });

  it('does NOT add --wii-motionplus by default', () => {
    const args = adapter.buildLaunchArgs('/roms/wii/sports.iso', {});
    expect(args).not.toContain('--wii-motionplus');
  });

  it('adds --debugger when debug is true', () => {
    const args = adapter.buildLaunchArgs('/roms/wii/mkwii.iso', { debug: true });
    expect(args).toContain('--debugger');
  });

  it('uses retroarch args when backend override is retroarch', () => {
    const raAdapter = createSystemAdapter('wii', 'retroarch');
    const args = raAdapter.buildLaunchArgs('/roms/wii/mkwii.iso', {
      coreLibraryPath: '/usr/lib/libretro/dolphin_libretro.so',
    });
    expect(args).toContain('-L');
    expect(args).toContain('/usr/lib/libretro/dolphin_libretro.so');
  });

  it('saves to wii/ subdirectory', () => {
    const path = adapter.getSavePath('wii-mario-kart-wii', '/saves');
    expect(path).toBe('/saves/wii/wii-mario-kart-wii');
  });
});

// ---------------------------------------------------------------------------
// Session templates — Nintendo Wii
// ---------------------------------------------------------------------------

describe('Nintendo Wii session templates', () => {
  const store = new SessionTemplateStore();

  it('includes a default 2P Wii template', () => {
    const tpl = store.get('wii-default-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('wii');
    expect(tpl?.playerCount).toBe(2);
    expect(tpl?.emulatorBackendId).toBe('dolphin');
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes a default 4P Wii template', () => {
    const tpl = store.get('wii-default-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('wii');
    expect(tpl?.playerCount).toBe(4);
    expect(tpl?.emulatorBackendId).toBe('dolphin');
  });

  it('includes Mario Kart Wii 4P template', () => {
    const tpl = store.get('wii-mario-kart-wii-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('wii-mario-kart-wii');
    expect(tpl?.playerCount).toBe(4);
    expect(tpl?.emulatorBackendId).toBe('dolphin');
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes Super Smash Bros. Brawl 4P template', () => {
    const tpl = store.get('wii-super-smash-bros-brawl-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('wii-super-smash-bros-brawl');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes New Super Mario Bros. Wii 4P template', () => {
    const tpl = store.get('wii-new-super-mario-bros-wii-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('wii-new-super-mario-bros-wii');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Wii Sports 4P template', () => {
    const tpl = store.get('wii-wii-sports-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('wii-wii-sports');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Wii Sports Resort 4P template', () => {
    const tpl = store.get('wii-wii-sports-resort-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('wii-wii-sports-resort');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Mario Party 8 and 9 4P templates', () => {
    for (const id of ['wii-mario-party-8-4p', 'wii-mario-party-9-4p']) {
      const tpl = store.get(id);
      expect(tpl).not.toBeNull();
      expect(tpl?.system).toBe('wii');
      expect(tpl?.playerCount).toBe(4);
    }
  });

  it('includes Donkey Kong Country Returns 2P template', () => {
    const tpl = store.get('wii-donkey-kong-country-returns-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('wii-donkey-kong-country-returns');
    expect(tpl?.playerCount).toBe(2);
  });

  it("includes Kirby's Return to Dream Land 4P template", () => {
    const tpl = store.get('wii-kirby-return-to-dream-land-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.playerCount).toBe(4);
  });

  it('getForGame resolves wii-default as fallback', () => {
    const tpl = store.getForGame('wii-unknown-game', 'wii');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('wii');
  });

  it('all Wii relay templates have latencyTarget of 120ms', () => {
    const wiiTemplates = store.listAll().filter((t) => t.system === 'wii' && t.netplayMode === 'online-relay');
    expect(wiiTemplates.length).toBeGreaterThan(0);
    for (const tpl of wiiTemplates) {
      expect(tpl.latencyTarget).toBe(120);
    }
  });
});

// ---------------------------------------------------------------------------
// Mock game catalog — Wii games
// ---------------------------------------------------------------------------

describe('Wii mock game catalog', () => {
  const wiiGames = MOCK_GAMES.filter((g) => g.system === 'Wii');

  it('includes at least 9 Wii games in the catalog', () => {
    expect(wiiGames.length).toBeGreaterThanOrEqual(9);
  });

  it('includes Mario Kart Wii', () => {
    const game = wiiGames.find((g) => g.id === 'wii-mario-kart-wii');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(4);
  });

  it('includes Super Smash Bros. Brawl', () => {
    const game = wiiGames.find((g) => g.id === 'wii-super-smash-bros-brawl');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(4);
  });

  it('includes New Super Mario Bros. Wii', () => {
    const game = wiiGames.find((g) => g.id === 'wii-new-super-mario-bros-wii');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(4);
  });

  it('includes Wii Sports', () => {
    const game = wiiGames.find((g) => g.id === 'wii-wii-sports');
    expect(game).toBeDefined();
  });

  it('includes Wii Sports Resort', () => {
    const game = wiiGames.find((g) => g.id === 'wii-wii-sports-resort');
    expect(game).toBeDefined();
    expect(game?.tags).toContain('MotionPlus');
  });

  it('all Wii games have the correct systemColor', () => {
    for (const game of wiiGames) {
      expect(game.systemColor).toBe('#E4E4E4');
    }
  });

  it('Donkey Kong Country Returns is 2P co-op', () => {
    const game = wiiGames.find((g) => g.id === 'wii-donkey-kong-country-returns');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(2);
  });
});
