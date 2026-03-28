/**
 * Phase 33 — Sega Master System (SMS) support
 *
 * Tests for:
 *  - sms system type in SYSTEM_INFO (name, shortName, generation, maxLocalPlayers, color)
 *  - RetroArch backend now includes sms in systems list
 *  - ROM scanner recognises .sms, .gg, and .sg extensions
 *  - System adapter for sms (RetroArch + Genesis Plus GX path)
 *  - Session templates (default + 8 SMS game templates)
 *  - Mock games (8 SMS games with correct system/color)
 *  - No duplicate IDs introduced by Phase 33
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import { SYSTEM_INFO } from '../../../../packages/shared-types/src/systems';
import { ROM_EXTENSIONS } from '../../../../packages/emulator-bridge/src/scanner';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';

// ---------------------------------------------------------------------------
// SMS system type
// ---------------------------------------------------------------------------

describe('SMS system type (Phase 33)', () => {
  it('includes sms in SYSTEM_INFO', () => {
    expect(SYSTEM_INFO['sms']).toBeDefined();
  });

  it('SMS name is "Sega Master System"', () => {
    expect(SYSTEM_INFO['sms'].name).toBe('Sega Master System');
  });

  it('SMS shortName is "SMS"', () => {
    expect(SYSTEM_INFO['sms'].shortName).toBe('SMS');
  });

  it('SMS is generation 3', () => {
    expect(SYSTEM_INFO['sms'].generation).toBe(3);
  });

  it('SMS supports up to 2 local players', () => {
    expect(SYSTEM_INFO['sms'].maxLocalPlayers).toBe(2);
  });

  it('SMS does not support link cable', () => {
    expect(SYSTEM_INFO['sms'].supportsLink).toBe(false);
  });

  it('SMS color is deep SEGA blue', () => {
    expect(SYSTEM_INFO['sms'].color).toBe('#003399');
  });
});

// ---------------------------------------------------------------------------
// RetroArch backend includes sms
// ---------------------------------------------------------------------------

describe('RetroArch backend — SMS support (Phase 33)', () => {
  it('RetroArch systems list includes sms', () => {
    const retroarch = KNOWN_BACKENDS.find((b) => b.id === 'retroarch');
    expect(retroarch?.systems).toContain('sms');
  });

  it('RetroArch recommended cores hint mentions genesis_plus_gx for Master System', () => {
    const retroarch = KNOWN_BACKENDS.find((b) => b.id === 'retroarch');
    const hint = retroarch?.notes?.find((h) => h.includes('genesis_plus_gx'));
    expect(hint).toBeDefined();
    expect(hint).toContain('Master System');
  });
});

// ---------------------------------------------------------------------------
// ROM scanner — SMS extensions
// ---------------------------------------------------------------------------

describe('ROM scanner — SMS extensions (Phase 33)', () => {
  it('.sms extension maps to sms system', () => {
    expect(ROM_EXTENSIONS['.sms']).toBe('sms');
  });

  it('.gg extension maps to sms system', () => {
    expect(ROM_EXTENSIONS['.gg']).toBe('sms');
  });

  it('.sg extension maps to sms system', () => {
    expect(ROM_EXTENSIONS['.sg']).toBe('sms');
  });
});

// ---------------------------------------------------------------------------
// System adapter — SMS
// ---------------------------------------------------------------------------

describe('createSystemAdapter — SMS (Phase 33)', () => {
  it('returns an adapter for sms', () => {
    const adapter = createSystemAdapter('sms');
    expect(adapter).not.toBeNull();
  });

  it('SMS adapter preferredBackendId is retroarch', () => {
    const adapter = createSystemAdapter('sms')!;
    expect(adapter.preferredBackendId).toBe('retroarch');
  });

  it('SMS adapter has no fallback backends', () => {
    const adapter = createSystemAdapter('sms')!;
    expect(adapter.fallbackBackendIds).toHaveLength(0);
  });

  it('SMS adapter buildLaunchArgs returns a non-empty array for a rom path', () => {
    const adapter = createSystemAdapter('sms')!;
    const args = adapter.buildLaunchArgs('/roms/sms/sonic.sms', {});
    expect(Array.isArray(args)).toBe(true);
    expect(args.length).toBeGreaterThan(0);
  });

  it('SMS adapter getSavePath includes sms prefix', () => {
    const adapter = createSystemAdapter('sms')!;
    const savePath = adapter.getSavePath('sms-sonic', '/saves');
    expect(savePath).toContain('sms');
  });
});

// ---------------------------------------------------------------------------
// Session templates — SMS
// ---------------------------------------------------------------------------

describe('SMS session templates (Phase 33)', () => {
  const store = new SessionTemplateStore();

  const SMS_EXPECTED_TEMPLATES = [
    'sms-default-2p',
    'sms-alex-kidd-in-miracle-world-1p',
    'sms-sonic-the-hedgehog-1p',
    'sms-wonder-boy-iii-1p',
    'sms-phantasy-star-1p',
    'sms-golden-axe-warrior-1p',
    'sms-r-type-1p',
    'sms-shinobi-1p',
    'sms-after-burner-1p',
  ];

  it('stores the correct number of SMS templates', () => {
    const allTemplates = store.listAll();
    const smsTemplates = allTemplates.filter((t) => t.system === 'sms');
    expect(smsTemplates.length).toBe(SMS_EXPECTED_TEMPLATES.length);
  });

  for (const tplId of SMS_EXPECTED_TEMPLATES) {
    it(`template "${tplId}" exists`, () => {
      const tpl = store.get(tplId);
      expect(tpl).not.toBeNull();
    });

    it(`template "${tplId}" uses retroarch backend`, () => {
      const tpl = store.get(tplId)!;
      expect(tpl.emulatorBackendId).toBe('retroarch');
    });

    it(`template "${tplId}" uses online-relay netplay`, () => {
      const tpl = store.get(tplId)!;
      expect(tpl.netplayMode).toBe('online-relay');
    });

    it(`template "${tplId}" system is sms`, () => {
      const tpl = store.get(tplId)!;
      expect(tpl.system).toBe('sms');
    });
  }

  it('getForGame returns default sms template for unknown sms game', () => {
    const tpl = store.getForGame('sms-unknown-game', 'sms');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('sms');
  });
});

// ---------------------------------------------------------------------------
// Mock games — SMS
// ---------------------------------------------------------------------------

describe('SMS mock games (Phase 33)', () => {
  const smsGames = MOCK_GAMES.filter((g) => g.system === 'SMS');

  it('contains 8 SMS mock games', () => {
    expect(smsGames.length).toBe(8);
  });

  it('all SMS games have system set to "SMS"', () => {
    for (const g of smsGames) {
      expect(g.system).toBe('SMS');
    }
  });

  it('all SMS games have the correct systemColor', () => {
    for (const g of smsGames) {
      expect(g.systemColor).toBe('#003399');
    }
  });

  const expectedIds = [
    'sms-alex-kidd-in-miracle-world',
    'sms-sonic-the-hedgehog',
    'sms-wonder-boy-iii',
    'sms-phantasy-star',
    'sms-golden-axe-warrior',
    'sms-r-type',
    'sms-shinobi',
    'sms-after-burner',
  ];

  for (const id of expectedIds) {
    it(`mock game "${id}" exists`, () => {
      const game = MOCK_GAMES.find((g) => g.id === id);
      expect(game).toBeDefined();
    });
  }
});

// ---------------------------------------------------------------------------
// Duplicate ID check
// ---------------------------------------------------------------------------

describe('No duplicate IDs introduced by Phase 33', () => {
  it('all MOCK_GAMES IDs are unique', () => {
    const ids = MOCK_GAMES.map((g) => g.id);
    const unique = new Set(ids);
    expect(unique.size).toBe(ids.length);
  });

  it('all session template IDs are unique', () => {
    const store = new SessionTemplateStore();
    const ids = store.listAll().map((t) => t.id);
    const unique = new Set(ids);
    expect(unique.size).toBe(ids.length);
  });
});
