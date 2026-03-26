/**
 * Phase 41 — PSX Overhaul
 *
 * Tests for:
 *  - Expanded PSX mock game catalog (15 games total)
 *  - New PSX session templates (16 total)
 *  - Extended PSX netplay whitelist (15 PSX entries)
 *  - Expanded PSX game metadata (13 entries)
 *  - PSX adapter new flags: pgxpEnabled, cdRomFastBoot, multitapEnabled, analogControllerEnabled
 *  - mednafen-psx in PSX fallback backends
 *  - mednafen-psx backend registered in KNOWN_BACKENDS
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';
import { GameMetadataStore } from '../game-metadata-store';
import { NETPLAY_WHITELIST, approvedGamesForSystem } from '../../../../apps/desktop/src/lib/netplay-whitelist';

// ─────────────────────────────────────────────────────────────────────────────
// Expanded mock game catalog
// ─────────────────────────────────────────────────────────────────────────────

describe('PSX mock game catalog — overhaul', () => {
  const psxGames = MOCK_GAMES.filter((g) => g.system === 'PSX');

  it('has at least 15 PSX games', () => {
    expect(psxGames.length).toBeGreaterThanOrEqual(15);
  });

  it('includes Crash Team Racing', () => {
    expect(psxGames.find((g) => g.id === 'psx-crash-team-racing')).toBeDefined();
  });

  it('includes Final Fantasy VII', () => {
    expect(psxGames.find((g) => g.id === 'psx-final-fantasy-vii')).toBeDefined();
  });

  it('includes Resident Evil 2', () => {
    expect(psxGames.find((g) => g.id === 'psx-resident-evil-2')).toBeDefined();
  });

  it('includes Spyro 2', () => {
    expect(psxGames.find((g) => g.id === 'psx-spyro-2')).toBeDefined();
  });

  it('includes Gran Turismo 2', () => {
    expect(psxGames.find((g) => g.id === 'psx-gran-turismo-2')).toBeDefined();
  });

  it('includes Diablo', () => {
    expect(psxGames.find((g) => g.id === 'psx-diablo')).toBeDefined();
  });

  it('Crash Team Racing is 4-player max', () => {
    const game = psxGames.find((g) => g.id === 'psx-crash-team-racing');
    expect(game?.maxPlayers).toBe(4);
  });

  it('Gran Turismo 2 is 2-player max', () => {
    const game = psxGames.find((g) => g.id === 'psx-gran-turismo-2');
    expect(game?.maxPlayers).toBe(2);
  });

  it('single-player games have maxPlayers 1', () => {
    const singlePlayerIds = [
      'psx-final-fantasy-vii',
      'psx-resident-evil-2',
      'psx-spyro-2',
      'psx-diablo',
      'psx-crash-bandicoot',
      'psx-castlevania-sotn',
      'psx-metal-gear-solid',
    ];
    for (const id of singlePlayerIds) {
      const game = psxGames.find((g) => g.id === id);
      expect(game?.maxPlayers, `${id} should be 1-player`).toBe(1);
    }
  });

  it('all PSX games use system color #808080', () => {
    for (const game of psxGames) {
      expect(game.systemColor).toBe('#808080');
    }
  });

  it('has no duplicate PSX game IDs', () => {
    const ids = psxGames.map((g) => g.id);
    expect(ids.length).toBe(new Set(ids).size);
  });

  it('all PSX games have a coverEmoji', () => {
    for (const game of psxGames) {
      expect(game.coverEmoji).toBeTruthy();
    }
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Session templates
// ─────────────────────────────────────────────────────────────────────────────

describe('PSX session templates — overhaul', () => {
  const store = new SessionTemplateStore();
  const psxTemplates = store.listAll().filter((t) => t.system === 'psx');

  it('has at least 16 PSX session templates', () => {
    expect(psxTemplates.length).toBeGreaterThanOrEqual(16);
  });

  it('psx-crash-team-racing-4p template exists with 4 players', () => {
    const t = store.get('psx-crash-team-racing-4p');
    expect(t).toBeDefined();
    expect(t?.playerCount).toBe(4);
  });

  it('psx-crash-team-racing-4p has latencyTarget 80', () => {
    const t = store.get('psx-crash-team-racing-4p');
    expect(t?.latencyTarget).toBe(80);
  });

  it('psx-final-fantasy-vii-1p template exists', () => {
    const t = store.get('psx-final-fantasy-vii-1p');
    expect(t).toBeDefined();
    expect(t?.playerCount).toBe(1);
  });

  it('psx-final-fantasy-vii-1p has latencyTarget 150', () => {
    const t = store.get('psx-final-fantasy-vii-1p');
    expect(t?.latencyTarget).toBe(150);
  });

  it('psx-resident-evil-2-1p template exists', () => {
    expect(store.get('psx-resident-evil-2-1p')).toBeDefined();
  });

  it('psx-spyro-2-1p template exists', () => {
    expect(store.get('psx-spyro-2-1p')).toBeDefined();
  });

  it('psx-gran-turismo-2-2p template exists with 2 players', () => {
    const t = store.get('psx-gran-turismo-2-2p');
    expect(t).toBeDefined();
    expect(t?.playerCount).toBe(2);
  });

  it('psx-gran-turismo-2-2p has latencyTarget 80', () => {
    const t = store.get('psx-gran-turismo-2-2p');
    expect(t?.latencyTarget).toBe(80);
  });

  it('psx-diablo-1p template exists', () => {
    expect(store.get('psx-diablo-1p')).toBeDefined();
  });

  it('all new PSX templates use duckstation backend', () => {
    const newIds = [
      'psx-crash-team-racing-4p',
      'psx-final-fantasy-vii-1p',
      'psx-resident-evil-2-1p',
      'psx-spyro-2-1p',
      'psx-gran-turismo-2-2p',
      'psx-diablo-1p',
    ];
    for (const id of newIds) {
      expect(store.get(id)?.emulatorBackendId).toBe('duckstation');
    }
  });

  it('all new PSX templates use online-relay netplay mode', () => {
    const newIds = [
      'psx-crash-team-racing-4p',
      'psx-final-fantasy-vii-1p',
      'psx-resident-evil-2-1p',
      'psx-spyro-2-1p',
      'psx-gran-turismo-2-2p',
      'psx-diablo-1p',
    ];
    for (const id of newIds) {
      expect(store.get(id)?.netplayMode).toBe('online-relay');
    }
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Netplay whitelist
// ─────────────────────────────────────────────────────────────────────────────

describe('PSX netplay whitelist — overhaul', () => {
  const psxEntries = NETPLAY_WHITELIST.filter((e) => e.system === 'psx');

  it('has at least 15 PSX whitelist entries', () => {
    expect(psxEntries.length).toBeGreaterThanOrEqual(15);
  });

  it('psx-crash-bandicoot is approved', () => {
    const entry = psxEntries.find((e) => e.gameId === 'psx-crash-bandicoot');
    expect(entry?.rating).toBe('approved');
  });

  it('psx-tony-hawks-pro-skater-2 is approved', () => {
    const entry = psxEntries.find((e) => e.gameId === 'psx-tony-hawks-pro-skater-2');
    expect(entry?.rating).toBe('approved');
  });

  it('psx-crash-bash is approved with 4 max players', () => {
    const entry = psxEntries.find((e) => e.gameId === 'psx-crash-bash');
    expect(entry?.rating).toBe('approved');
    expect(entry?.recommendedMaxPlayers).toBe(4);
  });

  it('psx-worms-armageddon is approved with 4 max players', () => {
    const entry = psxEntries.find((e) => e.gameId === 'psx-worms-armageddon');
    expect(entry?.rating).toBe('approved');
    expect(entry?.recommendedMaxPlayers).toBe(4);
  });

  it('psx-twisted-metal-2 is caution', () => {
    const entry = psxEntries.find((e) => e.gameId === 'psx-twisted-metal-2');
    expect(entry?.rating).toBe('caution');
  });

  it('psx-gran-turismo-2 is caution', () => {
    const entry = psxEntries.find((e) => e.gameId === 'psx-gran-turismo-2');
    expect(entry?.rating).toBe('caution');
  });

  it('psx-diablo is caution', () => {
    const entry = psxEntries.find((e) => e.gameId === 'psx-diablo');
    expect(entry?.rating).toBe('caution');
  });

  it('psx-crash-team-racing is approved with 4 max players', () => {
    const entry = psxEntries.find((e) => e.gameId === 'psx-crash-team-racing');
    expect(entry?.rating).toBe('approved');
    expect(entry?.recommendedMaxPlayers).toBe(4);
  });

  it('psx-tekken-3 is approved', () => {
    const entry = psxEntries.find((e) => e.gameId === 'psx-tekken-3');
    expect(entry?.rating).toBe('approved');
  });

  it('psx-street-fighter-alpha-3 is approved', () => {
    const entry = psxEntries.find((e) => e.gameId === 'psx-street-fighter-alpha-3');
    expect(entry?.rating).toBe('approved');
  });

  it('approvedGamesForSystem returns ≥ 6 approved PSX games', () => {
    const approved = approvedGamesForSystem('psx');
    expect(approved.length).toBeGreaterThanOrEqual(6);
  });

  it('all PSX entries have a reason string', () => {
    for (const entry of psxEntries) {
      expect(typeof entry.reason).toBe('string');
      expect(entry.reason.length).toBeGreaterThan(0);
    }
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Game metadata store
// ─────────────────────────────────────────────────────────────────────────────

describe('PSX game metadata — overhaul', () => {
  const store = new GameMetadataStore();

  it('has metadata for psx-tekken-3', () => {
    const meta = store.get('psx-tekken-3');
    expect(meta).toBeDefined();
    expect(meta?.genre).toContain('Fighting');
  });

  it('has metadata for psx-street-fighter-alpha-3', () => {
    const meta = store.get('psx-street-fighter-alpha-3');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Capcom');
  });

  it('has metadata for psx-tony-hawks-pro-skater-2', () => {
    const meta = store.get('psx-tony-hawks-pro-skater-2');
    expect(meta).toBeDefined();
    expect(meta?.year).toBe(2000);
  });

  it('has metadata for psx-twisted-metal-2', () => {
    const meta = store.get('psx-twisted-metal-2');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('SingleTrac');
  });

  it('has metadata for psx-crash-bash', () => {
    const meta = store.get('psx-crash-bash');
    expect(meta).toBeDefined();
    expect(meta?.genre).toContain('Party');
  });

  it('has metadata for psx-worms-armageddon', () => {
    const meta = store.get('psx-worms-armageddon');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Team17');
  });

  it('has metadata for psx-crash-bandicoot', () => {
    const meta = store.get('psx-crash-bandicoot');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Naughty Dog');
  });

  it('has metadata for psx-metal-gear-solid', () => {
    const meta = store.get('psx-metal-gear-solid');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Konami');
    expect(meta?.year).toBe(1998);
  });

  it('has metadata for psx-final-fantasy-vii', () => {
    const meta = store.get('psx-final-fantasy-vii');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Square');
    expect(meta?.year).toBe(1997);
  });

  it('has metadata for psx-gran-turismo-2', () => {
    const meta = store.get('psx-gran-turismo-2');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Polyphony Digital');
    expect(meta?.genre).toContain('Racing');
  });

  it('has metadata for psx-diablo', () => {
    const meta = store.get('psx-diablo');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Blizzard Entertainment');
  });

  it('has metadata for psx-crash-team-racing', () => {
    const meta = store.get('psx-crash-team-racing');
    expect(meta).toBeDefined();
    expect(meta?.developer).toBe('Naughty Dog');
  });

  it('has metadata for psx-castlevania-symphony-of-the-night', () => {
    const meta = store.get('psx-castlevania-symphony-of-the-night');
    expect(meta).toBeDefined();
    expect(meta?.genre).toContain('Action RPG');
  });

  it('all new PSX metadata entries have netplayTips', () => {
    const newIds = [
      'psx-tekken-3',
      'psx-street-fighter-alpha-3',
      'psx-tony-hawks-pro-skater-2',
      'psx-twisted-metal-2',
      'psx-crash-bash',
      'psx-worms-armageddon',
      'psx-crash-bandicoot',
      'psx-metal-gear-solid',
      'psx-final-fantasy-vii',
      'psx-gran-turismo-2',
      'psx-diablo',
    ];
    for (const id of newIds) {
      const meta = store.get(id);
      expect(meta?.netplayTips?.length, `${id} should have netplayTips`).toBeGreaterThan(0);
    }
  });

  it('all new PSX metadata entries have onboardingTips', () => {
    const newIds = [
      'psx-tekken-3',
      'psx-street-fighter-alpha-3',
      'psx-tony-hawks-pro-skater-2',
      'psx-twisted-metal-2',
      'psx-crash-bash',
      'psx-worms-armageddon',
      'psx-crash-bandicoot',
      'psx-metal-gear-solid',
      'psx-final-fantasy-vii',
      'psx-gran-turismo-2',
      'psx-diablo',
    ];
    for (const id of newIds) {
      const meta = store.get(id);
      expect(meta?.onboardingTips?.length, `${id} should have onboardingTips`).toBeGreaterThan(0);
    }
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// PSX adapter — new flags
// ─────────────────────────────────────────────────────────────────────────────

describe('PSX adapter — new PSX-specific flags', () => {
  const adapter = createSystemAdapter('psx');

  it('mednafen-psx is in PSX fallback backends', () => {
    expect(adapter.fallbackBackendIds).toContain('mednafen-psx');
  });

  it('retroarch is still in PSX fallback backends', () => {
    expect(adapter.fallbackBackendIds).toContain('retroarch');
  });

  it('pgxpEnabled adds --pgxp flag', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/tekken3.bin', { pgxpEnabled: true });
    expect(args).toContain('--pgxp');
  });

  it('pgxpEnabled false does not add --pgxp', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/tekken3.bin', { pgxpEnabled: false });
    expect(args).not.toContain('--pgxp');
  });

  it('cdRomFastBoot adds --fast-boot flag', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/crash.bin', { cdRomFastBoot: true });
    expect(args).toContain('--fast-boot');
  });

  it('cdRomFastBoot false does not add --fast-boot', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/crash.bin', { cdRomFastBoot: false });
    expect(args).not.toContain('--fast-boot');
  });

  it('multitapEnabled adds --multitap flag', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/crash-bash.bin', { multitapEnabled: true });
    expect(args).toContain('--multitap');
  });

  it('multitapEnabled false does not add --multitap', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/crash-bash.bin', { multitapEnabled: false });
    expect(args).not.toContain('--multitap');
  });

  it('analogControllerEnabled adds --analog flag', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/gran-turismo-2.bin', { analogControllerEnabled: true });
    expect(args).toContain('--analog');
  });

  it('analogControllerEnabled false does not add --analog', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/gran-turismo-2.bin', { analogControllerEnabled: false });
    expect(args).not.toContain('--analog');
  });

  it('omitting PSX flags does NOT add their corresponding switches', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/ff7.bin', {});
    expect(args).not.toContain('--pgxp');
    expect(args).not.toContain('--fast-boot');
    expect(args).not.toContain('--multitap');
    expect(args).not.toContain('--analog');
  });

  it('all new PSX flags can be combined in one call', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/ctr.bin', {
      pgxpEnabled: true,
      cdRomFastBoot: true,
      multitapEnabled: true,
      analogControllerEnabled: true,
      fullscreen: true,
    });
    expect(args).toContain('--pgxp');
    expect(args).toContain('--fast-boot');
    expect(args).toContain('--multitap');
    expect(args).toContain('--analog');
    expect(args).toContain('--fullscreen');
  });

  it('mednafen-psx backend uses RetroArch args (does not contain raw rom as second arg)', () => {
    const mednafenAdapter = createSystemAdapter('psx', 'mednafen-psx');
    const args = mednafenAdapter.buildLaunchArgs('/roms/psx/ctr.bin', {});
    // RetroArch-style: rom path should still appear but no DuckStation-only flags
    expect(args).toContain('/roms/psx/ctr.bin');
    expect(args).not.toContain('--pgxp');
  });

  it('retroarch backend does NOT add DuckStation-specific flags', () => {
    const raAdapter = createSystemAdapter('psx', 'retroarch');
    const args = raAdapter.buildLaunchArgs('/roms/psx/tekken3.bin', { pgxpEnabled: true });
    // pgxpEnabled is silently ignored for non-duckstation backends
    expect(args).not.toContain('--pgxp');
  });

  it('duckstation backend includes ROM path as first arg', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/ff7.bin', {});
    expect(args[0]).toBe('/roms/psx/ff7.bin');
  });

  it('getSavePath returns correct PSX save path', () => {
    const path = adapter.getSavePath('psx-tekken-3', '/saves');
    expect(path).toBe('/saves/psx/psx-tekken-3');
  });

  it('fullscreen flag still works alongside new PSX flags', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/gt2.bin', {
      fullscreen: true,
      analogControllerEnabled: true,
    });
    expect(args).toContain('--fullscreen');
    expect(args).toContain('--analog');
  });

  it('debug flag still works alongside new PSX flags', () => {
    const args = adapter.buildLaunchArgs('/roms/psx/mgs.bin', {
      debug: true,
      pgxpEnabled: true,
    });
    expect(args).toContain('--verbose');
    expect(args).toContain('--pgxp');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// mednafen-psx backend registration
// ─────────────────────────────────────────────────────────────────────────────

describe('mednafen-psx backend registration', () => {
  const backend = KNOWN_BACKENDS.find((b) => b.id === 'mednafen-psx');

  it('mednafen-psx backend exists in KNOWN_BACKENDS', () => {
    expect(backend).toBeDefined();
  });

  it('mednafen-psx supports PSX system', () => {
    expect(backend?.systems).toContain('psx');
  });

  it('mednafen-psx has netplay support', () => {
    expect(backend?.supportsNetplay).toBe(true);
  });

  it('mednafen-psx uses retroarch executable', () => {
    expect(backend?.executableName).toBe('retroarch');
  });

  it('mednafen-psx supports save states', () => {
    expect(backend?.supportsSaveStates).toBe(true);
  });

  it('mednafen-psx has local launch capability', () => {
    expect(backend?.capabilities.localLaunch).toBe(true);
  });

  it('mednafen-psx has at least one note about Beetle PSX HW', () => {
    const notes = backend?.notes ?? [];
    expect(notes.some((n) => n.toLowerCase().includes('beetle'))).toBe(true);
  });

  it('mednafen-psx has a website', () => {
    expect(backend?.website).toBeTruthy();
  });

  it('duckstation backend still exists and supports PSX', () => {
    const ds = KNOWN_BACKENDS.find((b) => b.id === 'duckstation');
    expect(ds).toBeDefined();
    expect(ds?.systems).toContain('psx');
  });

  it('duckstation still has no native netplay', () => {
    const ds = KNOWN_BACKENDS.find((b) => b.id === 'duckstation');
    expect(ds?.supportsNetplay).toBe(false);
  });
});
