export interface SessionTemplateConfig {
  id: string;
  gameId: string;
  system: string;
  emulatorBackendId: string;
  playerCount: number;
  controllerMappings: { slot: number; profile: string }[];
  saveRules: {
    autoSave: boolean;
    autoSaveIntervalSeconds?: number;
    allowSaveStates: boolean;
    syncSavesToCloud: boolean;
  };
  netplayMode: 'local-only' | 'online-p2p' | 'online-relay' | 'hybrid';
  latencyTarget?: number;
  uiLayoutOptions?: {
    dsScreenLayout?: 'stacked' | 'side-by-side' | 'top-focus' | 'bottom-focus';
    dsScreenSwap?: boolean;
  };
  /**
   * WFC / Pokémon Online configuration.
   * When set, the emulator should be launched with WFC relay support.
   */
  wfcConfig?: {
    /** The DNS server to point the emulator at for WFC authentication. */
    dnsServer: string;
    /** Human-readable description of the WFC setup. */
    description: string;
    /**
     * Stable ID of the WFC provider (e.g. 'wiimmfi', 'altwfc').
     * Used by the UI to let players switch providers without touching DNS.
     * Defaults to 'wiimmfi' when omitted.
     */
    providerId?: string;
  };
  /**
   * Mario Kart DS matchmaking mode tag.
   * Used by the MarioKartPage to bucket rooms into Quick / Public / Ranked.
   */
  kartMode?: 'quick' | 'public' | 'ranked';
  /**
   * DSi mode — when true, melonDS is launched with --dsi-mode.
   * Requires DSi BIOS files (bios7i.bin, bios9i.bin, nand.bin).
   */
  dsiMode?: boolean;
}

/** Default save rules shared by most Phase 1 games. */
const DEFAULT_SAVE_RULES: SessionTemplateConfig['saveRules'] = {
  autoSave: true,
  autoSaveIntervalSeconds: 60,
  allowSaveStates: true,
  syncSavesToCloud: false,
};

/** Save rules for competitive / arcade sessions where saves are not meaningful. */
const NO_SAVE_RULES: SessionTemplateConfig['saveRules'] = {
  ...DEFAULT_SAVE_RULES,
  autoSave: false,
  allowSaveStates: false,
};

/** Build default controller mappings for N players. */
function defaultMappings(count: number): { slot: number; profile: string }[] {
  return Array.from({ length: count }, (_, i) => ({ slot: i, profile: 'default' }));
}

/**
 * Default session templates for Phase 1 systems.
 * These provide out-of-the-box multiplayer configurations.
 */
