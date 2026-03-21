/**
 * Phase 8 — Core System Expansion (NES / SNES / GB / GBC / GBA / N64 / NDS)
 *
 * Tests for:
 *  - System types in SYSTEM_INFO
 *  - Emulator backends for each system
 *  - System adapter launch args (including performancePreset for N64)
 *  - Session templates counts
 *  - Golden-list mock game catalog counts
 *  - No duplicate IDs regression
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import { SYSTEM_INFO } from '../../../../packages/shared-types/src/systems';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';

// ---------------------------------------------------------------------------
// NES system type
// ---------------------------------------------------------------------------

describe('NES system type', () => {
  it('includes nes in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['nes']).toBeDefined();
    expect(SYSTEM_INFO['nes'].shortName).toBe('NES');
  });

  it('NES supports up to 2 local players', () => {
    expect(SYSTEM_INFO['nes'].maxLocalPlayers).toBeGreaterThanOrEqual(2);
  });

  it('NES does not support link cable', () => {
    expect(SYSTEM_INFO['nes'].supportsLink).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// SNES system type
// ---------------------------------------------------------------------------

describe('SNES system type', () => {
  it('includes snes in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['snes']).toBeDefined();
    expect(SYSTEM_INFO['snes'].shortName).toBe('SNES');
  });

  it('SNES supports up to 4 local players', () => {
    expect(SYSTEM_INFO['snes'].maxLocalPlayers).toBe(4);
  });
});

// ---------------------------------------------------------------------------
// GB / GBC system types
// ---------------------------------------------------------------------------

describe('GB / GBC system types', () => {
  it('includes gb and gbc in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['gb']).toBeDefined();
    expect(SYSTEM_INFO['gbc']).toBeDefined();
  });

  it('GB supports link cable', () => {
    expect(SYSTEM_INFO['gb'].supportsLink).toBe(true);
  });

  it('GBC supports link cable', () => {
    expect(SYSTEM_INFO['gbc'].supportsLink).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// GBA system type
// ---------------------------------------------------------------------------

describe('GBA system type', () => {
  it('includes gba in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['gba']).toBeDefined();
    expect(SYSTEM_INFO['gba'].shortName).toBe('GBA');
  });

  it('GBA supports link cable', () => {
    expect(SYSTEM_INFO['gba'].supportsLink).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// N64 system type
// ---------------------------------------------------------------------------

describe('N64 system type', () => {
  it('includes n64 in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['n64']).toBeDefined();
    expect(SYSTEM_INFO['n64'].shortName).toBe('N64');
  });

  it('N64 supports up to 4 local players', () => {
    expect(SYSTEM_INFO['n64'].maxLocalPlayers).toBe(4);
  });

  it('N64 does not support link cable', () => {
    expect(SYSTEM_INFO['n64'].supportsLink).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// NDS system type
// ---------------------------------------------------------------------------

describe('NDS system type', () => {
  it('includes nds in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['nds']).toBeDefined();
    expect(SYSTEM_INFO['nds'].shortName).toBe('NDS');
  });
});

// ---------------------------------------------------------------------------
// Backends coverage
// ---------------------------------------------------------------------------

describe('Backend coverage — Phase 8 systems', () => {
  it('FCEUX backend covers NES', () => {
    const fceux = KNOWN_BACKENDS.find((b) => b.id === 'fceux');
    expect(fceux).toBeDefined();
    expect(fceux!.systems).toContain('nes');
  });

  it('Snes9x backend covers SNES', () => {
    const snes9x = KNOWN_BACKENDS.find((b) => b.id === 'snes9x');
    expect(snes9x).toBeDefined();
    expect(snes9x!.systems).toContain('snes');
  });

  it('mGBA backend covers GB, GBC, and GBA', () => {
    const mgba = KNOWN_BACKENDS.find((b) => b.id === 'mgba');
    expect(mgba).toBeDefined();
    expect(mgba!.systems).toContain('gb');
    expect(mgba!.systems).toContain('gbc');
    expect(mgba!.systems).toContain('gba');
  });

  it('mGBA supports link cable', () => {
    const mgba = KNOWN_BACKENDS.find((b) => b.id === 'mgba');
    expect(mgba!.capabilities.linkCable).toBe(true);
  });

  it('Mupen64Plus backend covers N64', () => {
    const mupen = KNOWN_BACKENDS.find((b) => b.id === 'mupen64plus');
    expect(mupen).toBeDefined();
    expect(mupen!.systems).toContain('n64');
  });

  it('melonDS backend covers NDS', () => {
    const melonds = KNOWN_BACKENDS.find((b) => b.id === 'melonds');
    expect(melonds).toBeDefined();
    expect(melonds!.systems).toContain('nds');
  });
});

// ---------------------------------------------------------------------------
// NES adapter
// ---------------------------------------------------------------------------

describe('NES system adapter', () => {
  const adapter = createSystemAdapter('nes');

  it('preferred backend is fceux', () => {
    expect(adapter.preferredBackendId).toBe('fceux');
  });

  it('fallback backends include nestopia and retroarch', () => {
    expect(adapter.fallbackBackendIds).toContain('nestopia');
    expect(adapter.fallbackBackendIds).toContain('retroarch');
  });

  it('buildLaunchArgs includes ROM path', () => {
    const args = adapter.buildLaunchArgs('/roms/nes/contra.nes', {});
    expect(args).toContain('/roms/nes/contra.nes');
  });

  it('buildLaunchArgs includes --net for netplay', () => {
    const args = adapter.buildLaunchArgs('/roms/nes/contra.nes', { netplayHost: '127.0.0.1', netplayPort: 9001 });
    expect(args.join(' ')).toContain('127.0.0.1:9001');
  });

  it('save path uses nes subdirectory', () => {
    expect(adapter.getSavePath('nes-contra', '/saves')).toBe('/saves/nes/nes-contra');
  });

  it('compatibilityFlags are appended to args', () => {
    const args = adapter.buildLaunchArgs('/roms/nes/contra.nes', { compatibilityFlags: ['--no-audio'] });
    expect(args).toContain('--no-audio');
  });
});

// ---------------------------------------------------------------------------
// SNES adapter
// ---------------------------------------------------------------------------

describe('SNES system adapter', () => {
  const adapter = createSystemAdapter('snes');

  it('preferred backend is snes9x', () => {
    expect(adapter.preferredBackendId).toBe('snes9x');
  });

  it('buildLaunchArgs includes ROM path', () => {
    const args = adapter.buildLaunchArgs('/roms/snes/sf2.sfc', {});
    expect(args).toContain('/roms/snes/sf2.sfc');
  });

  it('buildLaunchArgs includes -netplay for relay netplay', () => {
    const args = adapter.buildLaunchArgs('/roms/snes/sf2.sfc', { netplayHost: '127.0.0.1', netplayPort: 9002 });
    expect(args).toContain('-netplay');
    expect(args).toContain('127.0.0.1');
  });

  it('save path uses snes subdirectory', () => {
    expect(adapter.getSavePath('snes-super-bomberman', '/saves')).toBe('/saves/snes/snes-super-bomberman');
  });

  it('compatibilityFlags are appended to args', () => {
    const args = adapter.buildLaunchArgs('/roms/snes/sf2.sfc', { compatibilityFlags: ['--exact-rate'] });
    expect(args).toContain('--exact-rate');
  });
});

// ---------------------------------------------------------------------------
// GB / GBC / GBA adapter (mGBA)
// ---------------------------------------------------------------------------

describe('GB/GBC/GBA system adapter (mGBA)', () => {
  const gbAdapter = createSystemAdapter('gb');
  const gbcAdapter = createSystemAdapter('gbc');
  const gbaAdapter = createSystemAdapter('gba');

  it('GB preferred backend is mgba', () => {
    expect(gbAdapter.preferredBackendId).toBe('mgba');
  });

  it('GBA preferred backend is mgba', () => {
    expect(gbaAdapter.preferredBackendId).toBe('mgba');
  });

  it('GB buildLaunchArgs includes ROM path', () => {
    const args = gbAdapter.buildLaunchArgs('/roms/gb/tetris.gb', {});
    expect(args.join(' ')).toContain('tetris.gb');
  });

  it('GBA buildLaunchArgs includes --link-host for link cable netplay', () => {
    const args = gbaAdapter.buildLaunchArgs('/roms/gba/pokemon-firered.gba', { netplayHost: '127.0.0.1', netplayPort: 9003 });
    expect(args).toContain('--link-host');
    expect(args.join(' ')).toContain('127.0.0.1:9003');
  });

  it('GB save path uses gb subdirectory', () => {
    expect(gbAdapter.getSavePath('gb-tetris', '/saves')).toBe('/saves/gb/gb-tetris');
  });

  it('GBC save path uses gbc subdirectory', () => {
    expect(gbcAdapter.getSavePath('gbc-pokemon-crystal', '/saves')).toBe('/saves/gbc/gbc-pokemon-crystal');
  });

  it('GBA fallback includes vbam', () => {
    expect(gbaAdapter.fallbackBackendIds).toContain('vbam');
  });

  it('GB fallback includes sameboy', () => {
    expect(gbAdapter.fallbackBackendIds).toContain('sameboy');
  });

  it('compatibilityFlags are appended to args', () => {
    const args = gbaAdapter.buildLaunchArgs('/roms/gba/emerald.gba', { compatibilityFlags: ['--no-video'] });
    expect(args).toContain('--no-video');
  });
});

// ---------------------------------------------------------------------------
// N64 adapter + performance preset
// ---------------------------------------------------------------------------

describe('N64 system adapter (Mupen64Plus)', () => {
  const adapter = createSystemAdapter('n64');

  it('preferred backend is mupen64plus', () => {
    expect(adapter.preferredBackendId).toBe('mupen64plus');
  });

  it('fallback backends include project64 and retroarch', () => {
    expect(adapter.fallbackBackendIds).toContain('project64');
    expect(adapter.fallbackBackendIds).toContain('retroarch');
  });

  it('buildLaunchArgs includes --rom flag', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/mk64.z64', {});
    expect(args).toContain('--rom');
    expect(args).toContain('/roms/n64/mk64.z64');
  });

  it('buildLaunchArgs includes --netplay-host and --netplay-port for relay', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/mk64.z64', { netplayHost: '127.0.0.1', netplayPort: 9004 });
    expect(args).toContain('--netplay-host');
    expect(args).toContain('127.0.0.1');
    expect(args).toContain('--netplay-port');
    expect(args).toContain('9004');
  });

  it('performancePreset "fast" adds --emumode 2 (dynarec)', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/mk64.z64', { performancePreset: 'fast' });
    expect(args).toContain('--emumode');
    expect(args).toContain('2');
  });

  it('performancePreset "accurate" adds --emumode 0 (interpreter)', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/mk64.z64', { performancePreset: 'accurate' });
    expect(args).toContain('--emumode');
    expect(args).toContain('0');
  });

  it('performancePreset "balanced" does not add --emumode', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/mk64.z64', { performancePreset: 'balanced' });
    expect(args).not.toContain('--emumode');
  });

  it('save path uses n64 subdirectory', () => {
    expect(adapter.getSavePath('n64-mario-kart-64', '/saves')).toBe('/saves/n64/n64-mario-kart-64');
  });

  it('compatibilityFlags are appended to args', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/mk64.z64', { compatibilityFlags: ['--noosd'] });
    expect(args).toContain('--noosd');
  });
});

// ---------------------------------------------------------------------------
// NDS adapter
// ---------------------------------------------------------------------------

describe('NDS system adapter (melonDS)', () => {
  const adapter = createSystemAdapter('nds');

  it('preferred backend is melonds', () => {
    expect(adapter.preferredBackendId).toBe('melonds');
  });

  it('fallback backend includes desmume', () => {
    expect(adapter.fallbackBackendIds).toContain('desmume');
  });

  it('buildLaunchArgs includes --wifi-host and --wifi-port for WFC relay', () => {
    const args = adapter.buildLaunchArgs('/roms/nds/mkds.nds', { netplayHost: '127.0.0.1', netplayPort: 9005 });
    expect(args).toContain('--wifi-host');
    expect(args).toContain('--wifi-port');
  });

  it('buildLaunchArgs includes --screen-layout', () => {
    const args = adapter.buildLaunchArgs('/roms/nds/mkds.nds', { screenLayout: 'side-by-side' });
    expect(args.join(' ')).toContain('--screen-layout=side-by-side');
  });

  it('buildLaunchArgs includes --wfc-dns when set', () => {
    const args = adapter.buildLaunchArgs('/roms/nds/pokemon-diamond.nds', { wfcDnsServer: '178.62.43.212' });
    expect(args.join(' ')).toContain('--wfc-dns=178.62.43.212');
  });

  it('save path uses nds subdirectory', () => {
    expect(adapter.getSavePath('nds-mario-kart-ds', '/saves')).toBe('/saves/nds/nds-mario-kart-ds');
  });

  it('compatibilityFlags are appended to args', () => {
    const args = adapter.buildLaunchArgs('/roms/nds/mkds.nds', { compatibilityFlags: ['--jit-enable'] });
    expect(args).toContain('--jit-enable');
  });
});

// ---------------------------------------------------------------------------
// Session templates — Phase 8 systems
// ---------------------------------------------------------------------------

describe('Session templates — Phase 8 systems', () => {
  const store = new SessionTemplateStore();

  it('NES has at least 5 session templates', () => {
    const templates = store.listAll().filter((t) => t.system === 'nes');
    expect(templates.length).toBeGreaterThanOrEqual(5);
  });

  it('SNES has at least 5 session templates', () => {
    const templates = store.listAll().filter((t) => t.system === 'snes');
    expect(templates.length).toBeGreaterThanOrEqual(5);
  });

  it('GB has at least 2 session templates', () => {
    const templates = store.listAll().filter((t) => t.system === 'gb');
    expect(templates.length).toBeGreaterThanOrEqual(2);
  });

  it('GBC has at least 2 session templates', () => {
    const templates = store.listAll().filter((t) => t.system === 'gbc');
    expect(templates.length).toBeGreaterThanOrEqual(2);
  });

  it('GBA has at least 5 session templates', () => {
    const templates = store.listAll().filter((t) => t.system === 'gba');
    expect(templates.length).toBeGreaterThanOrEqual(5);
  });

  it('N64 has at least 10 session templates', () => {
    const templates = store.listAll().filter((t) => t.system === 'n64');
    expect(templates.length).toBeGreaterThanOrEqual(10);
  });

  it('NDS has at least 10 session templates', () => {
    const templates = store.listAll().filter((t) => t.system === 'nds');
    expect(templates.length).toBeGreaterThanOrEqual(10);
  });

  it('nes-default-2p template exists', () => {
    expect(store.get('nes-default-2p')).toBeDefined();
  });

  it('snes-default-2p template exists', () => {
    expect(store.get('snes-default-2p')).toBeDefined();
  });

  it('n64-mario-kart-64-4p template exists', () => {
    expect(store.get('n64-mario-kart-64-4p')).toBeDefined();
  });

  it('nds-mario-kart-ds-4p template exists', () => {
    expect(store.get('nds-mario-kart-ds-4p')).toBeDefined();
  });
});

// ---------------------------------------------------------------------------
// Golden game catalog — Phase 8 systems
// ---------------------------------------------------------------------------

describe('Golden game catalog — Phase 8 systems', () => {
  it('NES has at least 9 games in the catalog', () => {
    const nesGames = MOCK_GAMES.filter((g) => g.system === 'NES');
    expect(nesGames.length).toBeGreaterThanOrEqual(9);
  });

  it('SNES has at least 9 games in the catalog', () => {
    const snesGames = MOCK_GAMES.filter((g) => g.system === 'SNES');
    expect(snesGames.length).toBeGreaterThanOrEqual(9);
  });

  it('GB has at least 6 games in the catalog', () => {
    const gbGames = MOCK_GAMES.filter((g) => g.system === 'GB');
    expect(gbGames.length).toBeGreaterThanOrEqual(6);
  });

  it('GBC has at least 4 games in the catalog', () => {
    const gbcGames = MOCK_GAMES.filter((g) => g.system === 'GBC');
    expect(gbcGames.length).toBeGreaterThanOrEqual(4);
  });

  it('GBA has at least 8 games in the catalog', () => {
    const gbaGames = MOCK_GAMES.filter((g) => g.system === 'GBA');
    expect(gbaGames.length).toBeGreaterThanOrEqual(8);
  });

  it('N64 has at least 9 games in the catalog', () => {
    const n64Games = MOCK_GAMES.filter((g) => g.system === 'N64');
    expect(n64Games.length).toBeGreaterThanOrEqual(9);
  });

  it('NDS has at least 9 non-DSiWare games in the catalog', () => {
    const ndsGames = MOCK_GAMES.filter((g) => g.system === 'NDS' && !g.isDsiWare);
    expect(ndsGames.length).toBeGreaterThanOrEqual(9);
  });

  it('NES includes nes-super-mario-bros in catalog', () => {
    expect(MOCK_GAMES.find((g) => g.id === 'nes-super-mario-bros')).toBeDefined();
  });

  it('NES includes nes-contra in catalog', () => {
    expect(MOCK_GAMES.find((g) => g.id === 'nes-contra')).toBeDefined();
  });

  it('SNES includes snes-street-fighter-ii-turbo in catalog', () => {
    expect(MOCK_GAMES.find((g) => g.id === 'snes-street-fighter-ii-turbo')).toBeDefined();
  });

  it('GBA includes gba-zelda-four-swords in catalog', () => {
    expect(MOCK_GAMES.find((g) => g.id === 'gba-zelda-four-swords')).toBeDefined();
  });

  it('N64 includes n64-pokemon-stadium in catalog', () => {
    expect(MOCK_GAMES.find((g) => g.id === 'n64-pokemon-stadium')).toBeDefined();
  });

  it('all NES games have system "NES"', () => {
    const nesGames = MOCK_GAMES.filter((g) => g.id.startsWith('nes-'));
    nesGames.forEach((g) => expect(g.system).toBe('NES'));
  });

  it('all SNES games have system "SNES"', () => {
    const snesGames = MOCK_GAMES.filter((g) => g.id.startsWith('snes-'));
    snesGames.forEach((g) => expect(g.system).toBe('SNES'));
  });
});

// ---------------------------------------------------------------------------
// Regression: no duplicate IDs in mock catalog
// ---------------------------------------------------------------------------

describe('No duplicate game IDs', () => {
  it('all MOCK_GAMES have unique IDs', () => {
    const ids = MOCK_GAMES.map((g) => g.id);
    const uniqueIds = new Set(ids);
    expect(uniqueIds.size).toBe(ids.length);
  });
});
