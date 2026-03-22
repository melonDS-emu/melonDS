/**
 * Phase 36 — Integration Hardening
 *
 * Tests for:
 *   - game-capability.ts  (centralized capability model)
 *   - launch-preflight.ts (structured launch preflight checks)
 */

import { describe, it, expect, vi } from 'vitest';
import {
  resolveGameCapability,
  SYSTEM_ONLINE_SUPPORT,
  onlineSupportBadgeColor,
  type OnlineSupportLevel,
} from '../game-capability';
import { runLaunchPreflight } from '../launch-preflight';

// Mock achievement-capability so tests don't depend on its implementation
vi.mock('../achievement-capability', () => ({
  getAchievementCapability: (system: string) => ({
    system,
    level: ['nes', 'snes', 'gba', 'sms', 'genesis', 'psx', 'gbc', 'gb', 'n64'].includes(system)
      ? 'full'
      : 'none',
    notes: '',
  }),
  systemSupportsAchievements: (system: string) =>
    ['nes', 'snes', 'gba', 'sms', 'genesis', 'psx', 'gbc', 'gb', 'n64'].includes(system),
}));

// ─────────────────────────────────────────────────────────────────────────────
// SYSTEM_ONLINE_SUPPORT matrix
// ─────────────────────────────────────────────────────────────────────────────

