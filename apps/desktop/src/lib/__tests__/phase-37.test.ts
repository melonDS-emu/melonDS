/**
 * Phase 37 — BIOS Validation, Netplay Whitelist & RetroArch Enhancement Presets
 *
 * Tests for:
 *   - bios-validator.ts   (BIOS registry, hash validation)
 *   - netplay-whitelist.ts (curated game ratings, helpers)
 *   - retroarch-presets.ts (named presets, netplay-safe filtering)
 *   - launch-preflight.ts  (BIOS check integration)
 */

import { describe, it, expect } from 'vitest';
import {
  BIOS_REQUIREMENTS,
  systemRequiresBios,
  getBiosRequirements,
  validateBiosEntry,
  validateAllBios,
} from '../bios-validator';
import {
  NETPLAY_WHITELIST,
  getNetplayEntry,
  isNetplayApproved,
  isNetplayIncompatible,
  getNetplayWarning,
  approvedGamesForSystem,
} from '../netplay-whitelist';
import {
  RETROARCH_PRESETS,
  getPreset,
  netplaySafePresets,
  singlePlayerPresets,
  getPresetOnlineWarning,
  applyPreset,
} from '../retroarch-presets';
import { runLaunchPreflight } from '../launch-preflight';

// ─────────────────────────────────────────────────────────────────────────────
// bios-validator — registry shape
// ─────────────────────────────────────────────────────────────────────────────

