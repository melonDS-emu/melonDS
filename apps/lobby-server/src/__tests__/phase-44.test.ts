/**
 * Phase 44 — PS2/PS3/SegaCD/32X/GB/GBC Overhaul
 *
 * Tests for:
 *  - Enhanced PS2 adapter flags: ps2HwRenderer, ps2Region, ps2FastBoot, multitapEnabled
 *  - New PS3 adapter (RPCS3/RPCN)
 *  - New Sega 32X adapter (RetroArch PicoDrive)
 *  - New mock games: 6 PS2, 10 PS3, 9 Sega CD, 6 Sega 32X, 4 GB, 3 GBC
 *  - New session templates for all new games
 *  - New game metadata entries
 *  - New netplay whitelist entries
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { MOCK_GAMES } from '../../../../apps/desktop/src/data/mock-games';
import { GameMetadataStore } from '../game-metadata-store';
import { NETPLAY_WHITELIST, approvedGamesForSystem } from '../../../../apps/desktop/src/lib/netplay-whitelist';

// ─────────────────────────────────────────────────────────────────────────────
// PS2 adapter — enhanced flags
// ─────────────────────────────────────────────────────────────────────────────

describe('PS2 adapter — enhanced flags', () => {
  const adapter = createSystemAdapter('ps2');

  it('ps2 adapter has correct system', () => {
    expect(adapter.system).toBe('ps2');
  });

  it('ps2 adapter prefers pcsx2 backend', () => {
    expect(adapter.preferredBackendId).toBe('pcsx2');
  });

  it('ps2 adapter has retroarch as fallback', () => {
    expect(adapter.fallbackBackendIds).toContain('retroarch');
  });

  it('ps2HwRenderer flag produces --renderer= arg', () => {
    const args = adapter.buildLaunchArgs('game.iso', { ps2HwRenderer: 'Vulkan' });
    expect(args).toContain('--renderer=Vulkan');
  });

  it('ps2HwRenderer OpenGL produces correct arg', () => {
    const args = adapter.buildLaunchArgs('game.iso', { ps2HwRenderer: 'OpenGL' });
    expect(args).toContain('--renderer=OpenGL');
  });

  it('ps2Region flag produces --region= arg', () => {
    const args = adapter.buildLaunchArgs('game.iso', { ps2Region: 'NTSC-U' });
    expect(args).toContain('--region=NTSC-U');
  });

  it('ps2Region PAL produces correct arg', () => {
    const args = adapter.buildLaunchArgs('game.iso', { ps2Region: 'PAL' });
    expect(args).toContain('--region=PAL');
  });

  it('ps2FastBoot flag produces --fast-boot arg', () => {
    const args = adapter.buildLaunchArgs('game.iso', { ps2FastBoot: true });
    expect(args).toContain('--fast-boot');
  });

  it('multitapEnabled flag produces --multitap arg', () => {
    const args = adapter.buildLaunchArgs('game.iso', { multitapEnabled: true });
    expect(args).toContain('--multitap');
  });

  it('ROM path appears after -- separator', () => {
    const args = adapter.buildLaunchArgs('game.iso', {});
    const dashDashIdx = args.indexOf('--');
    expect(dashDashIdx).toBeGreaterThan(-1);
    expect(args[dashDashIdx + 1]).toBe('game.iso');
  });

  it('all new flags combined produce correct args', () => {
    const args = adapter.buildLaunchArgs('game.iso', {
      ps2HwRenderer: 'Vulkan',
      ps2Region: 'NTSC-U',
      ps2FastBoot: true,
      multitapEnabled: true,
      fullscreen: true,
    });
    expect(args).toContain('--renderer=Vulkan');
    expect(args).toContain('--region=NTSC-U');
    expect(args).toContain('--fast-boot');
    expect(args).toContain('--multitap');
    expect(args).toContain('--fullscreen');
  });

  it('ps2 getSavePath returns correct path', () => {
    expect(adapter.getSavePath('tekken-5', '/saves')).toBe('/saves/ps2/tekken-5');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// PS3 adapter
// ─────────────────────────────────────────────────────────────────────────────

describe('PS3 adapter', () => {
  const adapter = createSystemAdapter('ps3');

  it('ps3 adapter has correct system', () => {
    expect(adapter.system).toBe('ps3');
  });

  it('ps3 adapter prefers rpcs3 backend', () => {
    expect(adapter.preferredBackendId).toBe('rpcs3');
  });

  it('ps3 adapter has empty fallback backends', () => {
    expect(adapter.fallbackBackendIds).toHaveLength(0);
  });

  it('ps3 launch args start with --no-gui', () => {
    const args = adapter.buildLaunchArgs('game.pkg', {});
    expect(args[0]).toBe('--no-gui');
  });

  it('ps3 launch args include rom path', () => {
    const args = adapter.buildLaunchArgs('game.pkg', {});
    expect(args).toContain('game.pkg');
  });

  it('ps3 fullscreen flag is appended', () => {
    const args = adapter.buildLaunchArgs('game.pkg', { fullscreen: true });
    expect(args).toContain('--fullscreen');
  });

  it('ps3 debug flag produces --verbose', () => {
    const args = adapter.buildLaunchArgs('game.pkg', { debug: true });
    expect(args).toContain('--verbose');
  });

  it('ps3 compatibilityFlags are appended', () => {
    const args = adapter.buildLaunchArgs('game.pkg', { compatibilityFlags: ['--no-audio'] });
    expect(args).toContain('--no-audio');
  });

  it('ps3 getSavePath returns correct path', () => {
    expect(adapter.getSavePath('street-fighter-iv', '/saves')).toBe('/saves/ps3/street-fighter-iv');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Sega 32X adapter
// ─────────────────────────────────────────────────────────────────────────────

describe('Sega 32X adapter', () => {
  const adapter = createSystemAdapter('sega32x');

  it('sega32x adapter has correct system', () => {
    expect(adapter.system).toBe('sega32x');
  });

  it('sega32x adapter prefers retroarch', () => {
    expect(adapter.preferredBackendId).toBe('retroarch');
  });

  it('sega32x adapter has empty fallback backends', () => {
    expect(adapter.fallbackBackendIds).toHaveLength(0);
  });

  it('sega32x launch args include rom path', () => {
    const args = adapter.buildLaunchArgs('game.32x', {});
    expect(args).toContain('game.32x');
  });

  it('sega32x getSavePath returns correct path', () => {
    expect(adapter.getSavePath('knuckles-chaotix', '/saves')).toBe('/saves/sega32x/knuckles-chaotix');
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Mock games — new titles
// ─────────────────────────────────────────────────────────────────────────────

describe('PS2 mock games — new additions', () => {
  const ps2Games = MOCK_GAMES.filter((g) => g.system === 'PS2');

  it('has at least 15 PS2 games after overhaul', () => {
    expect(ps2Games.length).toBeGreaterThanOrEqual(15);
  });

  it('includes Jak and Daxter', () => {
    expect(ps2Games.find((g) => g.id === 'ps2-jak-and-daxter')).toBeDefined();
  });

  it('includes Tekken 5', () => {
    expect(ps2Games.find((g) => g.id === 'ps2-tekken-5')).toBeDefined();
  });

  it('includes Devil May Cry', () => {
    expect(ps2Games.find((g) => g.id === 'ps2-devil-may-cry')).toBeDefined();
  });

  it('includes SSX 3', () => {
    expect(ps2Games.find((g) => g.id === 'ps2-ssx3')).toBeDefined();
  });

  it('includes TimeSplitters 2', () => {
    expect(ps2Games.find((g) => g.id === 'ps2-timesplitters-2')).toBeDefined();
  });

  it('includes ICO', () => {
    expect(ps2Games.find((g) => g.id === 'ps2-ico')).toBeDefined();
  });

  it('TimeSplitters 2 is 4-player', () => {
    const game = ps2Games.find((g) => g.id === 'ps2-timesplitters-2');
    expect(game?.maxPlayers).toBe(4);
  });

  it('Tekken 5 is 2-player', () => {
    const game = ps2Games.find((g) => g.id === 'ps2-tekken-5');
    expect(game?.maxPlayers).toBe(2);
  });

  it('all PS2 games have a non-empty description', () => {
    for (const game of ps2Games) {
      expect(game.description, `${game.id} missing description`).toBeTruthy();
    }
  });
});

describe('PS3 mock games', () => {
  const ps3Games = MOCK_GAMES.filter((g) => g.system === 'PS3');

  it('has at least 10 PS3 games', () => {
    expect(ps3Games.length).toBeGreaterThanOrEqual(10);
  });

  it('includes Street Fighter IV', () => {
    expect(ps3Games.find((g) => g.id === 'ps3-street-fighter-iv')).toBeDefined();
  });

  it('includes Tekken 6', () => {
    expect(ps3Games.find((g) => g.id === 'ps3-tekken-6')).toBeDefined();
  });

  it('includes LittleBigPlanet', () => {
    expect(ps3Games.find((g) => g.id === 'ps3-little-big-planet')).toBeDefined();
  });

  it('includes Castle Crashers', () => {
    expect(ps3Games.find((g) => g.id === 'ps3-castle-crashers')).toBeDefined();
  });

  it('includes Gran Turismo 5', () => {
    expect(ps3Games.find((g) => g.id === 'ps3-gran-turismo-5')).toBeDefined();
  });

  it('Gran Turismo 5 supports 16 players', () => {
    const game = ps3Games.find((g) => g.id === 'ps3-gran-turismo-5');
    expect(game?.maxPlayers).toBe(16);
  });

  it('WipEout HD supports 8 players', () => {
    const game = ps3Games.find((g) => g.id === 'ps3-wipeout-hd');
    expect(game?.maxPlayers).toBe(8);
  });
});

describe('Sega CD mock games', () => {
  const segaCDGames = MOCK_GAMES.filter((g) => g.system === 'Sega CD');

  it('has at least 9 Sega CD games', () => {
    expect(segaCDGames.length).toBeGreaterThanOrEqual(9);
  });

  it('includes Sonic CD', () => {
    expect(segaCDGames.find((g) => g.id === 'segacd-sonic-cd')).toBeDefined();
  });

  it('includes NBA Jam', () => {
    expect(segaCDGames.find((g) => g.id === 'segacd-nba-jam')).toBeDefined();
  });

  it('includes Snatcher', () => {
    expect(segaCDGames.find((g) => g.id === 'segacd-snatcher')).toBeDefined();
  });
});

describe('Sega 32X mock games', () => {
  const sega32xGames = MOCK_GAMES.filter((g) => g.system === 'Sega 32X');

  it('has at least 6 Sega 32X games', () => {
    expect(sega32xGames.length).toBeGreaterThanOrEqual(6);
  });

  it("includes Knuckles' Chaotix", () => {
    expect(sega32xGames.find((g) => g.id === 'sega32x-knuckles-chaotix')).toBeDefined();
  });

  it('includes Cosmic Carnage', () => {
    expect(sega32xGames.find((g) => g.id === 'sega32x-cosmic-carnage')).toBeDefined();
  });

  it('includes DOOM', () => {
    expect(sega32xGames.find((g) => g.id === 'sega32x-doom')).toBeDefined();
  });
});

describe('GB additional mock games', () => {
  const gbGames = MOCK_GAMES.filter((g) => g.system === 'GB');

  it('includes Pokemon Yellow', () => {
    expect(gbGames.find((g) => g.id === 'gb-pokemon-yellow')).toBeDefined();
  });

  it("includes Link's Awakening", () => {
    expect(gbGames.find((g) => g.id === 'gb-links-awakening')).toBeDefined();
  });

  it('includes Wario Land', () => {
    expect(gbGames.find((g) => g.id === 'gb-wario-land')).toBeDefined();
  });

  it('includes Metroid II', () => {
    expect(gbGames.find((g) => g.id === 'gb-metroid-ii')).toBeDefined();
  });
});

describe('GBC additional mock games', () => {
  const gbcGames = MOCK_GAMES.filter((g) => g.system === 'GBC');

  it('includes Oracle of Seasons', () => {
    expect(gbcGames.find((g) => g.id === 'gbc-zelda-oracle-of-seasons')).toBeDefined();
  });

  it('includes Oracle of Ages', () => {
    expect(gbcGames.find((g) => g.id === 'gbc-zelda-oracle-of-ages')).toBeDefined();
  });

  it('includes Wario Land 3', () => {
    expect(gbcGames.find((g) => g.id === 'gbc-wario-land-3')).toBeDefined();
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Session templates
// ─────────────────────────────────────────────────────────────────────────────

describe('PS2 session templates — new additions', () => {
  const store = new SessionTemplateStore();

  it('ps2-jak-and-daxter-1p template exists', () => {
    expect(store.get('ps2-jak-and-daxter-1p')).toBeDefined();
  });

  it('ps2-tekken-5-2p template exists', () => {
    expect(store.get('ps2-tekken-5-2p')).toBeDefined();
  });

  it('ps2-devil-may-cry-1p template exists', () => {
    expect(store.get('ps2-devil-may-cry-1p')).toBeDefined();
  });

  it('ps2-ssx3-2p template exists', () => {
    expect(store.get('ps2-ssx3-2p')).toBeDefined();
  });

  it('ps2-timesplitters-2-4p template exists', () => {
    expect(store.get('ps2-timesplitters-2-4p')).toBeDefined();
  });

  it('ps2-ico-1p template exists', () => {
    expect(store.get('ps2-ico-1p')).toBeDefined();
  });

  it('tekken-5 template has 80ms latency target', () => {
    expect(store.get('ps2-tekken-5-2p')?.latencyTarget).toBe(80);
  });

  it('timesplitters-2 template has 4 players', () => {
    expect(store.get('ps2-timesplitters-2-4p')?.playerCount).toBe(4);
  });
});

describe('Sega CD additional templates', () => {
  const store = new SessionTemplateStore();

  it('segacd-nba-jam-2p template exists', () => {
    expect(store.get('segacd-nba-jam-2p')).toBeDefined();
  });

  it('segacd-batman-returns-2p-v2 template exists', () => {
    expect(store.get('segacd-batman-returns-2p-v2')).toBeDefined();
  });

  it('segacd-robo-aleste-2p-v2 template exists', () => {
    expect(store.get('segacd-robo-aleste-2p-v2')).toBeDefined();
  });

  it('NBA Jam template has 80ms latency target', () => {
    expect(store.get('segacd-nba-jam-2p')?.latencyTarget).toBe(80);
  });
});

describe('PS3 session templates', () => {
  const store = new SessionTemplateStore();

  it('ps3-default template exists', () => {
    expect(store.get('ps3-default')).toBeDefined();
  });

  it('ps3-street-fighter-iv-2p template exists', () => {
    expect(store.get('ps3-street-fighter-iv-2p')).toBeDefined();
  });

  it('ps3-tekken-6-2p template exists', () => {
    expect(store.get('ps3-tekken-6-2p')).toBeDefined();
  });

  it('ps3-little-big-planet-4p template exists', () => {
    expect(store.get('ps3-little-big-planet-4p')).toBeDefined();
  });

  it('ps3-castle-crashers-4p template exists', () => {
    expect(store.get('ps3-castle-crashers-4p')).toBeDefined();
  });

  it('ps3-gran-turismo-5-16p template exists', () => {
    expect(store.get('ps3-gran-turismo-5-16p')).toBeDefined();
  });

  it('PS3 SF4 template targets 60ms', () => {
    expect(store.get('ps3-street-fighter-iv-2p')?.latencyTarget).toBe(60);
  });

  it('PS3 Gran Turismo 5 template has 16 players', () => {
    expect(store.get('ps3-gran-turismo-5-16p')?.playerCount).toBe(16);
  });

  it('all PS3 templates use rpcs3 backend', () => {
    const ps3Templates = store.getAllForSystem('ps3');
    for (const t of ps3Templates) {
      expect(t.emulatorBackendId).toBe('rpcs3');
    }
  });
});

describe('Sega 32X session templates', () => {
  const store = new SessionTemplateStore();

  it('sega32x-default-2p template exists', () => {
    expect(store.get('sega32x-default-2p')).toBeDefined();
  });

  it('sega32x-knuckles-chaotix-2p template exists', () => {
    expect(store.get('sega32x-knuckles-chaotix-2p')).toBeDefined();
  });

  it('sega32x-cosmic-carnage-2p template exists', () => {
    expect(store.get('sega32x-cosmic-carnage-2p')).toBeDefined();
  });

  it('sega32x-doom-1p template exists', () => {
    expect(store.get('sega32x-doom-1p')).toBeDefined();
  });

  it('all 32X templates use retroarch backend', () => {
    const templates = store.getAllForSystem('sega32x');
    for (const t of templates) {
      expect(t.emulatorBackendId).toBe('retroarch');
    }
  });

  it('Cosmic Carnage template targets 80ms', () => {
    expect(store.get('sega32x-cosmic-carnage-2p')?.latencyTarget).toBe(80);
  });
});

describe('GB/GBC additional templates', () => {
  const store = new SessionTemplateStore();

  it('gb-pokemon-yellow-2p template exists', () => {
    expect(store.get('gb-pokemon-yellow-2p')).toBeDefined();
  });

  it('gb-links-awakening-1p template exists', () => {
    expect(store.get('gb-links-awakening-1p')).toBeDefined();
  });

  it('gbc-zelda-oracle-of-seasons-1p template exists', () => {
    expect(store.get('gbc-zelda-oracle-of-seasons-1p')).toBeDefined();
  });

  it('gbc-zelda-oracle-of-ages-1p template exists', () => {
    expect(store.get('gbc-zelda-oracle-of-ages-1p')).toBeDefined();
  });

  it('gbc-wario-land-3-1p template exists', () => {
    expect(store.get('gbc-wario-land-3-1p')).toBeDefined();
  });

  it('gb Pokemon Yellow template targets 200ms', () => {
    expect(store.get('gb-pokemon-yellow-2p')?.latencyTarget).toBe(200);
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Game metadata
// ─────────────────────────────────────────────────────────────────────────────

describe('Game metadata — PS2 new entries', () => {
  const store = new GameMetadataStore();

  it('ps2-jak-and-daxter metadata exists', () => {
    expect(store.get('ps2-jak-and-daxter')).toBeDefined();
  });

  it('ps2-tekken-5 metadata exists', () => {
    expect(store.get('ps2-tekken-5')).toBeDefined();
  });

  it('ps2-ssx3 metadata exists', () => {
    expect(store.get('ps2-ssx3')).toBeDefined();
  });

  it('ps2-timesplitters-2 metadata exists', () => {
    expect(store.get('ps2-timesplitters-2')).toBeDefined();
  });

  it('ps2-devil-may-cry metadata exists', () => {
    expect(store.get('ps2-devil-may-cry')).toBeDefined();
  });

  it('ps2-ico metadata exists', () => {
    expect(store.get('ps2-ico')).toBeDefined();
  });

  it('Tekken 5 metadata has quickHostPreset', () => {
    expect(store.get('ps2-tekken-5')?.quickHostPreset).toBe('1v1');
  });
});

describe('Game metadata — PS3 entries', () => {
  const store = new GameMetadataStore();

  it('ps3-street-fighter-iv metadata exists', () => {
    expect(store.get('ps3-street-fighter-iv')).toBeDefined();
  });

  it('ps3-tekken-6 metadata exists', () => {
    expect(store.get('ps3-tekken-6')).toBeDefined();
  });

  it('ps3-little-big-planet metadata exists', () => {
    expect(store.get('ps3-little-big-planet')).toBeDefined();
  });

  it('ps3-castle-crashers metadata exists', () => {
    expect(store.get('ps3-castle-crashers')).toBeDefined();
  });

  it('ps3-borderlands metadata exists', () => {
    expect(store.get('ps3-borderlands')).toBeDefined();
  });

  it('ps3-blazblue-calamity-trigger metadata exists', () => {
    expect(store.get('ps3-blazblue-calamity-trigger')).toBeDefined();
  });

  it('PS3 SF4 metadata developer is Capcom', () => {
    expect(store.get('ps3-street-fighter-iv')?.developer).toBe('Capcom');
  });
});

describe('Game metadata — Sega CD entries', () => {
  const store = new GameMetadataStore();

  it('segacd-sonic-cd metadata exists', () => {
    expect(store.get('segacd-sonic-cd')).toBeDefined();
  });

  it('segacd-nba-jam metadata exists', () => {
    expect(store.get('segacd-nba-jam')).toBeDefined();
  });

  it('segacd-robo-aleste metadata exists', () => {
    expect(store.get('segacd-robo-aleste')).toBeDefined();
  });
});

describe('Game metadata — Sega 32X entries', () => {
  const store = new GameMetadataStore();

  it('sega32x-knuckles-chaotix metadata exists', () => {
    expect(store.get('sega32x-knuckles-chaotix')).toBeDefined();
  });

  it('sega32x-cosmic-carnage metadata exists', () => {
    expect(store.get('sega32x-cosmic-carnage')).toBeDefined();
  });

  it('sega32x-doom metadata exists', () => {
    expect(store.get('sega32x-doom')).toBeDefined();
  });
});

describe('Game metadata — GB/GBC additional entries', () => {
  const store = new GameMetadataStore();

  it('gb-pokemon-yellow metadata exists', () => {
    expect(store.get('gb-pokemon-yellow')).toBeDefined();
  });

  it('gb-links-awakening metadata exists', () => {
    expect(store.get('gb-links-awakening')).toBeDefined();
  });

  it('gbc-zelda-oracle-of-seasons metadata exists', () => {
    expect(store.get('gbc-zelda-oracle-of-seasons')).toBeDefined();
  });

  it('gbc-zelda-oracle-of-ages metadata exists', () => {
    expect(store.get('gbc-zelda-oracle-of-ages')).toBeDefined();
  });

  it('gbc-wario-land-3 metadata exists', () => {
    expect(store.get('gbc-wario-land-3')).toBeDefined();
  });
});

// ─────────────────────────────────────────────────────────────────────────────
// Netplay whitelist
// ─────────────────────────────────────────────────────────────────────────────

describe('Netplay whitelist — PS2 new entries', () => {
  it('ps2-timesplitters-2 is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'ps2-timesplitters-2');
    expect(entry).toBeDefined();
    expect(entry?.rating).toBe('approved');
  });

  it('ps2-tekken-5 is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'ps2-tekken-5');
    expect(entry).toBeDefined();
    expect(entry?.rating).toBe('approved');
  });

  it('ps2-ssx3 is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'ps2-ssx3');
    expect(entry).toBeDefined();
    expect(entry?.rating).toBe('approved');
  });

  it('ps2-ratchet-clank-upa is caution', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'ps2-ratchet-clank-upa');
    expect(entry?.rating).toBe('caution');
  });

  it('approved PS2 games list includes timesplitters-2', () => {
    const approved = approvedGamesForSystem('ps2');
    expect(approved.find((e) => e.gameId === 'ps2-timesplitters-2')).toBeDefined();
  });
});

describe('Netplay whitelist — PS3 entries', () => {
  it('ps3-street-fighter-iv is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'ps3-street-fighter-iv');
    expect(entry?.rating).toBe('approved');
  });

  it('ps3-little-big-planet is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'ps3-little-big-planet');
    expect(entry?.rating).toBe('approved');
  });

  it('ps3-blazblue-calamity-trigger is caution', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'ps3-blazblue-calamity-trigger');
    expect(entry?.rating).toBe('caution');
  });

  it('approved PS3 games list has multiple entries', () => {
    const approved = approvedGamesForSystem('ps3');
    expect(approved.length).toBeGreaterThanOrEqual(4);
  });
});

describe('Netplay whitelist — Sega CD entries', () => {
  it('segacd-final-fight-cd is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'segacd-final-fight-cd');
    expect(entry?.rating).toBe('approved');
  });

  it('segacd-nba-jam is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'segacd-nba-jam');
    expect(entry?.rating).toBe('approved');
  });

  it('segacd-robo-aleste is caution', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'segacd-robo-aleste');
    expect(entry?.rating).toBe('caution');
  });
});

describe('Netplay whitelist — Sega 32X entries', () => {
  it('sega32x-knuckles-chaotix is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'sega32x-knuckles-chaotix');
    expect(entry?.rating).toBe('approved');
  });

  it('sega32x-cosmic-carnage is caution', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'sega32x-cosmic-carnage');
    expect(entry?.rating).toBe('caution');
  });

  it('approved 32X entries include nba-jam-te', () => {
    const approved = approvedGamesForSystem('sega32x');
    expect(approved.find((e) => e.gameId === 'sega32x-nba-jam-te')).toBeDefined();
  });
});

describe('Netplay whitelist — GB/GBC new entries', () => {
  it('gb-pokemon-yellow is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'gb-pokemon-yellow');
    expect(entry?.rating).toBe('approved');
  });

  it('gbc-pokemon-crystal is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'gbc-pokemon-crystal');
    expect(entry?.rating).toBe('approved');
  });

  it('gbc-dragon-warrior-monsters is approved', () => {
    const entry = NETPLAY_WHITELIST.find((e) => e.gameId === 'gbc-dragon-warrior-monsters');
    expect(entry?.rating).toBe('approved');
  });
});
