/**
 * Phase 43/44/45 — Wii U Overhaul, 3DS Premium Experience & Nintendo Switch
 *
 * Tests for:
 *  - Wii U expanded mock game catalog (≥ 16 games including new titles)
 *  - Wii U expanded session templates (≥ 17 templates)
 *  - Wii U netplay whitelist entries (≥ 11 wiiu entries)
 *  - Cemu backend expanded notes (≥ 15 notes)
 *  - Wii U adapter new options (wiiuGamepadMode, wiiuProControllerEnabled, performancePreset)
 *  - 3DS expanded mock game catalog (≥ 16 games including new titles)
 *  - 3DS expanded session templates (≥ 17 templates)
 *  - 3DS netplay whitelist entries (≥ 9 3ds entries)
 *  - Citra/Lime3DS backend expanded notes (≥ 18 notes)
 *  - 3DS adapter new options (threeDsLayout, threeDsRegion)
 *  - Nintendo Switch system type and SYSTEM_INFO entry
 *  - Ryujinx backend registration
 *  - Switch ROM scanner extensions (.nsp, .xci, .nca, .nso)
 *  - Switch adapter (launch args, LDN mode, prod keys)
 *  - Switch mock game catalog (≥ 10 switch games)
 *  - Switch session templates (≥ 10 switch templates)
 *  - Switch netplay whitelist entries (≥ 7 switch entries)
 *  - SYSTEM_BACKEND_MAP switch → ryujinx
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import { ROM_EXTENSIONS } from '../../../../packages/emulator-bridge/src/scanner';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';
import { NETPLAY_WHITELIST, approvedGamesForSystem } from '../../../../apps/desktop/src/lib/netplay-whitelist';
import { SYSTEM_BACKEND_MAP } from '../../../../apps/desktop/src/lib/emulator-settings';
import { SYSTEM_INFO } from '../../../../packages/shared-types/src/systems';

const store = new SessionTemplateStore();

// ─────────────────────────────────────────────────────────────────────────────
// Wii U — mock game catalog
// ─────────────────────────────────────────────────────────────────────────────

describe('Wii U mock game catalog — overhaul', () => {
  const wiiuGames = MOCK_GAMES.filter((g) => g.system === 'Wii U');

  it('has at least 16 Wii U games', () => {
    expect(wiiuGames.length).toBeGreaterThanOrEqual(16);
  });

  it('includes Super Mario 3D World', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-super-mario-3d-world')).toBeDefined();
  });

  it('includes Hyrule Warriors', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-hyrule-warriors')).toBeDefined();
  });

  it('includes Star Fox Zero', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-star-fox-zero')).toBeDefined();
  });

  it("includes Yoshi's Woolly World", () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-yoshis-woolly-world')).toBeDefined();
  });

  it('includes Xenoblade Chronicles X', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-xenoblade-chronicles-x')).toBeDefined();
  });

  it('includes Captain Toad: Treasure Tracker', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-captain-toad-treasure-tracker')).toBeDefined();
  });

  it('Mario Kart 8 is rated for 4 players', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-mario-kart-8')?.maxPlayers).toBe(4);
  });

  it('Smash Bros. Wii U supports 8 players', () => {
    expect(wiiuGames.find((g) => g.id === 'wiiu-super-smash-bros-wiiu')?.maxPlayers).toBe(8);
  });

  it('all Wii U games have a coverEmoji', () => {
    wiiuGames.forEach((g) => expect(g.coverEmoji).toBeTruthy());
  });

  it('all Wii U games have a description', () => {
    wiiuGames.forEach((g) => expect(g.description).toBeTruthy());
  });

  it('no duplicate Wii U IDs', () => {
    const ids = wiiuGames.map((g) => g.id);
    expect(new Set(ids).size).toBe(ids.length);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Wii U — session templates
// ─────────────────────────────────────────────────────────────────────────────

describe('Wii U session templates — overhaul', () => {
  const wiiuTemplates = store.getAllForSystem('wiiu');

  it('has at least 17 Wii U session templates', () => {
    expect(wiiuTemplates.length).toBeGreaterThanOrEqual(17);
  });

  it('includes Super Mario 3D World 4P template', () => {
    expect(store.get('wiiu-super-mario-3d-world-4p')).not.toBeNull();
  });

  it('includes Hyrule Warriors 2P template', () => {
    expect(store.get('wiiu-hyrule-warriors-2p')).not.toBeNull();
  });

  it("includes Yoshi's Woolly World 2P template", () => {
    expect(store.get('wiiu-yoshis-woolly-world-2p')).not.toBeNull();
  });

  it('includes Xenoblade Chronicles X 4P template', () => {
    expect(store.get('wiiu-xenoblade-chronicles-x-4p')).not.toBeNull();
  });

  it('includes Captain Toad 2P template', () => {
    expect(store.get('wiiu-captain-toad-2p')).not.toBeNull();
  });

  it('all Wii U templates use cemu backend', () => {
    wiiuTemplates.forEach((t) => expect(t.emulatorBackendId).toBe('cemu'));
  });

  it('all Wii U templates target latency > 0', () => {
    wiiuTemplates.forEach((t) => expect(t.latencyTarget).toBeGreaterThan(0));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Wii U — netplay whitelist
// ─────────────────────────────────────────────────────────────────────────────

describe('Wii U netplay whitelist', () => {
  const wiiuEntries = NETPLAY_WHITELIST.filter((e) => e.system === 'wiiu');

  it('has at least 11 Wii U whitelist entries', () => {
    expect(wiiuEntries.length).toBeGreaterThanOrEqual(11);
  });

  it('Mario Kart 8 is approved', () => {
    const e = NETPLAY_WHITELIST.find((x) => x.gameId === 'wiiu-mario-kart-8');
    expect(e?.rating).toBe('approved');
  });

  it('Smash Bros. Wii U is approved', () => {
    const e = NETPLAY_WHITELIST.find((x) => x.gameId === 'wiiu-super-smash-bros-wiiu');
    expect(e?.rating).toBe('approved');
  });

  it('Splatoon is approved', () => {
    const e = NETPLAY_WHITELIST.find((x) => x.gameId === 'wiiu-splatoon');
    expect(e?.rating).toBe('approved');
  });

  it('has at least 7 approved Wii U entries', () => {
    expect(approvedGamesForSystem('wiiu').length).toBeGreaterThanOrEqual(7);
  });

  it('all Wii U entries have a non-empty reason', () => {
    wiiuEntries.forEach((e) => expect(e.reason.length).toBeGreaterThan(0));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Cemu backend
// ─────────────────────────────────────────────────────────────────────────────

describe('Cemu backend — expanded notes', () => {
  const cemu = KNOWN_BACKENDS.find((b) => b.id === 'cemu');

  it('Cemu backend exists', () => {
    expect(cemu).toBeDefined();
  });

  it('has at least 15 notes', () => {
    expect(cemu!.notes.length).toBeGreaterThanOrEqual(15);
  });

  it('mentions Vulkan', () => {
    expect(cemu!.notes.some((n) => n.toLowerCase().includes('vulkan'))).toBe(true);
  });

  it('mentions keys.txt', () => {
    expect(cemu!.notes.some((n) => n.includes('keys.txt'))).toBe(true);
  });

  it('mentions .wua format', () => {
    expect(cemu!.notes.some((n) => n.includes('.wua'))).toBe(true);
  });

  it('mentions Pretendo Network', () => {
    expect(cemu!.notes.some((n) => n.toLowerCase().includes('pretendo'))).toBe(true);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Wii U adapter — new options
// ─────────────────────────────────────────────────────────────────────────────

describe('Wii U adapter — new options', () => {
  const adapter = createSystemAdapter('wiiu');

  it('builds basic Cemu launch args', () => {
    const args = adapter.buildLaunchArgs('/games/mk8.wux', {});
    expect(args).toContain('-g');
    expect(args).toContain('/games/mk8.wux');
  });

  it('appends -f when fullscreen is true', () => {
    const args = adapter.buildLaunchArgs('/games/mk8.wux', { fullscreen: true });
    expect(args).toContain('-f');
  });

  it('appends interpreter flag when performancePreset is accurate', () => {
    const args = adapter.buildLaunchArgs('/games/mk8.wux', { performancePreset: 'accurate' });
    expect(args).toContain('--cpu-mode');
    expect(args).toContain('interpreter');
  });

  it('does not append interpreter flag for balanced preset', () => {
    const args = adapter.buildLaunchArgs('/games/mk8.wux', { performancePreset: 'balanced' });
    expect(args).not.toContain('interpreter');
  });

  it('appends compatibility flags verbatim', () => {
    const args = adapter.buildLaunchArgs('/games/mk8.wux', { compatibilityFlags: ['--single-core'] });
    expect(args).toContain('--single-core');
  });

  it('save path is wiiu/<gameId>', () => {
    expect(adapter.getSavePath('wiiu-mario-kart-8', '/saves')).toBe('/saves/wiiu/wiiu-mario-kart-8');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Nintendo 3DS — mock game catalog
// ─────────────────────────────────────────────────────────────────────────────

describe('Nintendo 3DS mock game catalog — overhaul', () => {
  const threeDsGames = MOCK_GAMES.filter((g) => g.system === '3DS');

  it('has at least 16 3DS games', () => {
    expect(threeDsGames.length).toBeGreaterThanOrEqual(16);
  });

  it("includes Luigi's Mansion: Dark Moon", () => {
    expect(threeDsGames.find((g) => g.id === '3ds-luigis-mansion-dark-moon')).toBeDefined();
  });

  it('includes Kirby: Triple Deluxe', () => {
    expect(threeDsGames.find((g) => g.id === '3ds-kirby-triple-deluxe')).toBeDefined();
  });

  it('includes Fire Emblem: Awakening', () => {
    expect(threeDsGames.find((g) => g.id === '3ds-fire-emblem-awakening')).toBeDefined();
  });

  it('includes Tomodachi Life', () => {
    expect(threeDsGames.find((g) => g.id === '3ds-tomodachi-life')).toBeDefined();
  });

  it('includes Pokémon Ultra Sun', () => {
    expect(threeDsGames.find((g) => g.id === '3ds-pokemon-ultra-sun')).toBeDefined();
  });

  it('all 3DS games have a coverEmoji', () => {
    threeDsGames.forEach((g) => expect(g.coverEmoji).toBeTruthy());
  });

  it('no duplicate 3DS IDs', () => {
    const ids = threeDsGames.map((g) => g.id);
    expect(new Set(ids).size).toBe(ids.length);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Nintendo 3DS — session templates
// ─────────────────────────────────────────────────────────────────────────────

describe('Nintendo 3DS session templates — overhaul', () => {
  const threeDsTemplates = store.getAllForSystem('3ds');

  it('has at least 17 3DS session templates', () => {
    expect(threeDsTemplates.length).toBeGreaterThanOrEqual(17);
  });

  it("includes Luigi's Mansion Dark Moon 4P template", () => {
    expect(store.get('3ds-luigis-mansion-dark-moon-4p')).not.toBeNull();
  });

  it('includes Kirby Triple Deluxe 4P template', () => {
    expect(store.get('3ds-kirby-triple-deluxe-4p')).not.toBeNull();
  });

  it('includes Fire Emblem Awakening 2P template', () => {
    expect(store.get('3ds-fire-emblem-awakening-2p')).not.toBeNull();
  });

  it('includes Pokémon Ultra Sun 2P template', () => {
    expect(store.get('3ds-pokemon-ultra-sun-2p')).not.toBeNull();
  });

  it('all 3DS templates use citra backend', () => {
    threeDsTemplates.forEach((t) => expect(t.emulatorBackendId).toBe('citra'));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Nintendo 3DS — netplay whitelist
// ─────────────────────────────────────────────────────────────────────────────

describe('Nintendo 3DS netplay whitelist', () => {
  const threeDsEntries = NETPLAY_WHITELIST.filter((e) => e.system === '3ds');

  it('has at least 9 3DS whitelist entries', () => {
    expect(threeDsEntries.length).toBeGreaterThanOrEqual(9);
  });

  it('Mario Kart 7 is approved', () => {
    const e = NETPLAY_WHITELIST.find((x) => x.gameId === '3ds-mario-kart-7');
    expect(e?.rating).toBe('approved');
  });

  it('Monster Hunter 4 Ultimate is approved', () => {
    const e = NETPLAY_WHITELIST.find((x) => x.gameId === '3ds-monster-hunter-4-ultimate');
    expect(e?.rating).toBe('approved');
  });

  it('has at least 7 approved 3DS entries', () => {
    expect(approvedGamesForSystem('3ds').length).toBeGreaterThanOrEqual(7);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Citra / Lime3DS backend — expanded notes
// ─────────────────────────────────────────────────────────────────────────────

describe('Citra/Lime3DS backend — expanded notes', () => {
  const citra = KNOWN_BACKENDS.find((b) => b.id === 'citra');

  it('Citra backend exists', () => {
    expect(citra).toBeDefined();
  });

  it('has at least 18 notes', () => {
    expect(citra!.notes.length).toBeGreaterThanOrEqual(18);
  });

  it('mentions Lime3DS', () => {
    expect(citra!.notes.some((n) => n.toLowerCase().includes('lime3ds'))).toBe(true);
  });

  it('mentions layout option', () => {
    expect(citra!.notes.some((n) => n.toLowerCase().includes('layout'))).toBe(true);
  });

  it('mentions shader cache', () => {
    expect(citra!.notes.some((n) => n.toLowerCase().includes('shader'))).toBe(true);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// 3DS adapter — new options
// ─────────────────────────────────────────────────────────────────────────────

describe('3DS adapter — new options', () => {
  const adapter = createSystemAdapter('3ds');

  it('builds basic Citra launch args', () => {
    const args = adapter.buildLaunchArgs('/games/mk7.3ds', {});
    expect(args).toContain('/games/mk7.3ds');
  });

  it('appends --fullscreen when fullscreen is true', () => {
    const args = adapter.buildLaunchArgs('/games/mk7.3ds', { fullscreen: true });
    expect(args).toContain('--fullscreen');
  });

  it('appends layout option for large layout', () => {
    const args = adapter.buildLaunchArgs('/games/mk7.3ds', { threeDsLayout: 'large' });
    expect(args).toContain('--layout-option');
    expect(args).toContain('2');
  });

  it('appends layout option for side-by-side', () => {
    const args = adapter.buildLaunchArgs('/games/mk7.3ds', { threeDsLayout: 'side-by-side' });
    expect(args).toContain('--layout-option');
    expect(args).toContain('3');
  });

  it('does not append layout option for default layout', () => {
    const args = adapter.buildLaunchArgs('/games/mk7.3ds', { threeDsLayout: 'default' });
    expect(args).not.toContain('--layout-option');
  });

  it('appends region override', () => {
    const args = adapter.buildLaunchArgs('/games/mk7.3ds', { threeDsRegion: 'usa' });
    expect(args).toContain('--region');
    expect(args).toContain('usa');
  });

  it('does not append region for auto', () => {
    const args = adapter.buildLaunchArgs('/games/mk7.3ds', { threeDsRegion: 'auto' });
    expect(args).not.toContain('--region');
  });

  it('save path is 3ds/<gameId>', () => {
    expect(adapter.getSavePath('3ds-mario-kart-7', '/saves')).toBe('/saves/3ds/3ds-mario-kart-7');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Nintendo Switch — system type
// ─────────────────────────────────────────────────────────────────────────────

describe('Nintendo Switch — system type', () => {
  it('switch is a valid SupportedSystem', () => {
    expect(SYSTEM_INFO['switch']).toBeDefined();
  });

  it('has generation 9', () => {
    expect(SYSTEM_INFO['switch'].generation).toBe(9);
  });

  it('has correct color #E4003A', () => {
    expect(SYSTEM_INFO['switch'].color).toBe('#E4003A');
  });

  it('has shortName Switch', () => {
    expect(SYSTEM_INFO['switch'].shortName).toBe('Switch');
  });

  it('supports up to 8 local players', () => {
    expect(SYSTEM_INFO['switch'].maxLocalPlayers).toBe(8);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Ryujinx backend
// ─────────────────────────────────────────────────────────────────────────────

describe('Ryujinx backend', () => {
  const ryujinx = KNOWN_BACKENDS.find((b) => b.id === 'ryujinx');

  it('Ryujinx backend exists', () => {
    expect(ryujinx).toBeDefined();
  });

  it('is associated with switch system', () => {
    expect(ryujinx!.systems).toContain('switch');
  });

  it('supportsNetplay is true', () => {
    expect(ryujinx!.supportsNetplay).toBe(true);
  });

  it('has at least 15 notes', () => {
    expect(ryujinx!.notes.length).toBeGreaterThanOrEqual(15);
  });

  it('mentions LDN', () => {
    expect(ryujinx!.notes.some((n) => n.toLowerCase().includes('ldn'))).toBe(true);
  });

  it('mentions prod.keys', () => {
    expect(ryujinx!.notes.some((n) => n.includes('prod.keys'))).toBe(true);
  });

  it('mentions RyuBing', () => {
    expect(ryujinx!.notes.some((n) => n.toLowerCase().includes('ryubing'))).toBe(true);
  });

  it('mentions Vulkan', () => {
    expect(ryujinx!.notes.some((n) => n.toLowerCase().includes('vulkan'))).toBe(true);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Switch ROM extensions
// ─────────────────────────────────────────────────────────────────────────────

describe('Switch ROM scanner extensions', () => {
  it('.nsp maps to switch', () => {
    expect(ROM_EXTENSIONS['.nsp']).toBe('switch');
  });

  it('.xci maps to switch', () => {
    expect(ROM_EXTENSIONS['.xci']).toBe('switch');
  });

  it('.nca maps to switch', () => {
    expect(ROM_EXTENSIONS['.nca']).toBe('switch');
  });

  it('.nso maps to switch', () => {
    expect(ROM_EXTENSIONS['.nso']).toBe('switch');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Switch adapter
// ─────────────────────────────────────────────────────────────────────────────

describe('Switch adapter', () => {
  const adapter = createSystemAdapter('switch');

  it('preferred backend is ryujinx', () => {
    expect(adapter.preferredBackendId).toBe('ryujinx');
  });

  it('builds basic Ryujinx launch args', () => {
    const args = adapter.buildLaunchArgs('/games/mk8dx.xci', {});
    expect(args).toContain('/games/mk8dx.xci');
  });

  it('appends --fullscreen when fullscreen is true', () => {
    const args = adapter.buildLaunchArgs('/games/mk8dx.xci', { fullscreen: true });
    expect(args).toContain('--fullscreen');
  });

  it('appends LDN multiplayer mode when switchLdnEnabled', () => {
    const args = adapter.buildLaunchArgs('/games/mk8dx.xci', { switchLdnEnabled: true });
    expect(args).toContain('--multiplayer-mode');
    expect(args).toContain('ldn');
  });

  it('does not append LDN when switchLdnEnabled is false', () => {
    const args = adapter.buildLaunchArgs('/games/mk8dx.xci', { switchLdnEnabled: false });
    expect(args).not.toContain('--multiplayer-mode');
  });

  it('appends prod.keys path when switchProdKeys is set', () => {
    const args = adapter.buildLaunchArgs('/games/mk8dx.xci', { switchProdKeys: '/keys/prod.keys' });
    expect(args).toContain('--prod-keys');
    expect(args).toContain('/keys/prod.keys');
  });

  it('appends compatibility flags verbatim', () => {
    const args = adapter.buildLaunchArgs('/games/mk8dx.xci', { compatibilityFlags: ['--no-docked'] });
    expect(args).toContain('--no-docked');
  });

  it('save path is switch/<gameId>', () => {
    expect(adapter.getSavePath('switch-mario-kart-8-deluxe', '/saves')).toBe('/saves/switch/switch-mario-kart-8-deluxe');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Switch mock game catalog
// ─────────────────────────────────────────────────────────────────────────────

describe('Switch mock game catalog', () => {
  const switchGames = MOCK_GAMES.filter((g) => g.system === 'Switch');

  it('has at least 10 Switch games', () => {
    expect(switchGames.length).toBeGreaterThanOrEqual(10);
  });

  it('includes Mario Kart 8 Deluxe', () => {
    expect(switchGames.find((g) => g.id === 'switch-mario-kart-8-deluxe')).toBeDefined();
  });

  it('includes Super Smash Bros. Ultimate', () => {
    expect(switchGames.find((g) => g.id === 'switch-super-smash-bros-ultimate')).toBeDefined();
  });

  it('includes Splatoon 3', () => {
    expect(switchGames.find((g) => g.id === 'switch-splatoon-3')).toBeDefined();
  });

  it('includes Pokémon Scarlet', () => {
    expect(switchGames.find((g) => g.id === 'switch-pokemon-scarlet')).toBeDefined();
  });

  it('includes Animal Crossing: New Horizons', () => {
    expect(switchGames.find((g) => g.id === 'switch-animal-crossing-new-horizons')).toBeDefined();
  });

  it('includes Mario Party Superstars', () => {
    expect(switchGames.find((g) => g.id === 'switch-mario-party-superstars')).toBeDefined();
  });

  it('Smash Ultimate supports 8 players', () => {
    expect(switchGames.find((g) => g.id === 'switch-super-smash-bros-ultimate')?.maxPlayers).toBe(8);
  });

  it('all Switch games have a coverEmoji', () => {
    switchGames.forEach((g) => expect(g.coverEmoji).toBeTruthy());
  });

  it('all Switch games have a description', () => {
    switchGames.forEach((g) => expect(g.description).toBeTruthy());
  });

  it('no duplicate Switch game IDs', () => {
    const ids = switchGames.map((g) => g.id);
    expect(new Set(ids).size).toBe(ids.length);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Switch session templates
// ─────────────────────────────────────────────────────────────────────────────

describe('Switch session templates', () => {
  const switchTemplates = store.getAllForSystem('switch');

  it('has at least 10 Switch session templates', () => {
    expect(switchTemplates.length).toBeGreaterThanOrEqual(10);
  });

  it('includes Mario Kart 8 Deluxe 4P template', () => {
    expect(store.get('switch-mario-kart-8-deluxe-4p')).not.toBeNull();
  });

  it('includes Smash Ultimate 4P template', () => {
    expect(store.get('switch-super-smash-bros-ultimate-4p')).not.toBeNull();
  });

  it('includes Smash Ultimate 8P template', () => {
    expect(store.get('switch-super-smash-bros-ultimate-8p')).not.toBeNull();
  });

  it('includes Splatoon 3 4P template', () => {
    expect(store.get('switch-splatoon-3-4p')).not.toBeNull();
  });

  it('includes Mario Party Superstars 4P template', () => {
    expect(store.get('switch-mario-party-superstars-4p')).not.toBeNull();
  });

  it('all Switch templates use ryujinx backend', () => {
    switchTemplates.forEach((t) => expect(t.emulatorBackendId).toBe('ryujinx'));
  });

  it('racing/fighting templates target ≤ 80 ms', () => {
    const mkTemplate = store.get('switch-mario-kart-8-deluxe-4p');
    const smashTemplate = store.get('switch-super-smash-bros-ultimate-4p');
    expect(mkTemplate!.latencyTarget).toBeLessThanOrEqual(80);
    expect(smashTemplate!.latencyTarget).toBeLessThanOrEqual(80);
  });

  it('default 2P template exists', () => {
    expect(store.get('switch-default-2p')).not.toBeNull();
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Switch netplay whitelist
// ─────────────────────────────────────────────────────────────────────────────

describe('Switch netplay whitelist', () => {
  const switchEntries = NETPLAY_WHITELIST.filter((e) => e.system === 'switch');

  it('has at least 7 Switch whitelist entries', () => {
    expect(switchEntries.length).toBeGreaterThanOrEqual(7);
  });

  it('Mario Kart 8 Deluxe is approved', () => {
    const e = NETPLAY_WHITELIST.find((x) => x.gameId === 'switch-mario-kart-8-deluxe');
    expect(e?.rating).toBe('approved');
  });

  it('Smash Ultimate is approved', () => {
    const e = NETPLAY_WHITELIST.find((x) => x.gameId === 'switch-super-smash-bros-ultimate');
    expect(e?.rating).toBe('approved');
  });

  it('Splatoon 3 is approved', () => {
    const e = NETPLAY_WHITELIST.find((x) => x.gameId === 'switch-splatoon-3');
    expect(e?.rating).toBe('approved');
  });

  it('has at least 4 approved Switch entries', () => {
    expect(approvedGamesForSystem('switch').length).toBeGreaterThanOrEqual(4);
  });

  it('all Switch entries have a reason string', () => {
    switchEntries.forEach((e) => expect(e.reason.length).toBeGreaterThan(0));
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// SYSTEM_BACKEND_MAP
// ─────────────────────────────────────────────────────────────────────────────

describe('SYSTEM_BACKEND_MAP', () => {
  it('switch maps to ryujinx', () => {
    expect(SYSTEM_BACKEND_MAP['switch']).toBe('ryujinx');
  });

  it('wiiu maps to cemu', () => {
    expect(SYSTEM_BACKEND_MAP['wiiu']).toBe('cemu');
  });

  it('3ds maps to citra', () => {
    expect(SYSTEM_BACKEND_MAP['3ds']).toBe('citra');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// No duplicate IDs across entire MOCK_GAMES catalog
// ─────────────────────────────────────────────────────────────────────────────

describe('Global mock game catalog integrity', () => {
  it('no duplicate game IDs across all systems', () => {
    const ids = MOCK_GAMES.map((g) => g.id);
    const unique = new Set(ids);
    const duplicates = ids.filter((id, i) => ids.indexOf(id) !== i);
    expect(duplicates).toEqual([]);
    expect(unique.size).toBe(ids.length);
  });
});