describe('BIOS_REQUIREMENTS registry', () => {
  it('contains entries for psx, gba, nds, and dreamcast', () => {
    for (const sys of ['psx', 'gba', 'nds', 'dreamcast']) {
      expect(BIOS_REQUIREMENTS[sys]).toBeDefined();
    }
  });

  it('every entry has a non-empty filename, description, and at least one knownMd5', () => {
    for (const req of Object.values(BIOS_REQUIREMENTS)) {
      for (const entry of req.entries) {
        expect(entry.filename.length).toBeGreaterThan(0);
        expect(entry.description.length).toBeGreaterThan(0);
        expect(entry.knownMd5.length).toBeGreaterThan(0);
      }
    }
  });

  it('all MD5 hashes are lowercase 32-char hex strings', () => {
    const md5Re = /^[0-9a-f]{32}$/;
    for (const req of Object.values(BIOS_REQUIREMENTS)) {
      for (const entry of req.entries) {
        for (const hash of entry.knownMd5) {
          expect(hash).toMatch(md5Re);
        }
      }
    }
  });

  it('PSX has at least one required BIOS entry', () => {
    const psxReq = BIOS_REQUIREMENTS['psx'];
    expect(psxReq.entries.some((e) => e.required)).toBe(true);
  });

  it('NDS has at least three entries (bios7, bios9, firmware)', () => {
    expect(BIOS_REQUIREMENTS['nds'].entries.length).toBeGreaterThanOrEqual(3);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// bios-validator — systemRequiresBios
// ─────────────────────────────────────────────────────────────────────────────

describe('systemRequiresBios()', () => {
  it('returns true for psx', () => expect(systemRequiresBios('psx')).toBe(true));
  it('returns true for nds', () => expect(systemRequiresBios('nds')).toBe(true));
  it('returns true for dreamcast', () => expect(systemRequiresBios('dreamcast')).toBe(true));
  it('returns false for gba (no required entries)', () => expect(systemRequiresBios('gba')).toBe(false));
  it('returns false for nes (not in registry)', () => expect(systemRequiresBios('nes')).toBe(false));
  it('is case-insensitive', () => expect(systemRequiresBios('PSX')).toBe(true));
});

// ─────────────────────────────────────────────────────────────────────────────
// bios-validator — getBiosRequirements
// ─────────────────────────────────────────────────────────────────────────────

describe('getBiosRequirements()', () => {
  it('returns the requirement object for psx', () => {
    const req = getBiosRequirements('psx');
    expect(req).not.toBeNull();
    expect(req!.system).toBe('psx');
  });

  it('returns null for unknown system', () => {
    expect(getBiosRequirements('atari2600')).toBeNull();
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// bios-validator — validateBiosEntry
// ─────────────────────────────────────────────────────────────────────────────

describe('validateBiosEntry()', () => {
  const PSX_SCPH1001_MD5 = '37157331346bb75cbddca0be29eaa6ea';

  it('passes when hash matches a known-good PSX BIOS', () => {
    const result = validateBiosEntry('psx', 'scph1001.bin', PSX_SCPH1001_MD5);
    expect(result.hashMatch).toBe(true);
    expect(result.blocked).toBe(false);
    expect(result.provided).toBe(true);
  });

  it('blocks when required BIOS is not provided (null md5)', () => {
    const result = validateBiosEntry('psx', 'scph1001.bin', null);
    expect(result.provided).toBe(false);
    expect(result.blocked).toBe(true);
  });

  it('blocks when required BIOS hash does not match', () => {
    const result = validateBiosEntry('psx', 'scph1001.bin', 'aabbccdd' + '00'.repeat(14));
    expect(result.hashMatch).toBe(false);
    expect(result.blocked).toBe(true);
  });

  it('does not block for optional GBA BIOS when not provided', () => {
    const result = validateBiosEntry('gba', 'gba_bios.bin', null);
    expect(result.blocked).toBe(false);
    expect(result.provided).toBe(false);
  });

  it('does not block for unknown BIOS filename', () => {
    const result = validateBiosEntry('psx', 'unknown_file.bin', null);
    expect(result.blocked).toBe(false);
  });

  it('is case-insensitive for filename', () => {
    const lower = validateBiosEntry('psx', 'scph1001.bin', PSX_SCPH1001_MD5);
    const upper = validateBiosEntry('PSX', 'SCPH1001.BIN', PSX_SCPH1001_MD5);
    expect(lower.hashMatch).toBe(true);
    expect(upper.hashMatch).toBe(true);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// bios-validator — validateAllBios
// ─────────────────────────────────────────────────────────────────────────────

describe('validateAllBios()', () => {
  it('returns one result per registered entry', () => {
    const results = validateAllBios('psx', {});
    expect(results.length).toBe(BIOS_REQUIREMENTS['psx'].entries.length);
  });

  it('returns empty array for unknown system', () => {
    expect(validateAllBios('atari2600', {})).toHaveLength(0);
  });

  it('detects a valid PSX BIOS and passes', () => {
    const md5 = '37157331346bb75cbddca0be29eaa6ea';
    const results = validateAllBios('psx', { 'scph1001.bin': md5 });
    const main = results.find((r) => r.filename === 'scph1001.bin')!;
    expect(main.hashMatch).toBe(true);
    expect(main.blocked).toBe(false);
  });

  it('blocks when all BIOS files are absent for PSX', () => {
    const results = validateAllBios('psx', {});
    expect(results.some((r) => r.blocked)).toBe(true);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// netplay-whitelist — catalog shape
// ─────────────────────────────────────────────────────────────────────────────

describe('NETPLAY_WHITELIST catalog', () => {
  it('is non-empty', () => {
    expect(NETPLAY_WHITELIST.length).toBeGreaterThan(0);
  });

  it('every entry has a non-empty gameId, system, and reason', () => {
    for (const entry of NETPLAY_WHITELIST) {
      expect(entry.gameId.length).toBeGreaterThan(0);
      expect(entry.system.length).toBeGreaterThan(0);
      expect(entry.reason.length).toBeGreaterThan(0);
    }
  });

  it('all ratings are valid NetplayRating values', () => {
    const valid = new Set(['approved', 'caution', 'incompatible']);
    for (const entry of NETPLAY_WHITELIST) {
      expect(valid.has(entry.rating)).toBe(true);
    }
  });

  it('gameIds are unique', () => {
    const ids = NETPLAY_WHITELIST.map((e) => e.gameId);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it('includes PSX, NES, SNES, GBA, N64, Genesis, NDS entries', () => {
    const systems = new Set(NETPLAY_WHITELIST.map((e) => e.system));
    for (const sys of ['psx', 'nes', 'snes', 'gba', 'n64', 'genesis', 'nds']) {
      expect(systems.has(sys)).toBe(true);
    }
  });

  it('has at least 30 entries covering key systems', () => {
    expect(NETPLAY_WHITELIST.length).toBeGreaterThanOrEqual(30);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// netplay-whitelist — getNetplayEntry
// ─────────────────────────────────────────────────────────────────────────────

describe('getNetplayEntry()', () => {
  it('returns the entry for a known gameId', () => {
    const entry = getNetplayEntry('psx-crash-team-racing');
    expect(entry).not.toBeNull();
    expect(entry!.system).toBe('psx');
    expect(entry!.rating).toBe('approved');
  });

  it('returns null for an unknown gameId', () => {
    expect(getNetplayEntry('sega-saturn-panzer-dragoon')).toBeNull();
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// netplay-whitelist — isNetplayApproved / isNetplayIncompatible / getNetplayWarning
// ─────────────────────────────────────────────────────────────────────────────

describe('isNetplayApproved()', () => {
  it('returns true for an approved game', () => {
    expect(isNetplayApproved('nes-super-mario-bros')).toBe(true);
  });

  it('returns false for a caution-rated game', () => {
    expect(isNetplayApproved('psx-resident-evil-2')).toBe(false);
  });

  it('returns false for an unknown game', () => {
    expect(isNetplayApproved('unknown-game-id')).toBe(false);
  });
});

describe('isNetplayIncompatible()', () => {
  it('returns false for an approved game', () => {
    expect(isNetplayIncompatible('psx-crash-team-racing')).toBe(false);
  });

  it('returns false for an unknown game', () => {
    expect(isNetplayIncompatible('unknown-game-id')).toBe(false);
  });
});

describe('getNetplayWarning()', () => {
  it('returns null for an approved game', () => {
    expect(getNetplayWarning('psx-crash-team-racing')).toBeNull();
  });

  it('returns a warning string for a caution-rated game', () => {
    const warning = getNetplayWarning('psx-resident-evil-2');
    expect(warning).toBeTruthy();
    expect(typeof warning).toBe('string');
  });

  it('returns null for an unknown game', () => {
    expect(getNetplayWarning('unknown-game-id')).toBeNull();
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// netplay-whitelist — approvedGamesForSystem
// ─────────────────────────────────────────────────────────────────────────────

describe('approvedGamesForSystem()', () => {
  it('returns only approved NES games', () => {
    const approved = approvedGamesForSystem('nes');
    expect(approved.length).toBeGreaterThan(0);
    for (const e of approved) {
      expect(e.system).toBe('nes');
      expect(e.rating).toBe('approved');
    }
  });

  it('is case-insensitive', () => {
    const lower = approvedGamesForSystem('psx');
    const upper = approvedGamesForSystem('PSX');
    expect(lower.length).toBe(upper.length);
  });

  it('returns empty array for system with no approved games', () => {
    expect(approvedGamesForSystem('atari2600')).toHaveLength(0);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// retroarch-presets — catalog shape
// ─────────────────────────────────────────────────────────────────────────────

describe('RETROARCH_PRESETS catalog', () => {
  it('contains at least 4 netplay-safe presets', () => {
    expect(netplaySafePresets().length).toBeGreaterThanOrEqual(4);
  });

  it('contains at least 3 single-player presets', () => {
    expect(singlePlayerPresets().length).toBeGreaterThanOrEqual(3);
  });

  it('every preset has a non-empty id, name, and description', () => {
    for (const preset of RETROARCH_PRESETS) {
      expect(preset.id.length).toBeGreaterThan(0);
      expect(preset.name.length).toBeGreaterThan(0);
      expect(preset.description.length).toBeGreaterThan(0);
    }
  });

  it('preset ids are unique', () => {
    const ids = RETROARCH_PRESETS.map((p) => p.id);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it('non-safe presets have an onlineWarning', () => {
    for (const preset of RETROARCH_PRESETS) {
      if (!preset.netplaySafe) {
        expect(preset.onlineWarning).toBeTruthy();
      }
    }
  });

  it('all settings values are strings', () => {
    for (const preset of RETROARCH_PRESETS) {
      for (const [, v] of Object.entries(preset.settings)) {
        expect(typeof v).toBe('string');
      }
    }
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// retroarch-presets — getPreset
// ─────────────────────────────────────────────────────────────────────────────

describe('getPreset()', () => {
  it('returns the clean preset', () => {
    const p = getPreset('clean');
    expect(p).not.toBeNull();
    expect(p!.netplaySafe).toBe(true);
  });

  it('returns the crt-royale preset as not netplay-safe', () => {
    const p = getPreset('crt-royale');
    expect(p).not.toBeNull();
    expect(p!.netplaySafe).toBe(false);
  });

  it('returns null for unknown preset id', () => {
    expect(getPreset('ultra-hd-4k-remaster')).toBeNull();
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// retroarch-presets — getPresetOnlineWarning
// ─────────────────────────────────────────────────────────────────────────────

describe('getPresetOnlineWarning()', () => {
  it('returns null for a netplay-safe preset', () => {
    expect(getPresetOnlineWarning('clean')).toBeNull();
    expect(getPresetOnlineWarning('performance')).toBeNull();
  });

  it('returns a warning string for run-ahead preset', () => {
    const warning = getPresetOnlineWarning('runahead-1');
    expect(warning).toBeTruthy();
  });

  it('returns null for an unknown preset id', () => {
    expect(getPresetOnlineWarning('does-not-exist')).toBeNull();
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// retroarch-presets — applyPreset
// ─────────────────────────────────────────────────────────────────────────────

describe('applyPreset()', () => {
  it('returns a merged config with preset settings taking precedence', () => {
    const base = { video_smooth: 'true', my_custom_key: 'hello' };
    const merged = applyPreset('clean', base);
    expect(merged['video_smooth']).toBe('false'); // preset overrides base
    expect(merged['my_custom_key']).toBe('hello'); // base key preserved
  });

  it('returns a copy of base when presetId is unknown', () => {
    const base = { some_key: 'some_value' };
    const merged = applyPreset('does-not-exist', base);
    expect(merged).toEqual(base);
  });

  it('works with no base config', () => {
    const merged = applyPreset('performance');
    expect(typeof merged).toBe('object');
    expect(merged['video_scale_integer']).toBe('true');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// launch-preflight — BIOS check integration
// ─────────────────────────────────────────────────────────────────────────────

describe('runLaunchPreflight — BIOS check', () => {
  it('blocks PSX launch when biosPath is not provided', () => {
    const result = runLaunchPreflight({
      system: 'psx',
      mode: 'local',
      romPath: '/roms/crash.bin',
      emulatorPath: '/usr/bin/retroarch',
      biosPath: null,
    });
    expect(result.canLaunch).toBe(false);
    expect(result.issues.some((c) => c.id === 'bios-missing')).toBe(true);
  });

  it('passes PSX launch when biosPath is provided', () => {
    const result = runLaunchPreflight({
      system: 'psx',
      mode: 'local',
      romPath: '/roms/crash.bin',
      emulatorPath: '/usr/bin/retroarch',
      biosPath: '/bios/scph1001.bin',
    });
    expect(result.canLaunch).toBe(true);
    expect(result.issues.some((c) => c.id === 'bios-missing')).toBe(false);
  });

  it('does not check BIOS for nes (no BIOS required)', () => {
    const result = runLaunchPreflight({
      system: 'nes',
      mode: 'local',
      romPath: '/roms/mario.nes',
      emulatorPath: '/usr/bin/retroarch',
      // biosPath intentionally omitted
    });
    expect(result.canLaunch).toBe(true);
    expect(result.issues.some((c) => c.id === 'bios-missing')).toBe(false);
  });

  it('blocks NDS launch when biosPath is missing', () => {
    const result = runLaunchPreflight({
      system: 'nds',
      mode: 'local',
      romPath: '/roms/mario-kart-ds.nds',
      emulatorPath: '/usr/bin/melonDS',
      biosPath: undefined,
    });
    expect(result.canLaunch).toBe(false);
    const biosIssue = result.issues.find((c) => c.id === 'bios-missing');
    expect(biosIssue).toBeDefined();
    expect(biosIssue!.message).toContain('NDS');
  });

  it('passes Dreamcast launch when biosPath is provided', () => {
    const result = runLaunchPreflight({
      system: 'dreamcast',
      mode: 'local',
      romPath: '/roms/sonic-adventure.cdi',
      emulatorPath: '/usr/bin/flycast',
      biosPath: '/bios/dc_boot.bin',
    });
    expect(result.canLaunch).toBe(true);
  });

  it('GBA with no biosPath still launches (BIOS is optional)', () => {
    const result = runLaunchPreflight({
      system: 'gba',
      mode: 'local',
      romPath: '/roms/mario-kart.gba',
      emulatorPath: '/usr/bin/retroarch',
      // biosPath not set
    });
    expect(result.canLaunch).toBe(true);
  });
});