export const DEFAULT_TEMPLATES: SessionTemplateConfig[] = [
  // === NES ===
  {
    id: 'nes-default-2p',
    gameId: 'nes-default',
    system: 'nes',
    emulatorBackendId: 'fceux',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  // === SNES ===
  {
    id: 'snes-default-2p',
    gameId: 'snes-default',
    system: 'snes',
    emulatorBackendId: 'snes9x',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'snes-default-4p',
    gameId: 'snes-default-4p',
    system: 'snes',
    emulatorBackendId: 'snes9x',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  // === Game Boy ===
  {
    id: 'gb-default-2p',
    gameId: 'gb-default',
    system: 'gb',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-p2p',
    latencyTarget: 80,
  },
  // === Game Boy Color ===
  {
    id: 'gbc-default-2p',
    gameId: 'gbc-default',
    system: 'gbc',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-p2p',
    latencyTarget: 80,
  },
  // === Game Boy Advance ===
  {
    id: 'gba-default-2p',
    gameId: 'gba-default',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-p2p',
    latencyTarget: 80,
  },
  // === Specific popular games ===
  // --- NES classics ---
  {
    id: 'nes-super-mario-bros-2p',
    gameId: 'nes-super-mario-bros',
    system: 'nes',
    emulatorBackendId: 'fceux',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'nes-contra-2p',
    gameId: 'nes-contra',
    system: 'nes',
    emulatorBackendId: 'fceux',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'nes-double-dragon-2p',
    gameId: 'nes-double-dragon',
    system: 'nes',
    emulatorBackendId: 'fceux',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'nes-tecmo-super-bowl-2p',
    gameId: 'nes-tecmo-super-bowl',
    system: 'nes',
    emulatorBackendId: 'fceux',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  // --- SNES classics ---
  {
    id: 'snes-super-bomberman-4p',
    gameId: 'snes-super-bomberman',
    system: 'snes',
    emulatorBackendId: 'snes9x',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'snes-street-fighter-2-2p',
    gameId: 'snes-street-fighter-2',
    system: 'snes',
    emulatorBackendId: 'snes9x',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'snes-donkey-kong-country-2p',
    gameId: 'snes-donkey-kong-country',
    system: 'snes',
    emulatorBackendId: 'snes9x',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'snes-kirby-super-star-4p',
    gameId: 'snes-kirby-super-star',
    system: 'snes',
    emulatorBackendId: 'snes9x',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'snes-super-mario-kart-2p',
    gameId: 'snes-super-mario-kart',
    system: 'snes',
    emulatorBackendId: 'snes9x',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  // === GB Pokémon Gen I — link cable presets (mGBA primary, SameBoy alternative) ===
  {
    id: 'gb-pokemon-red-2p',
    gameId: 'gb-pokemon-red',
    system: 'gb',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-p2p',
    latencyTarget: 80,
  },
  {
    id: 'gb-pokemon-blue-2p',
    gameId: 'gb-pokemon-blue',
    system: 'gb',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-p2p',
    latencyTarget: 80,
  },
  {
    id: 'gb-pokemon-yellow-2p',
    gameId: 'gb-pokemon-yellow',
    system: 'gb',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-p2p',
    latencyTarget: 80,
  },
  // === GBA Pokémon Gen III — link cable presets ===
  {
    id: 'gba-pokemon-firered-2p',
    gameId: 'gba-pokemon-firered',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-p2p',
    latencyTarget: 100,
  },
  {
    id: 'gba-pokemon-leafgreen-2p',
    gameId: 'gba-pokemon-leafgreen',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-p2p',
    latencyTarget: 100,
  },
  {
    id: 'gba-pokemon-ruby-2p',
    gameId: 'gba-pokemon-ruby',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-p2p',
    latencyTarget: 100,
  },
  {
    id: 'gba-pokemon-sapphire-2p',
    gameId: 'gba-pokemon-sapphire',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-p2p',
    latencyTarget: 100,
  },
  {
    id: 'gba-pokemon-emerald-2p',
    gameId: 'gba-pokemon-emerald',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-p2p',
    latencyTarget: 100,
  },
  // === GBA Zelda Four Swords — 4-player link co-op ===
  {
    id: 'gba-zelda-four-swords-4p',
    gameId: 'gba-zelda-four-swords',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'gbc-pokemon-crystal-2p',
    gameId: 'gbc-pokemon-crystal',
    system: 'gbc',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-p2p',
    latencyTarget: 100,
  },
  // === Nintendo 64 (Phase 2) ===
  {
    id: 'n64-default-2p',
    gameId: 'n64-default',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'n64-default-4p',
    gameId: 'n64-default-4p',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 150,
  },
  {
    id: 'n64-mario-kart-64-4p',
    gameId: 'n64-mario-kart-64',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 150,
  },
  {
    id: 'n64-smash-bros-4p',
    gameId: 'n64-super-smash-bros',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'n64-mario-party-2-4p',
    gameId: 'n64-mario-party-2',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 150,
  },
  {
    id: 'n64-goldeneye-007-4p',
    gameId: 'n64-goldeneye-007',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'n64-diddy-kong-racing-4p',
    gameId: 'n64-diddy-kong-racing',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 150,
  },
  // === N64 Mario Sports games ===
  {
    id: 'n64-mario-tennis-4p',
    gameId: 'n64-mario-tennis',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'n64-mario-golf-4p',
    gameId: 'n64-mario-golf',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  // === N64 Pokémon games ===
  {
    id: 'n64-pokemon-stadium-4p',
    gameId: 'n64-pokemon-stadium',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'n64-pokemon-stadium-2-4p',
    gameId: 'n64-pokemon-stadium-2',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'n64-pokemon-puzzle-league-2p',
    gameId: 'n64-pokemon-puzzle-league',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'n64-pokemon-snap-1p',
    gameId: 'n64-pokemon-snap',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'local-only',
  },
  // === Nintendo DS (Phase 3) ===
  {
    id: 'nds-default-2p',
    gameId: 'nds-default',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
  },
  {
    id: 'nds-default-4p',
    gameId: 'nds-default-4p',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
  },
  // === NDS Pokémon games — WFC / Pokémon Online presets ===
  {
    id: 'nds-pokemon-diamond-2p',
    gameId: 'nds-pokemon-diamond',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212', // Wiimmfi WFC replacement
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for Pokémon Diamond online play (GTS, Battle Tower, Union Room)',
    },
  },
  {
    id: 'nds-pokemon-pearl-2p',
    gameId: 'nds-pokemon-pearl',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for Pokémon Pearl online play (GTS, Battle Tower, Union Room)',
    },
  },
  {
    id: 'nds-pokemon-platinum-2p',
    gameId: 'nds-pokemon-platinum',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for Pokémon Platinum online play (GTS, Battle Tower, Union Room)',
    },
  },
  {
    id: 'nds-pokemon-hgss-2p',
    gameId: 'nds-pokemon-hgss',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for HeartGold/SoulSilver online play',
    },
  },
  // === Mario Kart DS (WFC) ===
  {
    id: 'nds-mario-kart-ds-4p',
    gameId: 'nds-mario-kart-ds',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    kartMode: 'public',
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for Mario Kart DS online racing',
    },
  },
  // Mario Kart DS — Quick Race (2-player, auto-matched)
  {
    id: 'nds-mario-kart-ds-quick-2p',
    gameId: 'nds-mario-kart-ds',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    kartMode: 'quick',
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Quick Race — auto-matched 1v1 via Wiimmfi',
    },
  },
  // Mario Kart DS — Ranked Race (4-player, skill-matched)
  {
    id: 'nds-mario-kart-ds-ranked-4p',
    gameId: 'nds-mario-kart-ds',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    kartMode: 'ranked',
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Ranked Race — skill-matched 4-player race via Wiimmfi',
    },
  },
  // === New Super Mario Bros. DS ===
  {
    id: 'nds-new-super-mario-bros-4p',
    gameId: 'nds-new-super-mario-bros',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
  },
  // === Metroid Prime Hunters — competitive 2P / 4P FPS ===
  {
    id: 'nds-metroid-prime-hunters-4p',
    gameId: 'nds-metroid-prime-hunters',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for Metroid Prime Hunters online deathmatch',
    },
  },
  // === Tetris DS — 4P head-to-head ===
  {
    id: 'nds-tetris-ds-4p',
    gameId: 'nds-tetris-ds',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for Tetris DS online versus',
    },
  },
  // === Mario Party DS — 4P mini-games ===
  {
    id: 'nds-mario-party-ds-4p',
    gameId: 'nds-mario-party-ds',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 130,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
  },
  // === The Legend of Zelda: Phantom Hourglass — 2P battle mode ===
  {
    id: 'nds-zelda-phantom-hourglass-2p',
    gameId: 'nds-zelda-phantom-hourglass',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for Phantom Hourglass Battle Mode online',
    },
  },
  // === Pokémon Black / White — WFC battle & trade ===
  {
    id: 'nds-pokemon-black-2p',
    gameId: 'nds-pokemon-black',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for Pokémon Black online play (GTS, Battle Subway, C-Gear)',
    },
  },
  {
    id: 'nds-pokemon-white-2p',
    gameId: 'nds-pokemon-white',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: { ...DEFAULT_SAVE_RULES, syncSavesToCloud: true },
    netplayMode: 'online-relay',
    latencyTarget: 100,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for Pokémon White online play (GTS, Battle Subway, C-Gear)',
    },
  },
  // === NDS Mario Sports games ===
  {
    id: 'nds-mario-hoops-3on3-4p',
    gameId: 'nds-mario-hoops-3on3',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
  },
  {
    id: 'nds-mario-and-sonic-olympic-games-4p',
    gameId: 'nds-mario-and-sonic-olympic-games',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi WFC for Mario & Sonic at the Olympic Games online events',
    },
  },
  // === GBA Mario Sports games ===
  {
    id: 'gba-mario-tennis-power-tour-4p',
    gameId: 'gba-mario-tennis-power-tour',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-p2p',
    latencyTarget: 100,
  },
  {
    id: 'gba-mario-golf-advance-tour-4p',
    gameId: 'gba-mario-golf-advance-tour',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-p2p',
    latencyTarget: 100,
  },
  // === DSiWare ===
  {
    id: 'dsiware-flipnote-studio-1p',
    gameId: 'dsiware-flipnote-studio',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'local-only',
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    dsiMode: true,
  },
  {
    id: 'dsiware-dr-mario-express-2p',
    gameId: 'dsiware-dr-mario-express',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    dsiMode: true,
  },
  {
    id: 'dsiware-art-academy-1p',
    gameId: 'dsiware-art-academy',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'local-only',
    uiLayoutOptions: { dsScreenLayout: 'bottom-focus' },
    dsiMode: true,
  },
  {
    id: 'dsiware-puzzle-league-2p',
    gameId: 'dsiware-puzzle-league',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    dsiMode: true,
  },

  // ===========================================================================
  // Phase 14 — Zelda & Metroid Online
  // ===========================================================================

  // === Metroid Prime Hunters — quick 1-on-1 (Phase 14) ===
  {
    id: 'nds-metroid-prime-hunters-2p',
    gameId: 'nds-metroid-prime-hunters',
    system: 'nds',
    emulatorBackendId: 'melonds',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,
    uiLayoutOptions: { dsScreenLayout: 'top-focus' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      providerId: 'wiimmfi',
      description: 'Uses Wiimmfi for Metroid Prime Hunters 1v1 deathmatch',
    },
  },

  // === GBA Zelda Four Swords — 2P variant ===
  {
    id: 'gba-zelda-four-swords-2p',
    gameId: 'gba-zelda-four-swords',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
];

