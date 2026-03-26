/**
 * Phase 40 — N64 Overhaul
 *
 * Tests for:
 *  - Expanded N64 mock game catalog (15 games total)
 *  - New N64 session templates (19+ total)
 *  - Extended N64 netplay whitelist (16 N64 entries)
 *  - Expanded N64 game metadata (15 entries)
 *  - N64 adapter new flags: transferPakEnabled, controllerPakMode, n64SaveType
 *  - parallel-n64 in N64 fallback backends
 *  - OnlineSupportLevel 'partial' added; N64 upgraded from 'experimental'
 *  - onlineSupportIcon / onlineSupportSummary / onlineSupportBadgeColor for 'partial'
 *  - N64 capability note updated
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';
import { GameMetadataStore } from '../game-metadata-store';
import { NETPLAY_WHITELIST, approvedGamesForSystem } from '../../../../apps/desktop/src/lib/netplay-whitelist';
import {
  SYSTEM_ONLINE_SUPPORT,
  onlineSupportIcon,
  onlineSupportSummary,
  onlineSupportBadgeColor,
  resolveGameCapability,
} from '../../../../apps/desktop/src/lib/game-capability';

// ─────────────────────────────────────────────────────────────────────────────
// Expanded mock game catalog
// ─────────────────────────────────────────────────────────────────────────────

describe('N64 mock game catalog — overhaul', () => {
  const n64Games = MOCK_GAMES.filter((g) => g.system === 'N64');

  it('has at least 15 N64 games', () => {
    expect(n64Games.length).toBeGreaterThanOrEqual(15);
  });

  it('includes Wave Race 64', () => {
    expect(n64Games.find((g) => g.id === 'n64-wave-race-64')).toBeDefined();
  });

  it('includes Star Fox 64', () => {
    expect(n64Games.find((g) => g.id === 'n64-star-fox-64')).toBeDefined();
  });

  it('includes F-Zero X', () => {
    expect(n64Games.find((g) => g.id === 'n64-f-zero-x')).toBeDefined();
  });

  it('includes Perfect Dark', () => {
    expect(n64Games.find((g) => g.id === 'n64-perfect-dark')).toBeDefined();
  });

  it('includes Mario Party (original)', () => {
    expect(n64Games.find((g) => g.id === 'n64-mario-party')).toBeDefined();
  });

  it('includes Bomberman 64', () => {
    expect(n64Games.find((g) => g.id === 'n64-bomberman-64')).toBeDefined();
  });

  it('Wave Race 64 is 2-player max', () => {
    const game = n64Games.find((g) => g.id === 'n64-wave-race-64');
    expect(game?.maxPlayers).toBe(2);
  });

  it('new 4-player games have maxPlayers = 4', () => {
    const ids = ['n64-star-fox-64', 'n64-f-zero-x', 'n64-perfect-dark', 'n64-mario-party', 'n64-bomberman-64'];
    for (const id of ids) {
      const game = n64Games.find((g) => g.id === id);
      expect(game?.maxPlayers).toBe(4);
    }
  });

  it('all N64 games use system color #009900', () => {
    for (const game of n64Games) {
      expect(game.systemColor).toBe('#009900');
    }
  });

  it('has no duplicate N64 game IDs', () => {
    const ids = n64Games.map((g) => g.id);
    expect(ids.length).toBe(new Set(ids).size);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Session templates
// ─────────────────────────────────────────────────────────────────────────────

describe('N64 session templates — overhaul', () => {
  const store = new SessionTemplateStore();
  const n64Templates = store.listAll().filter((t) => t.system === 'n64');

  it('has at least 19 N64 session templates', () => {
    expect(n64Templates.length).toBeGreaterThanOrEqual(19);
  });

  it('n64-wave-race-64-2p template exists with latencyTarget 80', () => {
    const t = store.get('n64-wave-race-64-2p');
    expect(t).toBeDefined();
    expect(t?.playerCount).toBe(2);
    expect(t?.latencyTarget).toBe(80);
  });

  it('n64-star-fox-64-4p template exists', () => {
    const t = store.get('n64-star-fox-64-4p');
    expect(t).toBeDefined();
    expect(t?.playerCount).toBe(4);
  });

  it('n64-f-zero-x-4p template exists with latencyTarget 120', () => {
    const t = store.get('n64-f-zero-x-4p');
    expect(t).toBeDefined();
    expect(t?.latencyTarget).toBe(120);
  });

  it('n64-perfect-dark-4p template exists', () => {
    expect(store.get('n64-perfect-dark-4p')).toBeDefined();
  });

  it('n64-mario-party-4p template exists', () => {
    expect(store.get('n64-mario-party-4p')).toBeDefined();
  });

  it('n64-bomberman-64-4p template exists', () => {
    expect(store.get('n64-bomberman-64-4p')).toBeDefined();
  });

  it('all new N64 templates use mupen64plus backend', () => {
    const newIds = [
      'n64-wave-race-64-2p', 'n64-star-fox-64-4p', 'n64-f-zero-x-4p',
      'n64-perfect-dark-4p', 'n64-mario-party-4p', 'n64-bomberman-64-4p',
    ];
    for (const id of newIds) {
      expect(store.get(id)?.emulatorBackendId).toBe('mupen64plus');
    }
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Netplay whitelist
// ─────────────────────────────────────────────────────────────────────────────

describe('N64 netplay whitelist — overhaul', () => {
  const n64Entries = NETPLAY_WHITELIST.filter((e) => e.system === 'n64');

  it('has at least 15 N64 whitelist entries', () => {
    expect(n64Entries.length).toBeGreaterThanOrEqual(15);
  });

  it('n64-diddy-kong-racing is approved', () => {
    const entry = n64Entries.find((e) => e.gameId === 'n64-diddy-kong-racing');
    expect(entry?.rating).toBe('approved');
  });

  it('n64-mario-tennis is approved', () => {
    const entry = n64Entries.find((e) => e.gameId === 'n64-mario-tennis');
    expect(entry?.rating).toBe('approved');
  });

  it('n64-mario-golf is approved', () => {
    const entry = n64Entries.find((e) => e.gameId === 'n64-mario-golf');
    expect(entry?.rating).toBe('approved');
  });

  it('n64-pokemon-stadium is caution', () => {
    const entry = n64Entries.find((e) => e.gameId === 'n64-pokemon-stadium');
    expect(entry?.rating).toBe('caution');
  });

  it('n64-pokemon-stadium-2 is caution', () => {
    const entry = n64Entries.find((e) => e.gameId === 'n64-pokemon-stadium-2');
    expect(entry?.rating).toBe('caution');
  });

  it('n64-wave-race-64 is approved with 2 max players', () => {
    const entry = n64Entries.find((e) => e.gameId === 'n64-wave-race-64');
    expect(entry?.rating).toBe('approved');
    expect(entry?.recommendedMaxPlayers).toBe(2);
  });

  it('n64-star-fox-64 is approved', () => {
    const entry = n64Entries.find((e) => e.gameId === 'n64-star-fox-64');
    expect(entry?.rating).toBe('approved');
  });

  it('n64-perfect-dark is caution', () => {
    const entry = n64Entries.find((e) => e.gameId === 'n64-perfect-dark');
    expect(entry?.rating).toBe('caution');
  });

  it('n64-mario-party is approved', () => {
    const entry = n64Entries.find((e) => e.gameId === 'n64-mario-party');
    expect(entry?.rating).toBe('approved');
  });

  it('n64-bomberman-64 is approved', () => {
    const entry = n64Entries.find((e) => e.gameId === 'n64-bomberman-64');
    expect(entry?.rating).toBe('approved');
  });

  it('approvedGamesForSystem returns ≥ 8 approved N64 games', () => {
    const approved = approvedGamesForSystem('n64');
    expect(approved.length).toBeGreaterThanOrEqual(8);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Game metadata store
// ─────────────────────────────────────────────────────────────────────────────

describe('N64 game metadata — overhaul', () => {
  const store = new GameMetadataStore();

  it('has metadata for n64-goldeneye-007', () => {
    const meta = store.get('n64-goldeneye-007');
    expect(meta).toBeDefined();
    expect(meta?.genre).toContain('Shooter');
  });

  it('has metadata for n64-mario-tennis', () => {
    const meta = store.get('n64-mario-tennis');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Camelot');
  });

  it('has metadata for n64-mario-golf', () => {
    const meta = store.get('n64-mario-golf');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Camelot');
  });

  it('has metadata for n64-pokemon-stadium', () => {
    const meta = store.get('n64-pokemon-stadium');
    expect(meta).toBeDefined();
    expect(meta?.genre).toContain('Battle');
  });

  it('has metadata for n64-pokemon-stadium-2', () => {
    const meta = store.get('n64-pokemon-stadium-2');
    expect(meta).toBeDefined();
    expect(meta?.year).toBe(2000);
  });

  it('has metadata for n64-wave-race-64', () => {
    const meta = store.get('n64-wave-race-64');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Nintendo');
  });

  it('has metadata for n64-star-fox-64', () => {
    const meta = store.get('n64-star-fox-64');
    expect(meta).toBeDefined();
    expect(meta?.year).toBe(1997);
  });

  it('has metadata for n64-f-zero-x', () => {
    const meta = store.get('n64-f-zero-x');
    expect(meta).toBeDefined();
    expect(meta?.genre).toContain('Racing');
  });

  it('has metadata for n64-perfect-dark', () => {
    const meta = store.get('n64-perfect-dark');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Rare');
    expect(meta?.year).toBe(2000);
  });

  it('has metadata for n64-mario-party', () => {
    const meta = store.get('n64-mario-party');
    expect(meta).toBeDefined();
    expect(meta?.genre).toContain('Party');
  });

  it('has metadata for n64-bomberman-64', () => {
    const meta = store.get('n64-bomberman-64');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Hudson Soft');
  });

  it('all new N64 metadata entries have netplayTips', () => {
    const newIds = [
      'n64-goldeneye-007', 'n64-mario-tennis', 'n64-mario-golf',
      'n64-pokemon-stadium', 'n64-pokemon-stadium-2', 'n64-wave-race-64',
      'n64-star-fox-64', 'n64-f-zero-x', 'n64-perfect-dark',
      'n64-mario-party', 'n64-bomberman-64',
    ];
    for (const id of newIds) {
      const meta = store.get(id);
      expect(meta?.netplayTips?.length).toBeGreaterThan(0);
    }
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// N64 adapter — new flags
// ─────────────────────────────────────────────────────────────────────────────

describe('N64 adapter — new accessory flags', () => {
  const adapter = createSystemAdapter('n64');

  it('parallel-n64 is in fallback backends', () => {
    expect(adapter.fallbackBackendIds).toContain('parallel-n64');
  });

  it('transferPakEnabled adds --transfer-pak flag', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/stadium.z64', { transferPakEnabled: true });
    expect(args).toContain('--transfer-pak');
  });

  it('transferPakEnabled false does not add --transfer-pak', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/stadium.z64', { transferPakEnabled: false });
    expect(args).not.toContain('--transfer-pak');
  });

  it('controllerPakMode standard adds --controller-pak standard', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/mk64.z64', { controllerPakMode: 'standard' });
    expect(args).toContain('--controller-pak');
    expect(args).toContain('standard');
  });

  it('controllerPakMode rumble adds --controller-pak rumble', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/mk64.z64', { controllerPakMode: 'rumble' });
    expect(args).toContain('--controller-pak');
    expect(args).toContain('rumble');
  });

  it('controllerPakMode none does NOT add --controller-pak', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/mk64.z64', { controllerPakMode: 'none' });
    expect(args).not.toContain('--controller-pak');
  });

  it('n64SaveType eeprom-4k adds --savetype eeprom-4k', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/game.z64', { n64SaveType: 'eeprom-4k' });
    expect(args).toContain('--savetype');
    expect(args).toContain('eeprom-4k');
  });

  it('n64SaveType sram adds --savetype sram', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/zelda.z64', { n64SaveType: 'sram' });
    expect(args).toContain('--savetype');
    expect(args).toContain('sram');
  });

  it('n64SaveType flash adds --savetype flash', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/snap.z64', { n64SaveType: 'flash' });
    expect(args).toContain('--savetype');
    expect(args).toContain('flash');
  });

  it('omitting n64SaveType does NOT add --savetype', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/mk64.z64', {});
    expect(args).not.toContain('--savetype');
  });

  it('parallel-n64 backend uses RetroArch args (no --rom flag)', () => {
    const parallelAdapter = createSystemAdapter('n64', 'parallel-n64');
    const args = parallelAdapter.buildLaunchArgs('/roms/n64/mk64.z64', {});
    expect(args).not.toContain('--rom');
    expect(args).toContain('/roms/n64/mk64.z64');
  });

  it('all new flags can be combined in one call', () => {
    const args = adapter.buildLaunchArgs('/roms/n64/stadium.z64', {
      transferPakEnabled: true,
      controllerPakMode: 'standard',
      n64SaveType: 'eeprom-4k',
      netplayHost: '10.0.0.1',
      netplayPort: 9001,
      performancePreset: 'balanced',
    });
    expect(args).toContain('--transfer-pak');
    expect(args).toContain('--controller-pak');
    expect(args).toContain('standard');
    expect(args).toContain('--savetype');
    expect(args).toContain('eeprom-4k');
    expect(args).toContain('--netplay-host');
    expect(args).not.toContain('--emumode');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// OnlineSupportLevel 'partial' + N64 upgrade
// ─────────────────────────────────────────────────────────────────────────────

describe('OnlineSupportLevel partial — N64 upgrade', () => {
  it("N64 online support is now 'partial'", () => {
    expect(SYSTEM_ONLINE_SUPPORT['n64']).toBe('partial');
  });

  it("onlineSupportIcon returns '🌍' for partial", () => {
    expect(onlineSupportIcon('partial')).toBe('🌍');
  });

  it('onlineSupportSummary mentions Netplay Whitelist for partial', () => {
    const summary = onlineSupportSummary('partial');
    expect(summary.toLowerCase()).toContain('netplay whitelist');
  });

  it('onlineSupportBadgeColor returns distinct colors for partial', () => {
    const partial = onlineSupportBadgeColor('partial');
    const experimental = onlineSupportBadgeColor('experimental');
    const supported = onlineSupportBadgeColor('supported');
    expect(partial.bg).not.toBe(experimental.bg);
    expect(partial.bg).not.toBe(supported.bg);
    expect(partial.text).toBeTruthy();
  });

  it('N64 capability resolves with level partial', () => {
    const cap = resolveGameCapability('n64');
    expect(cap.onlineSupportLevel).toBe('partial');
  });

  it('N64 capability note mentions Mupen64Plus', () => {
    const cap = resolveGameCapability('n64');
    expect(cap.onlineSupportNote.toLowerCase()).toContain('mupen64plus');
  });
});
