/**
 * Phase 22 — SEGA Dreamcast, Sony PlayStation (PSX), PlayStation 2 (PS2), PlayStation Portable (PSP)
 *
 * Tests for:
 *  - Each new system type in SYSTEM_INFO (name, shortName, generation, maxLocalPlayers, color)
 *  - New backends in KNOWN_BACKENDS (flycast, duckstation, pcsx2, ppsspp)
 *  - RetroArch backend now includes all 4 new systems
 *  - System adapters for dreamcast, psx, ps2, psp
 *  - Session templates (10 per system = 40 new templates)
 *  - Mock games (9 per system = 36 new games)
 *  - No duplicate IDs introduced by Phase 22
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import { SYSTEM_INFO } from '../../../../packages/shared-types/src/systems';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';

// ---------------------------------------------------------------------------
// Dreamcast system type
// ---------------------------------------------------------------------------

describe('Dreamcast system type', () => {
  it('includes dreamcast in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['dreamcast']).toBeDefined();
    expect(SYSTEM_INFO['dreamcast'].name).toBe('SEGA Dreamcast');
    expect(SYSTEM_INFO['dreamcast'].shortName).toBe('Dreamcast');
  });

  it('Dreamcast is generation 6', () => {
    expect(SYSTEM_INFO['dreamcast'].generation).toBe(6);
  });

  it('Dreamcast supports up to 4 local players', () => {
    expect(SYSTEM_INFO['dreamcast'].maxLocalPlayers).toBe(4);
  });

  it('Dreamcast does not support link cable', () => {
    expect(SYSTEM_INFO['dreamcast'].supportsLink).toBe(false);
  });

  it('Dreamcast color is SEGA orange', () => {
    expect(SYSTEM_INFO['dreamcast'].color).toBe('#FF6600');
  });
});

// ---------------------------------------------------------------------------
// PSX system type
// ---------------------------------------------------------------------------

describe('PSX system type', () => {
  it('includes psx in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['psx']).toBeDefined();
    expect(SYSTEM_INFO['psx'].name).toBe('Sony PlayStation');
    expect(SYSTEM_INFO['psx'].shortName).toBe('PSX');
  });

  it('PSX is generation 5', () => {
    expect(SYSTEM_INFO['psx'].generation).toBe(5);
  });

  it('PSX supports up to 4 local players', () => {
    expect(SYSTEM_INFO['psx'].maxLocalPlayers).toBe(4);
  });

  it('PSX does not support link cable', () => {
    expect(SYSTEM_INFO['psx'].supportsLink).toBe(false);
  });

  it('PSX color is gray', () => {
    expect(SYSTEM_INFO['psx'].color).toBe('#808080');
  });
});

// ---------------------------------------------------------------------------
// PS2 system type
// ---------------------------------------------------------------------------

describe('PS2 system type', () => {
  it('includes ps2 in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['ps2']).toBeDefined();
    expect(SYSTEM_INFO['ps2'].name).toBe('Sony PlayStation 2');
    expect(SYSTEM_INFO['ps2'].shortName).toBe('PS2');
  });

  it('PS2 is generation 6', () => {
    expect(SYSTEM_INFO['ps2'].generation).toBe(6);
  });

  it('PS2 supports up to 4 local players', () => {
    expect(SYSTEM_INFO['ps2'].maxLocalPlayers).toBe(4);
  });

  it('PS2 does not support link cable', () => {
    expect(SYSTEM_INFO['ps2'].supportsLink).toBe(false);
  });

  it('PS2 color is PlayStation blue', () => {
    expect(SYSTEM_INFO['ps2'].color).toBe('#00439C');
  });
});

// ---------------------------------------------------------------------------
// PSP system type
// ---------------------------------------------------------------------------

describe('PSP system type', () => {
  it('includes psp in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['psp']).toBeDefined();
    expect(SYSTEM_INFO['psp'].name).toBe('Sony PlayStation Portable');
    expect(SYSTEM_INFO['psp'].shortName).toBe('PSP');
  });

  it('PSP is generation 7', () => {
    expect(SYSTEM_INFO['psp'].generation).toBe(7);
  });

  it('PSP supports up to 6 local players', () => {
    expect(SYSTEM_INFO['psp'].maxLocalPlayers).toBe(6);
  });

  it('PSP does not support link cable', () => {
    expect(SYSTEM_INFO['psp'].supportsLink).toBe(false);
  });

  it('PSP color is near-black', () => {
    expect(SYSTEM_INFO['psp'].color).toBe('#1C1C1C');
  });
});

// ---------------------------------------------------------------------------
// New backends
// ---------------------------------------------------------------------------

describe('Flycast backend', () => {
  const flycast = KNOWN_BACKENDS.find((b) => b.id === 'flycast');

  it('flycast backend is defined', () => {
    expect(flycast).toBeDefined();
    expect(flycast?.name).toBe('Flycast');
  });

  it('flycast covers dreamcast', () => {
    expect(flycast?.systems).toContain('dreamcast');
  });

  it('flycast supports netplay', () => {
    expect(flycast?.supportsNetplay).toBe(true);
  });

  it('flycast supports save states', () => {
    expect(flycast?.supportsSaveStates).toBe(true);
  });

  it('flycast notes mention RetroArch core', () => {
    const notesText = flycast?.notes?.join(' ') ?? '';
    expect(notesText).toMatch(/flycast_libretro/i);
  });
});

describe('DuckStation backend', () => {
  const duckstation = KNOWN_BACKENDS.find((b) => b.id === 'duckstation');

  it('duckstation backend is defined', () => {
    expect(duckstation).toBeDefined();
    expect(duckstation?.name).toBe('DuckStation');
  });

  it('duckstation covers psx', () => {
    expect(duckstation?.systems).toContain('psx');
  });

  it('duckstation does not support netplay natively', () => {
    expect(duckstation?.supportsNetplay).toBe(false);
  });

  it('duckstation supports save states', () => {
    expect(duckstation?.supportsSaveStates).toBe(true);
  });

  it('duckstation notes mention relay-only netplay', () => {
    const notesText = duckstation?.notes?.join(' ') ?? '';
    expect(notesText).toMatch(/relay/i);
  });
});

describe('PCSX2 backend', () => {
  const pcsx2 = KNOWN_BACKENDS.find((b) => b.id === 'pcsx2');

  it('pcsx2 backend is defined', () => {
    expect(pcsx2).toBeDefined();
    expect(pcsx2?.name).toBe('PCSX2');
  });

  it('pcsx2 covers ps2', () => {
    expect(pcsx2?.systems).toContain('ps2');
  });

  it('pcsx2 supports netplay', () => {
    expect(pcsx2?.supportsNetplay).toBe(true);
  });

  it('pcsx2 supports save states', () => {
    expect(pcsx2?.supportsSaveStates).toBe(true);
  });

  it('pcsx2 notes mention relay netplay', () => {
    const notesText = pcsx2?.notes?.join(' ') ?? '';
    expect(notesText).toMatch(/relay/i);
  });
});

describe('PPSSPP backend', () => {
  const ppsspp = KNOWN_BACKENDS.find((b) => b.id === 'ppsspp');

  it('ppsspp backend is defined', () => {
    expect(ppsspp).toBeDefined();
    expect(ppsspp?.name).toBe('PPSSPP');
  });

  it('ppsspp covers psp', () => {
    expect(ppsspp?.systems).toContain('psp');
  });

  it('ppsspp supports netplay', () => {
    expect(ppsspp?.supportsNetplay).toBe(true);
  });

  it('ppsspp supports save states', () => {
    expect(ppsspp?.supportsSaveStates).toBe(true);
  });

  it('ppsspp notes mention adhoc-server', () => {
    const notesText = ppsspp?.notes?.join(' ') ?? '';
    expect(notesText).toMatch(/adhoc-server/i);
  });
});

describe('RetroArch backend covers new systems', () => {
  const retroarch = KNOWN_BACKENDS.find((b) => b.id === 'retroarch');

  it('RetroArch systems list includes dreamcast', () => {
    expect(retroarch?.systems).toContain('dreamcast');
  });

  it('RetroArch systems list includes psx', () => {
    expect(retroarch?.systems).toContain('psx');
  });

  it('RetroArch systems list includes ps2', () => {
    expect(retroarch?.systems).toContain('ps2');
  });

  it('RetroArch systems list includes psp', () => {
    expect(retroarch?.systems).toContain('psp');
  });
});

// ---------------------------------------------------------------------------
// Dreamcast system adapter
// ---------------------------------------------------------------------------

describe('Dreamcast system adapter', () => {
  const adapter = createSystemAdapter('dreamcast');

  it('uses retroarch as preferred backend', () => {
    expect(adapter.preferredBackendId).toBe('retroarch');
  });

  it('has flycast as fallback backend', () => {
    expect(adapter.fallbackBackendIds).toContain('flycast');
  });

  it('builds launch args with rom path', () => {
    const args = adapter.buildLaunchArgs('/roms/dc/sonicadventure2.gdi', {});
    expect(args).toContain('/roms/dc/sonicadventure2.gdi');
  });

  it('adds --fullscreen when fullscreen is requested (retroarch path)', () => {
    const args = adapter.buildLaunchArgs('/roms/dc/sonicadventure2.gdi', { fullscreen: true });
    expect(args).toContain('--fullscreen');
  });

  it('builds flycast standalone args when backendId is flycast', () => {
    const flycastAdapter = createSystemAdapter('dreamcast', 'flycast');
    const args = flycastAdapter.buildLaunchArgs('/roms/dc/mvc2.gdi', { fullscreen: true });
    expect(args).toContain('/roms/dc/mvc2.gdi');
    expect(args).toContain('--fullscreen');
  });

  it('adds --verbose for flycast debug mode', () => {
    const flycastAdapter = createSystemAdapter('dreamcast', 'flycast');
    const args = flycastAdapter.buildLaunchArgs('/roms/dc/mvc2.gdi', { debug: true });
    expect(args).toContain('--verbose');
  });

  it('saves to dreamcast/ subdirectory', () => {
    const path = adapter.getSavePath('dc-sonic-adventure-2', '/saves');
    expect(path).toBe('/saves/dreamcast/dc-sonic-adventure-2');
  });
});

// ---------------------------------------------------------------------------
// PSX system adapter
// ---------------------------------------------------------------------------

describe('PSX system adapter', () => {
  const adapter = createSystemAdapter('psx');

  it('uses duckstation as preferred backend', () => {
    expect(adapter.preferredBackendId).toBe('duckstation');
  });

  it('has retroarch as fallback backend', () => {
    expect(adapter.fallbackBackendIds).toContain('retroarch');
  });

  it('builds launch args with rom path (duckstation path)', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/tekken3.bin', {});
    expect(args).toContain('/roms/psx/tekken3.bin');
  });

  it('adds --fullscreen when fullscreen is requested', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/tekken3.bin', { fullscreen: true });
    expect(args).toContain('--fullscreen');
  });

  it('builds retroarch args when backendId is retroarch', () => {
    const raAdapter = createSystemAdapter('psx', 'retroarch');
    const args = raAdapter.buildLaunchArgs('/roms/psx/tekken3.bin', {
      coreLibraryPath: '/usr/lib/libretro/pcsx_rearmed_libretro.so',
    });
    expect(args).toContain('-L');
    expect(args).toContain('/usr/lib/libretro/pcsx_rearmed_libretro.so');
  });

  it('adds --verbose for debug mode', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/tekken3.bin', { debug: true });
    expect(args).toContain('--verbose');
  });

  it('saves to psx/ subdirectory', () => {
    const path = adapter.getSavePath('psx-tekken-3', '/saves');
    expect(path).toBe('/saves/psx/psx-tekken-3');
  });
});

// ---------------------------------------------------------------------------
// PS2 system adapter
// ---------------------------------------------------------------------------

describe('PS2 system adapter', () => {
  const adapter = createSystemAdapter('ps2');

  it('uses pcsx2 as preferred backend', () => {
    expect(adapter.preferredBackendId).toBe('pcsx2');
  });

  it('has retroarch as fallback backend', () => {
    expect(adapter.fallbackBackendIds).toContain('retroarch');
  });

  it('builds launch args with rom path (pcsx2 uses -- separator)', () => {
    const args = adapter.buildLaunchArgs('/roms/ps2/gtasa.iso', {});
    expect(args).toContain('/roms/ps2/gtasa.iso');
    expect(args).toContain('--');
    // ROM path must come after the -- separator
    const doubleDashIdx = args.indexOf('--');
    const romIdx = args.indexOf('/roms/ps2/gtasa.iso');
    expect(romIdx).toBeGreaterThan(doubleDashIdx);
  });

  it('adds --fullscreen when fullscreen is requested (before -- separator)', () => {
    const args = adapter.buildLaunchArgs('/roms/ps2/gtasa.iso', { fullscreen: true });
    expect(args).toContain('--fullscreen');
    const fullscreenIdx = args.indexOf('--fullscreen');
    const doubleDashIdx = args.indexOf('--');
    // --fullscreen should come before the -- separator
    expect(fullscreenIdx).toBeLessThan(doubleDashIdx);
  });

  it('builds retroarch args when backendId is retroarch', () => {
    const raAdapter = createSystemAdapter('ps2', 'retroarch');
    const args = raAdapter.buildLaunchArgs('/roms/ps2/gtasa.iso', {
      netplayPort: 9060,
    });
    expect(args).toContain('--host');
    expect(args).toContain('--port');
  });

  it('adds --verbose for debug mode', () => {
    const args = adapter.buildLaunchArgs('/roms/ps2/gtasa.iso', { debug: true });
    expect(args).toContain('--verbose');
  });

  it('saves to ps2/ subdirectory', () => {
    const path = adapter.getSavePath('ps2-gta-san-andreas', '/saves');
    expect(path).toBe('/saves/ps2/ps2-gta-san-andreas');
  });
});

// ---------------------------------------------------------------------------
// PSP system adapter
// ---------------------------------------------------------------------------

describe('PSP system adapter', () => {
  const adapter = createSystemAdapter('psp');

  it('uses ppsspp as preferred backend', () => {
    expect(adapter.preferredBackendId).toBe('ppsspp');
  });

  it('has retroarch as fallback backend', () => {
    expect(adapter.fallbackBackendIds).toContain('retroarch');
  });

  it('builds launch args with rom path', () => {
    const args = adapter.buildLaunchArgs('/roms/psp/mhfu.iso', {});
    expect(args).toContain('/roms/psp/mhfu.iso');
  });

  it('adds --fullscreen when fullscreen is requested', () => {
    const args = adapter.buildLaunchArgs('/roms/psp/mhfu.iso', { fullscreen: true });
    expect(args).toContain('--fullscreen');
  });

  it('adds --adhoc-server for ad-hoc relay netplay', () => {
    const args = adapter.buildLaunchArgs('/roms/psp/mhfu.iso', {
      netplayHost: 'relay.retrooasis.net',
      netplayPort: 9070,
    });
    expect(args).toContain('--adhoc-server');
    expect(args).toContain('relay.retrooasis.net:9070');
  });

  it('builds retroarch args when backendId is retroarch', () => {
    const raAdapter = createSystemAdapter('psp', 'retroarch');
    const args = raAdapter.buildLaunchArgs('/roms/psp/mhfu.iso', {
      netplayPort: 9070,
    });
    expect(args).toContain('--host');
    expect(args).toContain('--port');
  });

  it('adds --verbose for debug mode', () => {
    const args = adapter.buildLaunchArgs('/roms/psp/mhfu.iso', { debug: true });
    expect(args).toContain('--verbose');
  });

  it('saves to psp/ subdirectory', () => {
    const path = adapter.getSavePath('psp-monster-hunter-fu', '/saves');
    expect(path).toBe('/saves/psp/psp-monster-hunter-fu');
  });
});

// ---------------------------------------------------------------------------
// Session templates — Dreamcast
// ---------------------------------------------------------------------------

describe('Dreamcast session templates', () => {
  const store = new SessionTemplateStore();

  it('includes a default Dreamcast template', () => {
    const tpl = store.get('dreamcast-default');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('dreamcast');
    expect(tpl?.playerCount).toBe(2);
    expect(tpl?.emulatorBackendId).toBe('retroarch');
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes Sonic Adventure 2 2P template', () => {
    const tpl = store.get('dreamcast-sonic-adventure-2-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('dc-sonic-adventure-2');
    expect(tpl?.playerCount).toBe(2);
  });

  it('includes Power Stone 2 4P template', () => {
    const tpl = store.get('dreamcast-power-stone-2-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('dc-power-stone-2');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Marvel vs. Capcom 2 template', () => {
    const tpl = store.get('dreamcast-marvel-vs-capcom-2-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('dc-marvel-vs-capcom-2');
  });

  it('includes Soul Calibur template', () => {
    const tpl = store.get('dreamcast-soul-calibur-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('dc-soul-calibur');
  });

  it('includes Virtua Tennis 4P template', () => {
    const tpl = store.get('dreamcast-virtua-tennis-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('dc-virtua-tennis');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Jet Grind Radio template', () => {
    const tpl = store.get('dreamcast-jet-grind-radio-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('dc-jet-grind-radio');
  });

  it('all Dreamcast templates use online-relay netplay', () => {
    const dcTemplates = store.listAll().filter((t) => t.system === 'dreamcast');
    expect(dcTemplates.length).toBeGreaterThan(0);
    for (const tpl of dcTemplates) {
      expect(tpl.netplayMode).toBe('online-relay');
    }
  });

  it('all Dreamcast templates have latencyTarget set', () => {
    const dcTemplates = store.listAll().filter((t) => t.system === 'dreamcast');
    for (const tpl of dcTemplates) {
      expect(tpl.latencyTarget).toBeGreaterThan(0);
    }
  });

  it('there are at least 10 Dreamcast templates', () => {
    const dcTemplates = store.listAll().filter((t) => t.system === 'dreamcast');
    expect(dcTemplates.length).toBeGreaterThanOrEqual(10);
  });
});

// ---------------------------------------------------------------------------
// Session templates — PSX
// ---------------------------------------------------------------------------

describe('PSX session templates', () => {
  const store = new SessionTemplateStore();

  it('includes a default PSX template', () => {
    const tpl = store.get('psx-default');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('psx');
    expect(tpl?.playerCount).toBe(2);
    expect(tpl?.emulatorBackendId).toBe('duckstation');
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes Tekken 3 template', () => {
    const tpl = store.get('psx-tekken-3-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('psx-tekken-3');
    expect(tpl?.playerCount).toBe(2);
  });

  it('includes Crash Bash 4P template', () => {
    const tpl = store.get('psx-crash-bash-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('psx-crash-bash');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Metal Gear Solid 1P template', () => {
    const tpl = store.get('psx-metal-gear-solid-1p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('psx-metal-gear-solid');
    expect(tpl?.playerCount).toBe(1);
  });

  it('includes Castlevania SOTN 1P template', () => {
    const tpl = store.get('psx-castlevania-sotn-1p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('psx-castlevania-sotn');
  });

  it('all PSX templates use duckstation backend', () => {
    const psxTemplates = store.listAll().filter((t) => t.system === 'psx');
    expect(psxTemplates.length).toBeGreaterThan(0);
    for (const tpl of psxTemplates) {
      expect(tpl.emulatorBackendId).toBe('duckstation');
    }
  });

  it('original 10 PSX templates all have latencyTarget', () => {
    const psxTemplates = store.listAll().filter((t) => t.system === 'psx');
    for (const tpl of psxTemplates) {
      expect(tpl.latencyTarget).toBeGreaterThan(0);
    }
  });

  it('there are at least 16 PSX templates (1 default + 15 game-specific)', () => {
    const psxTemplates = store.listAll().filter((t) => t.system === 'psx');
    expect(psxTemplates.length).toBeGreaterThanOrEqual(16);
  });
});

// ---------------------------------------------------------------------------
// Session templates — PS2
// ---------------------------------------------------------------------------

describe('PS2 session templates', () => {
  const store = new SessionTemplateStore();

  it('includes a default PS2 template', () => {
    const tpl = store.get('ps2-default');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('ps2');
    expect(tpl?.playerCount).toBe(2);
    expect(tpl?.emulatorBackendId).toBe('pcsx2');
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes Guitar Hero II template', () => {
    const tpl = store.get('ps2-guitar-hero-2-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('ps2-guitar-hero-2');
    expect(tpl?.playerCount).toBe(2);
  });

  it('includes Kingdom Hearts 1P template', () => {
    const tpl = store.get('ps2-kingdom-hearts-1p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('ps2-kingdom-hearts');
    expect(tpl?.playerCount).toBe(1);
  });

  it('includes Shadow of the Colossus 1P template', () => {
    const tpl = store.get('ps2-shadow-of-the-colossus-1p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('ps2-shadow-of-the-colossus');
  });

  it('all PS2 templates use pcsx2 backend', () => {
    const ps2Templates = store.listAll().filter((t) => t.system === 'ps2');
    expect(ps2Templates.length).toBeGreaterThan(0);
    for (const tpl of ps2Templates) {
      expect(tpl.emulatorBackendId).toBe('pcsx2');
    }
  });

  it('all PS2 templates have latencyTarget of 120ms', () => {
    const ps2Templates = store.listAll().filter((t) => t.system === 'ps2');
    for (const tpl of ps2Templates) {
      expect(tpl.latencyTarget).toBe(120);
    }
  });

  it('there are exactly 10 PS2 templates (1 default + 9 game-specific)', () => {
    const ps2Templates = store.listAll().filter((t) => t.system === 'ps2');
    expect(ps2Templates.length).toBe(10);
  });
});

// ---------------------------------------------------------------------------
// Session templates — PSP
// ---------------------------------------------------------------------------

describe('PSP session templates', () => {
  const store = new SessionTemplateStore();

  it('includes a default PSP template', () => {
    const tpl = store.get('psp-default');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('psp');
    expect(tpl?.playerCount).toBe(2);
    expect(tpl?.emulatorBackendId).toBe('ppsspp');
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes Monster Hunter Freedom Unite 4P template', () => {
    const tpl = store.get('psp-monster-hunter-fu-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('psp-monster-hunter-fu');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes WipEout Pulse 8P template', () => {
    const tpl = store.get('psp-wipeout-pulse-8p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('psp-wipeout-pulse');
    expect(tpl?.playerCount).toBe(8);
  });

  it('includes God of War: Chains of Olympus 1P template', () => {
    const tpl = store.get('psp-god-of-war-chains-1p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('psp-god-of-war-chains');
    expect(tpl?.playerCount).toBe(1);
  });

  it('includes Crisis Core: FF7 1P template', () => {
    const tpl = store.get('psp-crisis-core-ff7-1p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('psp-crisis-core-ff7');
    expect(tpl?.playerCount).toBe(1);
  });

  it('all PSP templates use ppsspp backend', () => {
    const pspTemplates = store.listAll().filter((t) => t.system === 'psp');
    expect(pspTemplates.length).toBeGreaterThan(0);
    for (const tpl of pspTemplates) {
      expect(tpl.emulatorBackendId).toBe('ppsspp');
    }
  });

  it('all PSP templates have latencyTarget of 100ms', () => {
    const pspTemplates = store.listAll().filter((t) => t.system === 'psp');
    for (const tpl of pspTemplates) {
      expect(tpl.latencyTarget).toBe(100);
    }
  });

  it('there are exactly 10 PSP templates (1 default + 9 game-specific)', () => {
    const pspTemplates = store.listAll().filter((t) => t.system === 'psp');
    expect(pspTemplates.length).toBe(10);
  });
});

// ---------------------------------------------------------------------------
// Mock game catalog — Dreamcast
// ---------------------------------------------------------------------------

describe('Dreamcast mock game catalog', () => {
  const dcGames = MOCK_GAMES.filter((g) => g.system === 'Dreamcast');

  it('includes at least 9 Dreamcast games in the catalog', () => {
    expect(dcGames.length).toBeGreaterThanOrEqual(9);
  });

  it('includes Sonic Adventure 2', () => {
    const game = dcGames.find((g) => g.id === 'dc-sonic-adventure-2');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(2);
  });

  it('includes Power Stone 2', () => {
    const game = dcGames.find((g) => g.id === 'dc-power-stone-2');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(4);
  });

  it('includes Marvel vs. Capcom 2', () => {
    const game = dcGames.find((g) => g.id === 'dc-marvel-vs-capcom-2');
    expect(game).toBeDefined();
  });

  it('includes Soul Calibur', () => {
    const game = dcGames.find((g) => g.id === 'dc-soul-calibur');
    expect(game).toBeDefined();
  });

  it('includes Virtua Tennis', () => {
    const game = dcGames.find((g) => g.id === 'dc-virtua-tennis');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(4);
  });

  it('includes Jet Grind Radio', () => {
    const game = dcGames.find((g) => g.id === 'dc-jet-grind-radio');
    expect(game).toBeDefined();
  });

  it('all Dreamcast games have correct system label', () => {
    for (const game of dcGames) {
      expect(game.system).toBe('Dreamcast');
    }
  });

  it('all Dreamcast games have systemColor #FF6600', () => {
    for (const game of dcGames) {
      expect(game.systemColor).toBe('#FF6600');
    }
  });
});

// ---------------------------------------------------------------------------
// Mock game catalog — PSX
// ---------------------------------------------------------------------------

describe('PSX mock game catalog', () => {
  const psxGames = MOCK_GAMES.filter((g) => g.system === 'PSX');

  it('includes at least 9 PSX games in the catalog', () => {
    expect(psxGames.length).toBeGreaterThanOrEqual(9);
  });

  it('includes Tekken 3', () => {
    const game = psxGames.find((g) => g.id === 'psx-tekken-3');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(2);
  });

  it('includes Crash Bash', () => {
    const game = psxGames.find((g) => g.id === 'psx-crash-bash');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(4);
  });

  it('includes Castlevania: Symphony of the Night', () => {
    const game = psxGames.find((g) => g.id === 'psx-castlevania-sotn');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(1);
  });

  it('includes Metal Gear Solid', () => {
    const game = psxGames.find((g) => g.id === 'psx-metal-gear-solid');
    expect(game).toBeDefined();
  });

  it('all PSX games have correct system label', () => {
    for (const game of psxGames) {
      expect(game.system).toBe('PSX');
    }
  });

  it('all PSX games have systemColor #808080', () => {
    for (const game of psxGames) {
      expect(game.systemColor).toBe('#808080');
    }
  });
});

// ---------------------------------------------------------------------------
// Mock game catalog — PS2
// ---------------------------------------------------------------------------

describe('PS2 mock game catalog', () => {
  const ps2Games = MOCK_GAMES.filter((g) => g.system === 'PS2');

  it('includes at least 9 PS2 games in the catalog', () => {
    expect(ps2Games.length).toBeGreaterThanOrEqual(9);
  });

  it('includes Guitar Hero II', () => {
    const game = ps2Games.find((g) => g.id === 'ps2-guitar-hero-2');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(2);
  });

  it('includes Kingdom Hearts', () => {
    const game = ps2Games.find((g) => g.id === 'ps2-kingdom-hearts');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(1);
  });

  it('includes Shadow of the Colossus', () => {
    const game = ps2Games.find((g) => g.id === 'ps2-shadow-of-the-colossus');
    expect(game).toBeDefined();
  });

  it('includes GTA: San Andreas', () => {
    const game = ps2Games.find((g) => g.id === 'ps2-gta-san-andreas');
    expect(game).toBeDefined();
  });

  it('all PS2 games have correct system label', () => {
    for (const game of ps2Games) {
      expect(game.system).toBe('PS2');
    }
  });

  it('all PS2 games have systemColor #00439C', () => {
    for (const game of ps2Games) {
      expect(game.systemColor).toBe('#00439C');
    }
  });
});

// ---------------------------------------------------------------------------
// Mock game catalog — PSP
// ---------------------------------------------------------------------------

describe('PSP mock game catalog', () => {
  const pspGames = MOCK_GAMES.filter((g) => g.system === 'PSP');

  it('includes at least 9 PSP games in the catalog', () => {
    expect(pspGames.length).toBeGreaterThanOrEqual(9);
  });

  it('includes Monster Hunter Freedom Unite', () => {
    const game = pspGames.find((g) => g.id === 'psp-monster-hunter-fu');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(4);
  });

  it('includes WipEout Pulse', () => {
    const game = pspGames.find((g) => g.id === 'psp-wipeout-pulse');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(8);
  });

  it('includes Crisis Core: FF7', () => {
    const game = pspGames.find((g) => g.id === 'psp-crisis-core-ff7');
    expect(game).toBeDefined();
    expect(game?.maxPlayers).toBe(1);
  });

  it('includes God of War: Chains of Olympus', () => {
    const game = pspGames.find((g) => g.id === 'psp-god-of-war-chains');
    expect(game).toBeDefined();
  });

  it('all PSP games have correct system label', () => {
    for (const game of pspGames) {
      expect(game.system).toBe('PSP');
    }
  });

  it('all PSP games have systemColor #1C1C1C', () => {
    for (const game of pspGames) {
      expect(game.systemColor).toBe('#1C1C1C');
    }
  });
});

// ---------------------------------------------------------------------------
// Regression: no duplicate mock-game IDs introduced by Phase 22
// ---------------------------------------------------------------------------

describe('Mock game catalog — no new duplicates (Phase 22)', () => {
  it('has no duplicate IDs across the entire catalog', () => {
    const ids = MOCK_GAMES.map((g) => g.id);
    const unique = new Set(ids);
    expect(unique.size).toBe(ids.length);
  });
});
