/**
 * Phase 42 — Dreamcast Overhaul
 *
 * Tests for:
 *  - Expanded DC mock game catalog (17 games total, incl. 8 new titles)
 *  - Fixed player counts (Crazy Taxi 2P, Jet Grind Radio 2P)
 *  - New DC session templates (19 total)
 *  - Adjusted fighting-game latency targets (60 ms for MvC2, SC, DOA2)
 *  - Dreamcast netplay whitelist entries (16 DC entries)
 *  - DC adapter new flags: dcRegion, vmuEnabled, dcCableType
 *  - Flycast backend expanded notes
 *  - Flycast save-state extension fix (.state not .fcs)
 *  - SYSTEM_BACKEND_MAP dreamcast → retroarch
 *  - DC BIOS expanded entries
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';
import { NETPLAY_WHITELIST, approvedGamesForSystem } from '../../../../apps/desktop/src/lib/netplay-whitelist';
import { getBackendSaveExtensions } from '../../../../packages/save-system/src/discovery';
import { BIOS_REQUIREMENTS } from '../../../../apps/desktop/src/lib/bios-validator';
import { SYSTEM_BACKEND_MAP } from '../../../../apps/desktop/src/lib/emulator-settings';

// ─────────────────────────────────────────────────────────────────────────────
// Mock game catalog
// ─────────────────────────────────────────────────────────────────────────────

describe('Dreamcast mock game catalog — overhaul', () => {
  const dcGames = MOCK_GAMES.filter((g) => g.system === 'Dreamcast');

  it('has at least 16 Dreamcast games', () => {
    expect(dcGames.length).toBeGreaterThanOrEqual(16);
  });

  it('includes Dead or Alive 2', () => {
    expect(dcGames.find((g) => g.id === 'dc-dead-or-alive-2')).toBeDefined();
  });

  it('includes Capcom vs. SNK', () => {
    expect(dcGames.find((g) => g.id === 'dc-capcom-vs-snk')).toBeDefined();
  });

  it('includes Ikaruga', () => {
    expect(dcGames.find((g) => g.id === 'dc-ikaruga')).toBeDefined();
  });

  it('includes Cannon Spike', () => {
    expect(dcGames.find((g) => g.id === 'dc-cannon-spike')).toBeDefined();
  });

  it('includes Tech Romancer', () => {
    expect(dcGames.find((g) => g.id === 'dc-tech-romancer')).toBeDefined();
  });

  it('includes Toy Racer', () => {
    expect(dcGames.find((g) => g.id === 'dc-toy-racer')).toBeDefined();
  });

  it('includes Gauntlet Legends', () => {
    expect(dcGames.find((g) => g.id === 'dc-gauntlet-legends')).toBeDefined();
  });

  it('Crazy Taxi is correctly listed as 2-player', () => {
    const game = dcGames.find((g) => g.id === 'dc-crazy-taxi');
    expect(game?.maxPlayers).toBe(2);
  });

  it('Jet Grind Radio is correctly listed as 2-player', () => {
    const game = dcGames.find((g) => g.id === 'dc-jet-grind-radio');
    expect(game?.maxPlayers).toBe(2);
  });

  it('Power Stone 2 is listed as 4-player', () => {
    const game = dcGames.find((g) => g.id === 'dc-power-stone-2');
    expect(game?.maxPlayers).toBe(4);
  });

  it('Virtua Tennis is listed as 4-player', () => {
    const game = dcGames.find((g) => g.id === 'dc-virtua-tennis');
    expect(game?.maxPlayers).toBe(4);
  });

  it('Toy Racer is listed as 4-player', () => {
    const game = dcGames.find((g) => g.id === 'dc-toy-racer');
    expect(game?.maxPlayers).toBe(4);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Session templates
// ─────────────────────────────────────────────────────────────────────────────

describe('Dreamcast session templates — overhaul', () => {
  const store = new SessionTemplateStore();
  const dcTemplates = store.listAll().filter((t) => t.system === 'dreamcast');

  it('has at least 17 Dreamcast templates', () => {
    expect(dcTemplates.length).toBeGreaterThanOrEqual(17);
  });

  it('has a template for Dead or Alive 2', () => {
    expect(dcTemplates.find((t) => t.gameId === 'dc-dead-or-alive-2')).toBeDefined();
  });

  it('has a template for Capcom vs. SNK', () => {
    expect(dcTemplates.find((t) => t.gameId === 'dc-capcom-vs-snk')).toBeDefined();
  });

  it('has a template for Ikaruga', () => {
    expect(dcTemplates.find((t) => t.gameId === 'dc-ikaruga')).toBeDefined();
  });

  it('has a template for Cannon Spike', () => {
    expect(dcTemplates.find((t) => t.gameId === 'dc-cannon-spike')).toBeDefined();
  });

  it('has a template for Tech Romancer', () => {
    expect(dcTemplates.find((t) => t.gameId === 'dc-tech-romancer')).toBeDefined();
  });

  it('has a template for Toy Racer', () => {
    expect(dcTemplates.find((t) => t.gameId === 'dc-toy-racer')).toBeDefined();
  });

  it('has a template for Gauntlet Legends', () => {
    expect(dcTemplates.find((t) => t.gameId === 'dc-gauntlet-legends')).toBeDefined();
  });

  it('MvC2 template has latencyTarget ≤ 60 ms (frame-critical fighter)', () => {
    const t = dcTemplates.find((t) => t.gameId === 'dc-marvel-vs-capcom-2');
    expect(t?.latencyTarget).toBeLessThanOrEqual(60);
  });

  it('Soul Calibur template has latencyTarget ≤ 60 ms (weapons fighter)', () => {
    const t = dcTemplates.find((t) => t.gameId === 'dc-soul-calibur');
    expect(t?.latencyTarget).toBeLessThanOrEqual(60);
  });

  it('Dead or Alive 2 template has latencyTarget ≤ 60 ms (hold-timing fighter)', () => {
    const t = dcTemplates.find((t) => t.gameId === 'dc-dead-or-alive-2');
    expect(t?.latencyTarget).toBeLessThanOrEqual(60);
  });

  it('Virtua Tennis template has latencyTarget ≤ 100 ms (sports)', () => {
    const t = dcTemplates.find((t) => t.gameId === 'dc-virtua-tennis');
    expect(t?.latencyTarget).toBeLessThanOrEqual(100);
  });

  it('Ikaruga template has latencyTarget ≤ 100 ms (co-op shooter)', () => {
    const t = dcTemplates.find((t) => t.gameId === 'dc-ikaruga');
    expect(t?.latencyTarget).toBeLessThanOrEqual(100);
  });

  it('Toy Racer template uses 4 players', () => {
    const t = dcTemplates.find((t) => t.gameId === 'dc-toy-racer');
    expect(t?.playerCount).toBe(4);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Netplay whitelist
// ─────────────────────────────────────────────────────────────────────────────

describe('Dreamcast netplay whitelist — overhaul', () => {
  const dcEntries = NETPLAY_WHITELIST.filter((e) => e.system === 'dreamcast');

  it('has at least 16 Dreamcast whitelist entries', () => {
    expect(dcEntries.length).toBeGreaterThanOrEqual(16);
  });

  it('MvC2 is approved for netplay', () => {
    const e = dcEntries.find((e) => e.gameId === 'dc-marvel-vs-capcom-2');
    expect(e?.rating).toBe('approved');
  });

  it('Soul Calibur is approved for netplay', () => {
    const e = dcEntries.find((e) => e.gameId === 'dc-soul-calibur');
    expect(e?.rating).toBe('approved');
  });

  it('Dead or Alive 2 is approved for netplay', () => {
    const e = dcEntries.find((e) => e.gameId === 'dc-dead-or-alive-2');
    expect(e?.rating).toBe('approved');
  });

  it('Ikaruga is approved for netplay', () => {
    const e = dcEntries.find((e) => e.gameId === 'dc-ikaruga');
    expect(e?.rating).toBe('approved');
  });

  it('Toy Racer is approved for netplay', () => {
    const e = dcEntries.find((e) => e.gameId === 'dc-toy-racer');
    expect(e?.rating).toBe('approved');
  });

  it('approvedGamesForSystem returns DC entries', () => {
    const approved = approvedGamesForSystem('dreamcast');
    expect(approved.length).toBeGreaterThanOrEqual(10);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Adapter — new DC-specific flags
// ─────────────────────────────────────────────────────────────────────────────

describe('Dreamcast adapter — new DC-specific flags', () => {
  it('dcRegion flag passed to standalone Flycast args', () => {
    const adapter = createSystemAdapter('dreamcast', 'flycast');
    const args = adapter.buildLaunchArgs('/roms/dc/game.gdi', { dcRegion: 'ntsc-u' });
    expect(args).toContain('--region=ntsc-u');
  });

  it('dcCableType=vga flag passed to standalone Flycast', () => {
    const adapter = createSystemAdapter('dreamcast', 'flycast');
    const args = adapter.buildLaunchArgs('/roms/dc/game.gdi', { dcCableType: 'vga' });
    expect(args).toContain('--cable-type=vga');
  });

  it('vmuEnabled=false disables VMU via --vmu=0', () => {
    const adapter = createSystemAdapter('dreamcast', 'flycast');
    const args = adapter.buildLaunchArgs('/roms/dc/game.gdi', { vmuEnabled: false });
    expect(args).toContain('--vmu=0');
  });

  it('vmuEnabled=true (default) does not add --vmu=0', () => {
    const adapter = createSystemAdapter('dreamcast', 'flycast');
    const args = adapter.buildLaunchArgs('/roms/dc/game.gdi', { vmuEnabled: true });
    expect(args).not.toContain('--vmu=0');
  });

  it('performancePreset=accurate adds --no-dynarec for Flycast', () => {
    const adapter = createSystemAdapter('dreamcast', 'flycast');
    const args = adapter.buildLaunchArgs('/roms/dc/game.gdi', { performancePreset: 'accurate' });
    expect(args).toContain('--no-dynarec');
  });

  it('performancePreset=fast does not add --no-dynarec', () => {
    const adapter = createSystemAdapter('dreamcast', 'flycast');
    const args = adapter.buildLaunchArgs('/roms/dc/game.gdi', { performancePreset: 'fast' });
    expect(args).not.toContain('--no-dynarec');
  });

  it('retroarch backend uses RetroArch args (default)', () => {
    const adapter = createSystemAdapter('dreamcast');
    const args = adapter.buildLaunchArgs('/roms/dc/game.gdi', {
      coreLibraryPath: '/usr/lib/libretro/flycast_libretro.so',
      netplayPort: 55435,
    });
    expect(args).toContain('-L');
    expect(args).toContain('--host');
  });

  it('flycast fullscreen flag', () => {
    const adapter = createSystemAdapter('dreamcast', 'flycast');
    const args = adapter.buildLaunchArgs('/roms/dc/game.gdi', { fullscreen: true });
    expect(args).toContain('--fullscreen');
  });

  it('flycast debug flag', () => {
    const adapter = createSystemAdapter('dreamcast', 'flycast');
    const args = adapter.buildLaunchArgs('/roms/dc/game.gdi', { debug: true });
    expect(args).toContain('--verbose');
  });

  it('compatibilityFlags passed through for flycast', () => {
    const adapter = createSystemAdapter('dreamcast', 'flycast');
    const args = adapter.buildLaunchArgs('/roms/dc/game.gdi', { compatibilityFlags: ['--custom-flag'] });
    expect(args).toContain('--custom-flag');
  });

  it('getSavePath includes dreamcast subdirectory', () => {
    const adapter = createSystemAdapter('dreamcast');
    const p = adapter.getSavePath('dc-mvc2', '/saves');
    expect(p).toContain('dreamcast');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Flycast backend definition
// ─────────────────────────────────────────────────────────────────────────────

describe('Flycast backend definition — expanded', () => {
  const flycast = KNOWN_BACKENDS.find((b) => b.id === 'flycast');

  it('flycast backend is registered', () => {
    expect(flycast).toBeDefined();
  });

  it('flycast supports the dreamcast system', () => {
    expect(flycast?.systems).toContain('dreamcast');
  });

  it('flycast has at least 15 notes', () => {
    expect(flycast?.notes.length).toBeGreaterThanOrEqual(15);
  });

  it('flycast notes mention BIOS placement', () => {
    const combined = flycast?.notes.join(' ') ?? '';
    expect(combined).toMatch(/dc_boot\.bin/);
  });

  it('flycast notes mention .gdi ROM format', () => {
    const combined = flycast?.notes.join(' ') ?? '';
    expect(combined).toMatch(/\.gdi/);
  });

  it('flycast notes mention .chd ROM format', () => {
    const combined = flycast?.notes.join(' ') ?? '';
    expect(combined).toMatch(/\.chd/);
  });

  it('flycast notes mention VMU', () => {
    const combined = flycast?.notes.join(' ') ?? '';
    expect(combined.toLowerCase()).toMatch(/vmu/);
  });

  it('flycast capabilities: localLaunch and netplay true', () => {
    expect(flycast?.capabilities.localLaunch).toBe(true);
    expect(flycast?.capabilities.netplay).toBe(true);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Save-state extension fix
// ─────────────────────────────────────────────────────────────────────────────

describe('Flycast save extensions — state fix', () => {
  const exts = getBackendSaveExtensions('flycast');

  it('flycast SRAM extensions include .bin and .vms', () => {
    expect(exts.sramExtensions).toContain('.bin');
    expect(exts.sramExtensions).toContain('.vms');
  });

  it('flycast state extensions include .state (not .fcs)', () => {
    expect(exts.stateExtensions).toContain('.state');
    expect(exts.stateExtensions).not.toContain('.fcs');
  });

  it('flycast state extensions include slot variants .state1 through .state4', () => {
    expect(exts.stateExtensions).toContain('.state1');
    expect(exts.stateExtensions).toContain('.state4');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// BIOS validator — expanded Dreamcast entries
// ─────────────────────────────────────────────────────────────────────────────

describe('Dreamcast BIOS validator — expanded', () => {
  const dcBios = BIOS_REQUIREMENTS['dreamcast'];

  it('dreamcast BIOS requirement is registered', () => {
    expect(dcBios).toBeDefined();
  });

  it('dc_boot.bin entry is marked required', () => {
    const entry = dcBios.entries.find((e) => e.filename === 'dc_boot.bin');
    expect(entry?.required).toBe(true);
  });

  it('dc_boot.bin has at least 3 known MD5 variants', () => {
    const entry = dcBios.entries.find((e) => e.filename === 'dc_boot.bin');
    expect(entry?.knownMd5.length).toBeGreaterThanOrEqual(3);
  });

  it('dc_flash.bin entry is optional', () => {
    const entry = dcBios.entries.find((e) => e.filename === 'dc_flash.bin');
    expect(entry?.required).toBe(false);
  });

  it('dc_flash.bin has at least 2 known MD5 variants', () => {
    const entry = dcBios.entries.find((e) => e.filename === 'dc_flash.bin');
    expect(entry?.knownMd5.length).toBeGreaterThanOrEqual(2);
  });

  it('note references dc_boot.bin', () => {
    expect(dcBios.note).toMatch(/dc_boot\.bin/);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// SYSTEM_BACKEND_MAP — dreamcast uses retroarch
// ─────────────────────────────────────────────────────────────────────────────

describe('SYSTEM_BACKEND_MAP — dreamcast aligned to retroarch', () => {
  it('dreamcast maps to retroarch (consistent with adapter preferred backend)', () => {
    expect(SYSTEM_BACKEND_MAP['dreamcast']).toBe('retroarch');
  });
});
