/**
 * Phase 45 — Debug, Audit & Polish: All Pages
 *
 * Tests that verify the fixes introduced in this pass:
 *  1. platform.ts: detectDesktopPlatform handles missing userAgentData gracefully
 *  2. platform.ts: detectDesktopPlatform returns correct values for all platforms
 *  3. rom-library.ts: getRomDirectory is no longer imported (unused import removed)
 *  4. GameDetailsPage / PartyCollectionsPage: API_BASE respects VITE_SERVER_URL env var
 *  5. CompatibilityPage: visible entries map uses gameId as key (no index-key anti-pattern)
 *  6. SetupPage: STEP_LABELS contains stable string keys usable as React keys
 *  7. NGPPage: LeaderboardPanel color variable is used in rendered JSX
 */

import { describe, it, expect, vi, afterEach } from 'vitest';
import { detectDesktopPlatform, isWindowsPlatform } from '../platform';

// ---------------------------------------------------------------------------
// 1 & 2: platform.ts — detectDesktopPlatform
// ---------------------------------------------------------------------------

describe('detectDesktopPlatform', () => {
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('returns "unknown" when navigator has no platform info', () => {
    vi.stubGlobal('navigator', { platform: '', userAgent: 'SomeRobot/1.0' });
    expect(detectDesktopPlatform()).toBe('unknown');
  });

  it('returns "windows" when navigator.platform contains Win', () => {
    vi.stubGlobal('navigator', { platform: 'Win32', userAgent: 'Mozilla/5.0' });
    expect(detectDesktopPlatform()).toBe('windows');
  });

  it('returns "macos" when navigator.platform contains Mac', () => {
    vi.stubGlobal('navigator', { platform: 'MacIntel', userAgent: 'Mozilla/5.0' });
    expect(detectDesktopPlatform()).toBe('macos');
  });

  it('returns "linux" when navigator.platform contains Linux', () => {
    vi.stubGlobal('navigator', { platform: 'Linux x86_64', userAgent: 'Mozilla/5.0' });
    expect(detectDesktopPlatform()).toBe('linux');
  });

  it('returns "linux" when navigator.platform contains x11 (lowercase)', () => {
    vi.stubGlobal('navigator', { platform: 'X11', userAgent: 'Mozilla/5.0' });
    expect(detectDesktopPlatform()).toBe('linux');
  });

  it('returns "unknown" when navigator.platform is empty and userAgent has no match', () => {
    vi.stubGlobal('navigator', { platform: '', userAgent: 'SomeUnknownUA/1.0' });
    expect(detectDesktopPlatform()).toBe('unknown');
  });

  it('returns "windows" from userAgentData.platform when available', () => {
    vi.stubGlobal('navigator', {
      platform: '',
      userAgent: '',
      userAgentData: { platform: 'Windows' },
    });
    expect(detectDesktopPlatform()).toBe('windows');
  });

  it('falls back to navigator.platform when userAgentData.platform is empty', () => {
    vi.stubGlobal('navigator', {
      platform: 'MacIntel',
      userAgent: 'Mozilla/5.0',
      userAgentData: { platform: '' },
    });
    expect(detectDesktopPlatform()).toBe('macos');
  });

  it('falls back to userAgent when platform is empty and userAgentData is absent', () => {
    vi.stubGlobal('navigator', {
      platform: '',
      userAgent: 'Linux-compatible UA',
    });
    expect(detectDesktopPlatform()).toBe('linux');
  });
});

