/**
 * Phase 21 — SEGA Genesis / Mega Drive
 *
 * Tests for:
 *  - Genesis system type in SYSTEM_INFO
 *  - RetroArch backend covers genesis
 *  - Genesis system adapter launch args (RetroArch relay)
 *  - Genesis session templates (10 templates)
 *  - Genesis mock game catalog (9 games)
 *  - No duplicate IDs introduced by Phase 21
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import { SYSTEM_INFO } from '../../../../packages/shared-types/src/systems';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';

// ---------------------------------------------------------------------------
// Genesis system type
// ---------------------------------------------------------------------------

describe('Genesis system type', () => {
  it('includes genesis in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['genesis']).toBeDefined();
    expect(SYSTEM_INFO['genesis'].name).toBe('SEGA Genesis');
    expect(SYSTEM_INFO['genesis'].shortName).toBe('Genesis');
  });

  it('Genesis is generation 4', () => {
    expect(SYSTEM_INFO['genesis'].generation).toBe(4);
  });

  it('Genesis supports up to 2 local players', () => {
    expect(SYSTEM_INFO['genesis'].maxLocalPlayers).toBe(2);
  });

  it('Genesis does not support link cable', () => {
    expect(SYSTEM_INFO['genesis'].supportsLink).toBe(false);
  });

  it('Genesis color is SEGA blue', () => {
    expect(SYSTEM_INFO['genesis'].color).toBe('#0066CC');
  });
});

// ---------------------------------------------------------------------------
// RetroArch backend — Genesis coverage
// ---------------------------------------------------------------------------

describe('RetroArch backend covers Genesis', () => {
  it('RetroArch backend systems list includes genesis', () => {
    const retroarch = KNOWN_BACKENDS.find((b) => b.id === 'retroarch');
    expect(retroarch).toBeDefined();
    expect(retroarch?.systems).toContain('genesis');
  });

  it('RetroArch supports netplay', () => {
    const retroarch = KNOWN_BACKENDS.find((b) => b.id === 'retroarch');
    expect(retroarch?.supportsNetplay).toBe(true);
  });

  it('RetroArch notes mention Genesis Plus GX', () => {
    const retroarch = KNOWN_BACKENDS.find((b) => b.id === 'retroarch');
    const notesText = retroarch?.notes?.join(' ') ?? '';
    expect(notesText).toMatch(/genesis_plus_gx/i);
  });
});

// ---------------------------------------------------------------------------
// Genesis system adapter
// ---------------------------------------------------------------------------

describe('Genesis system adapter', () => {
  const adapter = createSystemAdapter('genesis');

  it('uses retroarch as preferred backend', () => {
    expect(adapter.preferredBackendId).toBe('retroarch');
  });

  it('has no fallback backends (retroarch is primary)', () => {
    expect(adapter.fallbackBackendIds).toEqual([]);
  });

  it('builds RetroArch launch args with rom path', () => {
    const args = adapter.buildLaunchArgs('/roms/genesis/sonic2.md', {});
    expect(args).toContain('/roms/genesis/sonic2.md');
  });

  it('adds --fullscreen when fullscreen is requested', () => {
    const args = adapter.buildLaunchArgs('/roms/genesis/sonic2.md', { fullscreen: true });
    expect(args).toContain('--fullscreen');
  });

  it('adds -L flag when coreLibraryPath is provided', () => {
    const args = adapter.buildLaunchArgs('/roms/genesis/sonic2.md', {
      coreLibraryPath: '/usr/lib/libretro/genesis_plus_gx_libretro.so',
    });
    expect(args).toContain('-L');
    expect(args).toContain('/usr/lib/libretro/genesis_plus_gx_libretro.so');
  });

  it('adds --host and --port for relay host (port only, no host address)', () => {
    const args = adapter.buildLaunchArgs('/roms/genesis/sor2.bin', {
      netplayPort: 9050,
    });
    expect(args).toContain('--host');
    expect(args).toContain('--port');
    expect(args).toContain('9050');
  });

  it('adds --connect and --port for relay client', () => {
    const args = adapter.buildLaunchArgs('/roms/genesis/sor2.bin', {
      netplayHost: 'relay.retrooasis.net',
      netplayPort: 9050,
      playerSlot: 2,
    });
    // RetroArch: player slot 2 → client (connect mode)
    expect(args).toContain('--connect');
    expect(args).toContain('relay.retrooasis.net');
    expect(args).toContain('--port');
    expect(args).toContain('9050');
  });

  it('saves to genesis/ subdirectory', () => {
    const path = adapter.getSavePath('genesis-sonic-the-hedgehog-2', '/saves');
    expect(path).toBe('/saves/genesis/genesis-sonic-the-hedgehog-2');
  });

  it('does NOT use dolphin or any other non-retroarch backend', () => {
    expect(adapter.preferredBackendId).not.toBe('dolphin');
    expect(adapter.preferredBackendId).not.toBe('citra');
    expect(adapter.preferredBackendId).not.toBe('mupen64plus');
  });
});

// ---------------------------------------------------------------------------
// Session templates — Genesis
// ---------------------------------------------------------------------------

describe('Genesis session templates', () => {
  const store = new SessionTemplateStore();

  it('includes a default 2P Genesis template', () => {
    const tpl = store.get('genesis-default-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('genesis');
    expect(tpl?.playerCount).toBe(2);
    expect(tpl?.emulatorBackendId).toBe('retroarch');
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes Sonic the Hedgehog 2 2P template', () => {
    const tpl = store.get('genesis-sonic-the-hedgehog-2-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('genesis-sonic-the-hedgehog-2');
    expect(tpl?.playerCount).toBe(2);
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes Streets of Rage 2 template', () => {
    const tpl = store.get('genesis-streets-of-rage-2-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('genesis-streets-of-rage-2');
    expect(tpl?.playerCount).toBe(2);
  });

  it('includes Mortal Kombat 3 template', () => {
    const tpl = store.get('genesis-mortal-kombat-3-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('genesis-mortal-kombat-3');
    expect(tpl?.playerCount).toBe(2);
  });

  it('includes NBA Jam template', () => {
    const tpl = store.get('genesis-nba-jam-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('genesis-nba-jam');
    expect(tpl?.playerCount).toBe(2);
  });

  it('includes Contra: Hard Corps template', () => {
    const tpl = store.get('genesis-contra-hard-corps-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('genesis-contra-hard-corps');
  });

  it('includes Gunstar Heroes template', () => {
    const tpl = store.get('genesis-gunstar-heroes-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('genesis-gunstar-heroes');
  });

  it('includes Golden Axe template', () => {
    const tpl = store.get('genesis-golden-axe-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('genesis-golden-axe');
  });

  it('includes ToeJam & Earl template', () => {
    const tpl = store.get('genesis-toejam-and-earl-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('genesis-toejam-and-earl');
  });

  it('includes Earthworm Jim 2 template', () => {
    const tpl = store.get('genesis-earthworm-jim-2-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('genesis-earthworm-jim-2');
  });

  it('getForGame resolves genesis-default as fallback', () => {
    const tpl = store.getForGame('genesis-unknown-game', 'genesis');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('genesis');
  });

  it('all Genesis relay templates use retroarch backend', () => {
    const genesisTemplates = store.listAll().filter((t) => t.system === 'genesis');
    expect(genesisTemplates.length).toBeGreaterThan(0);
    for (const tpl of genesisTemplates) {
      expect(tpl.emulatorBackendId).toBe('retroarch');
    }
  });

  it('all Genesis templates have latencyTarget of 100ms', () => {
    const genesisTemplates = store.listAll().filter((t) => t.system === 'genesis');
    for (const tpl of genesisTemplates) {
      expect(tpl.latencyTarget).toBe(100);
    }
  });

  it('all Genesis templates use online-relay netplay', () => {
    const genesisTemplates = store.listAll().filter((t) => t.system === 'genesis');
    for (const tpl of genesisTemplates) {
      expect(tpl.netplayMode).toBe('online-relay');
    }
  });

  it('there are exactly 10 Genesis templates (1 default + 9 game-specific)', () => {
    const genesisTemplates = store.listAll().filter((t) => t.system === 'genesis');
    expect(genesisTemplates.length).toBe(10);
  });
});

// ---------------------------------------------------------------------------
// Mock game catalog — Genesis games
// ---------------------------------------------------------------------------

describe('Genesis mock game catalog', () => {
  const genesisGames = MOCK_GAMES.filter((g) => g.system === 'Genesis');

  it('includes at least 9 Genesis games in the catalog', () => {
    expect(genesisGames.length).toBeGreaterThanOrEqual(9);
  });

  it('includes Sonic the Hedgehog 2', () => {
    const game = genesisGames.find((g) => g.id === 'genesis-sonic-the-hedgehog-2');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(2);
  });

  it('includes Streets of Rage 2', () => {
    const game = genesisGames.find((g) => g.id === 'genesis-streets-of-rage-2');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(2);
  });

  it('includes Mortal Kombat 3', () => {
    const game = genesisGames.find((g) => g.id === 'genesis-mortal-kombat-3');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(2);
  });

  it('includes NBA Jam', () => {
    const game = genesisGames.find((g) => g.id === 'genesis-nba-jam');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(2);
  });

  it('includes Contra: Hard Corps', () => {
    const game = genesisGames.find((g) => g.id === 'genesis-contra-hard-corps');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(2);
  });

  it('includes Gunstar Heroes', () => {
    const game = genesisGames.find((g) => g.id === 'genesis-gunstar-heroes');
    expect(game).toBeDefined();
  });

  it('includes Golden Axe', () => {
    const game = genesisGames.find((g) => g.id === 'genesis-golden-axe');
    expect(game).toBeDefined();
  });

  it('includes ToeJam & Earl', () => {
    const game = genesisGames.find((g) => g.id === 'genesis-toejam-and-earl');
    expect(game).toBeDefined();
  });

  it('includes Earthworm Jim 2', () => {
    const game = genesisGames.find((g) => g.id === 'genesis-earthworm-jim-2');
    expect(game).toBeDefined();
  });

  it('all Genesis games have the correct system label', () => {
    for (const game of genesisGames) {
      expect(game.system).toBe('Genesis');
    }
  });

  it('all Genesis games have systemColor #0066CC', () => {
    for (const game of genesisGames) {
      expect(game.systemColor).toBe('#0066CC');
    }
  });
});

// ---------------------------------------------------------------------------
// Regression: no duplicate mock-game IDs introduced by Phase 21
// ---------------------------------------------------------------------------

describe('Mock game catalog — no new duplicates (Phase 21)', () => {
  it('has no duplicate IDs across the entire catalog', () => {
    const ids = MOCK_GAMES.map((g) => g.id);
    const unique = new Set(ids);
    expect(unique.size).toBe(ids.length);
  });
});