describe('SYSTEM_ONLINE_SUPPORT matrix', () => {
  it('marks NES, SNES, PSX, SMS, and Genesis as supported', () => {
    for (const sys of ['nes', 'snes', 'psx', 'sms', 'genesis']) {
      expect(SYSTEM_ONLINE_SUPPORT[sys]).toBe('supported');
    }
  });

  it('marks Wii U and 3DS as local-only', () => {
    expect(SYSTEM_ONLINE_SUPPORT['wiiu']).toBe('local-only');
    expect(SYSTEM_ONLINE_SUPPORT['3ds']).toBe('local-only');
  });

  it('marks N64, GBA, GBC, GB, NDS, GC, Wii, Dreamcast, PS2, PSP as experimental', () => {
    for (const sys of ['n64', 'gba', 'gbc', 'gb', 'nds', 'gc', 'wii', 'dreamcast', 'ps2', 'psp']) {
      expect(SYSTEM_ONLINE_SUPPORT[sys]).toBe('experimental');
    }
  });

  it('covers all 17 supported systems', () => {
    const systems = [
      'nes', 'snes', 'gb', 'gbc', 'gba', 'n64', 'nds', 'gc', 'wii', 'wiiu',
      '3ds', 'genesis', 'sms', 'dreamcast', 'psx', 'ps2', 'psp',
    ];
    for (const sys of systems) {
      expect(SYSTEM_ONLINE_SUPPORT[sys]).toBeDefined();
    }
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// resolveGameCapability
// ─────────────────────────────────────────────────────────────────────────────

describe('resolveGameCapability', () => {
  it('returns canPlayLocally=false when emulator and ROM are missing', () => {
    const cap = resolveGameCapability('nes', {});
    expect(cap.canPlayLocally).toBe(false);
    expect(cap.localPlayBlockReason).toBeTruthy();
  });

  it('returns canPlayLocally=false when only ROM is missing', () => {
    const cap = resolveGameCapability('snes', { emulatorPath: '/usr/bin/snes9x' });
    expect(cap.canPlayLocally).toBe(false);
    expect(cap.localPlayBlockReason).toMatch(/ROM/i);
  });

  it('returns canPlayLocally=false when only emulator is missing', () => {
    const cap = resolveGameCapability('gba', { romPath: '/roms/game.gba' });
    expect(cap.canPlayLocally).toBe(false);
    expect(cap.localPlayBlockReason).toMatch(/emulator/i);
  });

  it('returns canPlayLocally=true when both emulator and ROM are provided', () => {
    const cap = resolveGameCapability('nes', {
      emulatorPath: '/usr/bin/fceux',
      romPath: '/roms/mario.nes',
    });
    expect(cap.canPlayLocally).toBe(true);
    expect(cap.localPlayBlockReason).toBeUndefined();
  });

  it('includes configure-emulator action when emulator is missing', () => {
    const cap = resolveGameCapability('sms', { romPath: '/roms/game.sms' });
    expect(cap.availableActions).toContain('configure-emulator');
    expect(cap.availableActions).not.toContain('play');
  });

  it('includes play action when emulator and ROM are both present', () => {
    const cap = resolveGameCapability('sms', {
      emulatorPath: '/usr/bin/retroarch',
      romPath: '/roms/game.sms',
    });
    expect(cap.availableActions).toContain('play');
  });

  it('always includes host-room and join-room actions', () => {
    const cap = resolveGameCapability('nes', {});
    expect(cap.availableActions).toContain('host-room');
    expect(cap.availableActions).toContain('join-room');
  });

  it('includes configure-achievements when RA not connected and system supports achievements', () => {
    const cap = resolveGameCapability('sms', {
      emulatorPath: '/usr/bin/retroarch',
      romPath: '/roms/game.sms',
      raConnected: false,
    });
    expect(cap.availableActions).toContain('configure-achievements');
  });

  it('excludes configure-achievements when RA is connected', () => {
    const cap = resolveGameCapability('sms', {
      emulatorPath: '/usr/bin/retroarch',
      romPath: '/roms/game.sms',
      raConnected: true,
    });
    expect(cap.availableActions).not.toContain('configure-achievements');
  });

  it('excludes configure-achievements for systems without achievement support', () => {
    const cap = resolveGameCapability('wiiu', {
      emulatorPath: '/usr/bin/cemu',
      romPath: '/roms/game.wud',
      raConnected: false,
    });
    expect(cap.availableActions).not.toContain('configure-achievements');
  });

  it('returns correct online support for SMS (supported)', () => {
    const cap = resolveGameCapability('sms', {});
    expect(cap.onlineSupportLevel).toBe('supported');
    expect(cap.onlineSupportLabel).toBe('Online Ready');
    expect(cap.onlineSupportNote).toBeTruthy();
  });

  it('returns correct online support for Wii U (local-only)', () => {
    const cap = resolveGameCapability('wiiu', {});
    expect(cap.onlineSupportLevel).toBe('local-only');
    expect(cap.onlineSupportLabel).toBe('Local Only');
  });

  it('returns correct online support for N64 (experimental)', () => {
    const cap = resolveGameCapability('n64', {});
    expect(cap.onlineSupportLevel).toBe('experimental');
    expect(cap.onlineSupportLabel).toBe('Experimental');
  });

  it('returns correct online support for Genesis (supported)', () => {
    const cap = resolveGameCapability('genesis', {});
    expect(cap.onlineSupportLevel).toBe('supported');
  });

  it('canHostRoom and canJoinRoom are always true', () => {
    for (const sys of ['nes', 'wiiu', '3ds', 'psp']) {
      const cap = resolveGameCapability(sys, {});
      expect(cap.canHostRoom).toBe(true);
      expect(cap.canJoinRoom).toBe(true);
    }
  });

  it('handles unknown systems gracefully', () => {
    const cap = resolveGameCapability('unknown-system', {});
    expect(cap.onlineSupportLevel).toBe('supported'); // fallback
    expect(cap.onlineSupportNote).toBeTruthy();
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// onlineSupportBadgeColor
// ─────────────────────────────────────────────────────────────────────────────

describe('onlineSupportBadgeColor', () => {
  it('returns non-empty bg and text for all three levels', () => {
    const levels: OnlineSupportLevel[] = ['supported', 'experimental', 'local-only'];
    for (const level of levels) {
      const c = onlineSupportBadgeColor(level);
      expect(c.bg).toBeTruthy();
      expect(c.text).toBeTruthy();
    }
  });

  it('returns distinct text colours for all three levels', () => {
    const s = onlineSupportBadgeColor('supported');
    const e = onlineSupportBadgeColor('experimental');
    const l = onlineSupportBadgeColor('local-only');
    expect(s.text).not.toBe(e.text);
    expect(e.text).not.toBe(l.text);
    expect(s.text).not.toBe(l.text);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// runLaunchPreflight
// ─────────────────────────────────────────────────────────────────────────────

describe('runLaunchPreflight', () => {
  it('blocks when ROM is missing', () => {
    const result = runLaunchPreflight({
      system: 'nes',
      mode: 'local',
      emulatorPath: '/usr/bin/fceux',
    });
    expect(result.canLaunch).toBe(false);
    expect(result.status).toBe('blocked');
    const check = result.issues.find((c) => c.id === 'rom-missing');
    expect(check).toBeDefined();
    expect(check?.status).toBe('blocked');
  });

  it('blocks when emulator is not configured', () => {
    const result = runLaunchPreflight({
      system: 'sms',
      mode: 'local',
      romPath: '/roms/game.sms',
    });
    expect(result.canLaunch).toBe(false);
    const check = result.issues.find((c) => c.id === 'emulator-not-configured');
    expect(check).toBeDefined();
    expect(check?.message).toMatch(/settings/i);
  });

  it('passes cleanly when both ROM and emulator are set and RA is connected', () => {
    const result = runLaunchPreflight({
      system: 'snes',
      mode: 'local',
      romPath: '/roms/game.sfc',
      emulatorPath: '/usr/bin/snes9x',
      raConnected: true,
    });
    expect(result.canLaunch).toBe(true);
    expect(result.status).toBe('ok');
    expect(result.issues).toHaveLength(0);
  });

  it('warns (does not block) when RA not connected but system supports achievements', () => {
    const result = runLaunchPreflight({
      system: 'nes',
      mode: 'local',
      romPath: '/roms/game.nes',
      emulatorPath: '/usr/bin/fceux',
      raConnected: false,
    });
    expect(result.canLaunch).toBe(true);
    expect(result.status).toBe('warning');
    const check = result.issues.find((c) => c.id === 'achievements-not-configured');
    expect(check?.status).toBe('warning');
  });

  it('does not warn about achievements for systems that do not support them', () => {
    const result = runLaunchPreflight({
      system: 'wiiu',
      mode: 'local',
      romPath: '/roms/game.wud',
      emulatorPath: '/usr/bin/cemu',
      raConnected: false,
    });
    expect(result.issues.find((c) => c.id === 'achievements-not-configured')).toBeUndefined();
  });

  it('warns for experimental online system in online mode', () => {
    const result = runLaunchPreflight({
      system: 'n64',
      mode: 'online',
      romPath: '/roms/game.n64',
      emulatorPath: '/usr/bin/mupen64plus',
    });
    expect(result.canLaunch).toBe(true);
    const check = result.issues.find((c) => c.id === 'online-experimental');
    expect(check?.status).toBe('warning');
    expect(check?.message).toMatch(/experimental/i);
  });

  it('warns for local-only system in online mode', () => {
    const result = runLaunchPreflight({
      system: 'wiiu',
      mode: 'online',
      romPath: '/roms/game.wud',
      emulatorPath: '/usr/bin/cemu',
    });
    const check = result.issues.find((c) => c.id === 'online-local-only');
    expect(check?.status).toBe('warning');
    expect(check?.message).toMatch(/local/i);
    expect(result.canLaunch).toBe(true);
  });

  it('does not add any online check in local mode', () => {
    const result = runLaunchPreflight({
      system: 'wiiu',
      mode: 'local',
      romPath: '/roms/game.wud',
      emulatorPath: '/usr/bin/cemu',
    });
    expect(result.issues.find((c) => c.id.startsWith('online-'))).toBeUndefined();
  });

  it('does not warn for fully supported system in online mode', () => {
    const result = runLaunchPreflight({
      system: 'sms',
      mode: 'online',
      romPath: '/roms/game.sms',
      emulatorPath: '/usr/bin/retroarch',
    });
    expect(result.issues.find((c) => c.id.startsWith('online-'))).toBeUndefined();
    expect(result.canLaunch).toBe(true);
  });

  it('blocks with two issues when both ROM and emulator are missing', () => {
    const result = runLaunchPreflight({ system: 'gba', mode: 'local' });
    expect(result.canLaunch).toBe(false);
    expect(result.issues.filter((c) => c.status === 'blocked')).toHaveLength(2);
  });

  it('allChecks includes passing checks as well as issues', () => {
    const result = runLaunchPreflight({
      system: 'snes',
      mode: 'local',
      romPath: '/roms/game.sfc',
      emulatorPath: '/usr/bin/snes9x',
      raConnected: true,
    });
    expect(result.allChecks.length).toBeGreaterThan(0);
    expect(result.allChecks.every((c) => c.status === 'ok')).toBe(true);
  });
});