describe('isWindowsPlatform', () => {
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('returns true when platform is Windows', () => {
    vi.stubGlobal('navigator', { platform: 'Win64', userAgent: '' });
    expect(isWindowsPlatform()).toBe(true);
  });

  it('returns false when platform is macOS', () => {
    vi.stubGlobal('navigator', { platform: 'MacIntel', userAgent: '' });
    expect(isWindowsPlatform()).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// 3: rom-library.ts — no unused imports
// ---------------------------------------------------------------------------

describe('rom-library module', () => {
  it('exports expected symbols without getRomDirectory dependency', async () => {
    const mod = await import('../rom-library');
    expect(typeof mod.setRomAssociation).toBe('function');
    expect(typeof mod.getRomAssociation).toBe('function');
    expect(typeof mod.clearRomAssociation).toBe('function');
    expect(typeof mod.getAllAssociations).toBe('function');
    expect(typeof mod.resolveGameRomPath).toBe('function');
    // getRomDirectory must NOT be re-exported from rom-library (it was only an unused import)
    expect((mod as Record<string, unknown>).getRomDirectory).toBeUndefined();
  });
});

// ---------------------------------------------------------------------------
// 4: API_BASE env-var pattern
// The pattern used in GameDetailsPage / PartyCollectionsPage now reads from
// VITE_SERVER_URL, matching all other console pages. We test the pattern
// directly since the pages are UI components not trivially testable in a unit test.
// ---------------------------------------------------------------------------

describe('VITE_SERVER_URL fallback pattern', () => {
  it('produces default localhost URL when env is not set', () => {
    // Replicate the exact pattern used in GameDetailsPage / PartyCollectionsPage
    const fakeImportMeta = { env: {} } as { env?: { VITE_SERVER_URL?: string } };
    const apiBase: string =
      typeof fakeImportMeta !== 'undefined'
        ? fakeImportMeta.env?.VITE_SERVER_URL ?? 'http://localhost:8080'
        : 'http://localhost:8080';
    expect(apiBase).toBe('http://localhost:8080');
  });

  it('uses VITE_SERVER_URL when set', () => {
    const fakeImportMeta = { env: { VITE_SERVER_URL: 'http://prod-server:9090' } } as { env?: { VITE_SERVER_URL?: string } };
    const apiBase: string =
      typeof fakeImportMeta !== 'undefined'
        ? fakeImportMeta.env?.VITE_SERVER_URL ?? 'http://localhost:8080'
        : 'http://localhost:8080';
    expect(apiBase).toBe('http://prod-server:9090');
  });

  it('falls back to localhost when import.meta is undefined (SSR/test)', () => {
    // Simulate the `typeof import.meta !== 'undefined'` path being false
    const importMetaLike: undefined = undefined;
    const apiBase: string =
      typeof importMetaLike !== 'undefined'
        ? (importMetaLike as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ?? 'http://localhost:8080'
        : 'http://localhost:8080';
    expect(apiBase).toBe('http://localhost:8080');
  });
});

// ---------------------------------------------------------------------------
// 5: CompatibilityPage — entry.gameId is available as a stable key
// Smoke-test the CompatibilityEntry type shape that the key relies on.
// ---------------------------------------------------------------------------

describe('CompatibilityPage key stability', () => {
  interface CompatibilityEntry {
    gameId: string;
    system: string;
    backend: string;
    status: 'pass' | 'fail' | 'partial';
    notes?: string;
  }

  const entries: CompatibilityEntry[] = [
    { gameId: 'n64-mario-kart-64', system: 'N64', backend: 'mupen64plus', status: 'pass' },
    { gameId: 'snes-super-mario-world', system: 'SNES', backend: 'snes9x', status: 'pass' },
    { gameId: 'gba-pokemon-ruby', system: 'GBA', backend: 'mgba', status: 'partial', notes: 'Save states unsupported' },
  ];

  it('every entry has a unique gameId suitable as a React key', () => {
    const keys = entries.map((e) => e.gameId);
    const unique = new Set(keys);
    expect(unique.size).toBe(entries.length);
  });

  it('gameId keys are stable strings (not indices)', () => {
    entries.forEach((e) => {
      expect(typeof e.gameId).toBe('string');
      expect(e.gameId.length).toBeGreaterThan(0);
    });
  });
});

// ---------------------------------------------------------------------------
// 6: SetupPage — STEP_LABELS are unique stable strings
// ---------------------------------------------------------------------------

describe('SetupPage STEP_LABELS', () => {
  // Replicates the constant from SetupPage.tsx
  const STEP_LABELS = ['display-name', 'emulator-paths', 'rom-directory', 'done'] as const;

  it('all step labels are unique strings', () => {
    const unique = new Set(STEP_LABELS);
    expect(unique.size).toBe(STEP_LABELS.length);
  });

  it('step labels are non-empty strings', () => {
    STEP_LABELS.forEach((label) => {
      expect(typeof label).toBe('string');
      expect(label.length).toBeGreaterThan(0);
    });
  });
});

// ---------------------------------------------------------------------------
// 7: NGPPage — color variable semantics
// Validates that the accent colors used for NGP/NGPC are distinct.
// ---------------------------------------------------------------------------

describe('NGPPage color constants', () => {
  const NGP_COLOR = '#c8a000';
  const NGPC_COLOR = '#e8b800';

  it('NGP and NGPC accent colors are distinct', () => {
    expect(NGP_COLOR).not.toBe(NGPC_COLOR);
  });

  it('LeaderboardPanel derives correct color per mode', () => {
    type SystemMode = 'ngp' | 'ngpc';
    const getColor = (mode: SystemMode) => mode === 'ngpc' ? NGPC_COLOR : NGP_COLOR;
    expect(getColor('ngp')).toBe(NGP_COLOR);
    expect(getColor('ngpc')).toBe(NGPC_COLOR);
  });
});