/**
 * SessionTemplateStore manages per-game session configurations.
 * Templates define how a game should be launched in a multiplayer session.
 */
export class SessionTemplateStore {
  private templates: Map<string, SessionTemplateConfig> = new Map();

  constructor(loadDefaults = true) {
    if (loadDefaults) {
      for (const tpl of DEFAULT_TEMPLATES) {
        this.templates.set(tpl.id, tpl);
      }
    }
  }

  /**
   * Get a session template by ID.
   */
  get(id: string): SessionTemplateConfig | null {
    return this.templates.get(id) ?? null;
  }

  /**
   * Get the default template for a game, falling back to the system default.
   */
  getForGame(gameId: string, system?: string): SessionTemplateConfig | null {
    for (const template of this.templates.values()) {
      if (template.gameId === gameId) return template;
    }
    // Fall back to system default template
    if (system) {
      for (const template of this.templates.values()) {
        if (template.gameId === `${system}-default`) return template;
      }
    }
    return null;
  }

  /**
   * Create or update a session template.
   */
  save(template: SessionTemplateConfig): void {
    this.templates.set(template.id, template);
  }

  /**
   * Delete a session template.
   */
  delete(id: string): boolean {
    return this.templates.delete(id);
  }

  /**
   * List all templates.
   */
  listAll(): SessionTemplateConfig[] {
    return Array.from(this.templates.values());
  }
}
