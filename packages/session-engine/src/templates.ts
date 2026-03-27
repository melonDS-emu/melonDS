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
  // === N64 Extended library ===
  {
    id: 'n64-wave-race-64-2p',
    gameId: 'n64-wave-race-64',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,
  },
  {
    id: 'n64-star-fox-64-4p',
    gameId: 'n64-star-fox-64',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'n64-f-zero-x-4p',
    gameId: 'n64-f-zero-x',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'n64-perfect-dark-4p',
    gameId: 'n64-perfect-dark',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'n64-mario-party-4p',
    gameId: 'n64-mario-party',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'n64-bomberman-64-4p',
    gameId: 'n64-bomberman-64',
    system: 'n64',
    emulatorBackendId: 'mupen64plus',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
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

  // === GameCube ===
  {
    id: 'gc-default-2p',
    gameId: 'gc-default',
    system: 'gc',
    emulatorBackendId: 'dolphin',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'gc-default-4p',
    gameId: 'gc-default-4p',
    system: 'gc',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'gc-mario-kart-double-dash-4p',
    gameId: 'gc-mario-kart-double-dash',
    system: 'gc',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'gc-super-smash-bros-melee-4p',
    gameId: 'gc-super-smash-bros-melee',
    system: 'gc',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'gc-mario-party-4-4p',
    gameId: 'gc-mario-party-4',
    system: 'gc',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'gc-mario-party-5-4p',
    gameId: 'gc-mario-party-5',
    system: 'gc',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'gc-mario-party-6-4p',
    gameId: 'gc-mario-party-6',
    system: 'gc',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'gc-mario-party-7-4p',
    gameId: 'gc-mario-party-7',
    system: 'gc',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'gc-f-zero-gx-4p',
    gameId: 'gc-f-zero-gx',
    system: 'gc',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'gc-luigi-mansion-1p',
    gameId: 'gc-luigi-mansion',
    system: 'gc',
    emulatorBackendId: 'dolphin',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'local-only',
  },

  // === Nintendo 3DS ===
  {
    id: '3ds-default-2p',
    gameId: '3ds-default',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-default-4p',
    gameId: '3ds-default-4p',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-mario-kart-7-8p',
    gameId: '3ds-mario-kart-7',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 8,
    controllerMappings: defaultMappings(8),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-super-smash-bros-for-3ds-4p',
    gameId: '3ds-super-smash-bros-for-3ds',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-pokemon-x-2p',
    gameId: '3ds-pokemon-x',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-pokemon-y-2p',
    gameId: '3ds-pokemon-y',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-pokemon-omega-ruby-2p',
    gameId: '3ds-pokemon-omega-ruby',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-pokemon-alpha-sapphire-2p',
    gameId: '3ds-pokemon-alpha-sapphire',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-pokemon-sun-2p',
    gameId: '3ds-pokemon-sun',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-pokemon-moon-2p',
    gameId: '3ds-pokemon-moon',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-animal-crossing-new-leaf-4p',
    gameId: '3ds-animal-crossing-new-leaf',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-monster-hunter-4-ultimate-4p',
    gameId: '3ds-monster-hunter-4-ultimate',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // === Nintendo Wii ===
  {
    id: 'wii-default-2p',
    gameId: 'wii-default',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wii-default-4p',
    gameId: 'wii-default-4p',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wii-mario-kart-wii-4p',
    gameId: 'wii-mario-kart-wii',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wii-super-smash-bros-brawl-4p',
    gameId: 'wii-super-smash-bros-brawl',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wii-new-super-mario-bros-wii-4p',
    gameId: 'wii-new-super-mario-bros-wii',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wii-wii-sports-4p',
    gameId: 'wii-wii-sports',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wii-wii-sports-resort-4p',
    gameId: 'wii-wii-sports-resort',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wii-mario-party-8-4p',
    gameId: 'wii-mario-party-8',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wii-mario-party-9-4p',
    gameId: 'wii-mario-party-9',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wii-donkey-kong-country-returns-2p',
    gameId: 'wii-donkey-kong-country-returns',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wii-kirby-return-to-dream-land-4p',
    gameId: 'wii-kirby-return-to-dream-land',
    system: 'wii',
    emulatorBackendId: 'dolphin',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // =========================================================================
  // SEGA Genesis / Mega Drive — RetroArch (Genesis Plus GX core)
  // All sessions use online-relay netplay via RetroArch's built-in
  // --host / --connect flags. Recommended core: genesis_plus_gx_libretro.so
  // =========================================================================

  // --- Default / fallback templates ---
  {
    id: 'genesis-default-2p',
    gameId: 'genesis-default',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Sonic the Hedgehog 2 ---
  {
    id: 'genesis-sonic-the-hedgehog-2-2p',
    gameId: 'genesis-sonic-the-hedgehog-2',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Streets of Rage 2 ---
  {
    id: 'genesis-streets-of-rage-2-2p',
    gameId: 'genesis-streets-of-rage-2',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Mortal Kombat 3 ---
  {
    id: 'genesis-mortal-kombat-3-2p',
    gameId: 'genesis-mortal-kombat-3',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- NBA Jam ---
  {
    id: 'genesis-nba-jam-2p',
    gameId: 'genesis-nba-jam',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Contra: Hard Corps ---
  {
    id: 'genesis-contra-hard-corps-2p',
    gameId: 'genesis-contra-hard-corps',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Gunstar Heroes ---
  {
    id: 'genesis-gunstar-heroes-2p',
    gameId: 'genesis-gunstar-heroes',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Golden Axe ---
  {
    id: 'genesis-golden-axe-2p',
    gameId: 'genesis-golden-axe',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- ToeJam & Earl ---
  {
    id: 'genesis-toejam-and-earl-2p',
    gameId: 'genesis-toejam-and-earl',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Earthworm Jim 2 ---
  {
    id: 'genesis-earthworm-jim-2-2p',
    gameId: 'genesis-earthworm-jim-2',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // =========================================================================
  // SEGA Genesis — additional titles (overhauled)
  // =========================================================================

  // --- Streets of Rage 3 ---
  {
    id: 'genesis-streets-of-rage-3-2p',
    gameId: 'genesis-streets-of-rage-3',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- X-Men 2: Clone Wars ---
  {
    id: 'genesis-x-men-2-clone-wars-2p',
    gameId: 'genesis-x-men-2-clone-wars',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Road Rash II ---
  {
    id: 'genesis-road-rash-ii-2p',
    gameId: 'genesis-road-rash-ii',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Super Street Fighter II ---
  {
    id: 'genesis-super-street-fighter-ii-2p',
    gameId: 'genesis-super-street-fighter-ii',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Battletoads & Double Dragon ---
  {
    id: 'genesis-battletoads-double-dragon-2p',
    gameId: 'genesis-battletoads-double-dragon',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Mutant League Football ---
  {
    id: 'genesis-mutant-league-football-2p',
    gameId: 'genesis-mutant-league-football',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Sonic the Hedgehog 3 ---
  {
    id: 'genesis-sonic-3-2p',
    gameId: 'genesis-sonic-3',
    system: 'genesis',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // =========================================================================
  // SEGA CD / Mega-CD — RetroArch (Genesis Plus GX core)
  // Genesis Plus GX handles Sega CD disc images natively. Requires a region-
  // appropriate BIOS image in the RetroArch system directory.
  // Supported formats: .cue/.bin pairs and .chd (Compressed Hunks of Data).
  // All sessions use online-relay netplay; CD audio streams are not relayed
  // but BGM is handled locally by each client's BIOS.
  // =========================================================================

  // --- Default / fallback template ---
  {
    id: 'segacd-default-2p',
    gameId: 'segacd-default',
    system: 'segacd',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Sonic CD ---
  {
    id: 'segacd-sonic-cd-1p',
    gameId: 'segacd-sonic-cd',
    system: 'segacd',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Final Fight CD ---
  {
    id: 'segacd-final-fight-cd-2p',
    gameId: 'segacd-final-fight-cd',
    system: 'segacd',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Batman Returns ---
  {
    id: 'segacd-batman-returns-2p',
    gameId: 'segacd-batman-returns',
    system: 'segacd',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Robo Aleste ---
  {
    id: 'segacd-robo-aleste-2p',
    gameId: 'segacd-robo-aleste',
    system: 'segacd',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Lunar: The Silver Star ---
  {
    id: 'segacd-lunar-silver-star-1p',
    gameId: 'segacd-lunar-silver-star',
    system: 'segacd',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Snatcher ---
  {
    id: 'segacd-snatcher-1p',
    gameId: 'segacd-snatcher',
    system: 'segacd',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Silpheed ---
  {
    id: 'segacd-silpheed-1p',
    gameId: 'segacd-silpheed',
    system: 'segacd',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Popful Mail ---
  {
    id: 'segacd-popful-mail-1p',
    gameId: 'segacd-popful-mail',
    system: 'segacd',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // =========================================================================
  // Sega Master System — RetroArch (Genesis Plus GX core)
  // All sessions use online-relay netplay via RetroArch's built-in
  // --host / --connect flags. Recommended core: genesis_plus_gx_libretro.so
  // Note: SMS does not support dedicated online play; relay-only netplay.
  // =========================================================================

  // --- Default / fallback template ---
  {
    id: 'sms-default-2p',
    gameId: 'sms-default',
    system: 'sms',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Alex Kidd in Miracle World ---
  {
    id: 'sms-alex-kidd-in-miracle-world-1p',
    gameId: 'sms-alex-kidd-in-miracle-world',
    system: 'sms',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Sonic the Hedgehog (SMS) ---
  {
    id: 'sms-sonic-the-hedgehog-1p',
    gameId: 'sms-sonic-the-hedgehog',
    system: 'sms',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Wonder Boy III: The Dragon's Trap ---
  {
    id: 'sms-wonder-boy-iii-1p',
    gameId: 'sms-wonder-boy-iii',
    system: 'sms',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Phantasy Star ---
  {
    id: 'sms-phantasy-star-1p',
    gameId: 'sms-phantasy-star',
    system: 'sms',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Golden Axe Warrior ---
  {
    id: 'sms-golden-axe-warrior-1p',
    gameId: 'sms-golden-axe-warrior',
    system: 'sms',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- R-Type ---
  {
    id: 'sms-r-type-1p',
    gameId: 'sms-r-type',
    system: 'sms',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Shinobi ---
  {
    id: 'sms-shinobi-1p',
    gameId: 'sms-shinobi',
    system: 'sms',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- After Burner ---
  {
    id: 'sms-after-burner-1p',
    gameId: 'sms-after-burner',
    system: 'sms',
    emulatorBackendId: 'retroarch',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // =========================================================================
  // SEGA Dreamcast templates
  // =========================================================================

  // --- Dreamcast default ---
  {
    id: 'dreamcast-default',
    gameId: 'dreamcast-default',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Sonic Adventure 2 ---
  {
    id: 'dreamcast-sonic-adventure-2-2p',
    gameId: 'dc-sonic-adventure-2',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Power Stone 2 ---
  {
    id: 'dreamcast-power-stone-2-4p',
    gameId: 'dc-power-stone-2',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,  // arena fighter; transformation combos need tight sync
  },

  // --- Marvel vs. Capcom 2 ---
  {
    id: 'dreamcast-marvel-vs-capcom-2-2p',
    gameId: 'dc-marvel-vs-capcom-2',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 60,  // frame-critical tag fighter; hi-lo mix requires low latency
  },

  // --- Soul Calibur ---
  {
    id: 'dreamcast-soul-calibur-2p',
    gameId: 'dc-soul-calibur',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 60,  // 3D weapons fighter; Guard Impact timing is frame-exact
  },

  // --- Crazy Taxi ---
  {
    id: 'dreamcast-crazy-taxi-2p',
    gameId: 'dc-crazy-taxi',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,  // score-attack; latency-tolerant for casual race comparison
  },

  // --- Virtua Tennis ---
  {
    id: 'dreamcast-virtua-tennis-4p',
    gameId: 'dc-virtua-tennis',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,  // sports; swing timing tolerates moderate latency
  },

  // --- NBA 2K2 ---
  {
    id: 'dreamcast-nba-2k2-2p',
    gameId: 'dc-nba-2k2',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,  // simulation; AI branching needs reasonable sync
  },

  // --- Project Justice ---
  {
    id: 'dreamcast-project-justice-2p',
    gameId: 'dc-project-justice',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,  // 3D team fighter; Rival Break timing benefits from low latency
  },

  // --- Jet Grind Radio ---
  {
    id: 'dreamcast-jet-grind-radio-2p',
    gameId: 'dc-jet-grind-radio',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,  // tag-battle timing sensitive; trick inputs best under 80 ms
  },

  // --- Dead or Alive 2 ---
  {
    id: 'dreamcast-dead-or-alive-2-2p',
    gameId: 'dc-dead-or-alive-2',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 60,  // hold/counter timing is frame-sensitive
  },

  // --- Capcom vs. SNK ---
  {
    id: 'dreamcast-capcom-vs-snk-2p',
    gameId: 'dc-capcom-vs-snk',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,  // 2D crossover fighter; combo execution needs solid sync
  },

  // --- Ikaruga ---
  {
    id: 'dreamcast-ikaruga-2p',
    gameId: 'dc-ikaruga',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,  // co-op shooter; bullet patterns deterministic; tolerant of 100 ms
  },

  // --- Cannon Spike ---
  {
    id: 'dreamcast-cannon-spike-2p',
    gameId: 'dc-cannon-spike',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,  // arcade co-op; deterministic; tolerates moderate latency
  },

  // --- Tech Romancer ---
  {
    id: 'dreamcast-tech-romancer-2p',
    gameId: 'dc-tech-romancer',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,  // 3D mech fighter; guard cancel and Hyper Mode timing benefit from < 80 ms
  },

  // --- Toy Racer ---
  {
    id: 'dreamcast-toy-racer-4p',
    gameId: 'dc-toy-racer',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,  // kart racer; originally designed for DC online; tolerant physics
  },

  // --- Gauntlet Legends ---
  {
    id: 'dreamcast-gauntlet-legends-4p',
    gameId: 'dc-gauntlet-legends',
    system: 'dreamcast',
    emulatorBackendId: 'retroarch',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,  // co-op dungeon crawler; moderate latency tolerance
  },

  // =========================================================================
  // Sony PlayStation (PSX) templates
  // =========================================================================

  // --- PSX default ---
  {
    id: 'psx-default',
    gameId: 'psx-default',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Tekken 3 ---
  {
    id: 'psx-tekken-3-2p',
    gameId: 'psx-tekken-3',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Street Fighter Alpha 3 ---
  {
    id: 'psx-street-fighter-alpha-3-2p',
    gameId: 'psx-street-fighter-alpha-3',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Tony Hawk's Pro Skater 2 ---
  {
    id: 'psx-tony-hawks-pro-skater-2-2p',
    gameId: 'psx-tony-hawks-pro-skater-2',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Twisted Metal 2 ---
  {
    id: 'psx-twisted-metal-2-2p',
    gameId: 'psx-twisted-metal-2',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Crash Bash ---
  {
    id: 'psx-crash-bash-4p',
    gameId: 'psx-crash-bash',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Worms Armageddon ---
  {
    id: 'psx-worms-armageddon-4p',
    gameId: 'psx-worms-armageddon',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Crash Bandicoot ---
  {
    id: 'psx-crash-bandicoot-1p',
    gameId: 'psx-crash-bandicoot',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Castlevania: Symphony of the Night ---
  {
    id: 'psx-castlevania-sotn-1p',
    gameId: 'psx-castlevania-sotn',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Metal Gear Solid ---
  {
    id: 'psx-metal-gear-solid-1p',
    gameId: 'psx-metal-gear-solid',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Crash Team Racing ---
  {
    id: 'psx-crash-team-racing-4p',
    gameId: 'psx-crash-team-racing',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,
  },

  // --- Final Fantasy VII ---
  {
    id: 'psx-final-fantasy-vii-1p',
    gameId: 'psx-final-fantasy-vii',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 150,
  },

  // --- Resident Evil 2 ---
  {
    id: 'psx-resident-evil-2-1p',
    gameId: 'psx-resident-evil-2',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Spyro 2: Ripto's Rage ---
  {
    id: 'psx-spyro-2-1p',
    gameId: 'psx-spyro-2',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Gran Turismo 2 ---
  {
    id: 'psx-gran-turismo-2-2p',
    gameId: 'psx-gran-turismo-2',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,
  },

  // --- Diablo ---
  {
    id: 'psx-diablo-1p',
    gameId: 'psx-diablo',
    system: 'psx',
    emulatorBackendId: 'duckstation',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // =========================================================================
  // Sony PlayStation 2 (PS2) templates
  // =========================================================================

  // --- PS2 default ---
  {
    id: 'ps2-default',
    gameId: 'ps2-default',
    system: 'ps2',
    emulatorBackendId: 'pcsx2',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Guitar Hero II ---
  {
    id: 'ps2-guitar-hero-2-2p',
    gameId: 'ps2-guitar-hero-2',
    system: 'ps2',
    emulatorBackendId: 'pcsx2',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Burnout 3 ---
  {
    id: 'ps2-burnout-3-2p',
    gameId: 'ps2-burnout-3',
    system: 'ps2',
    emulatorBackendId: 'pcsx2',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Katamari Damacy ---
  {
    id: 'ps2-katamari-damacy-2p',
    gameId: 'ps2-katamari-damacy',
    system: 'ps2',
    emulatorBackendId: 'pcsx2',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Ratchet & Clank: Up Your Arsenal ---
  {
    id: 'ps2-ratchet-clank-upa-2p',
    gameId: 'ps2-ratchet-clank-upa',
    system: 'ps2',
    emulatorBackendId: 'pcsx2',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Need for Speed: Underground 2 ---
  {
    id: 'ps2-need-for-speed-ug2-2p',
    gameId: 'ps2-need-for-speed-ug2',
    system: 'ps2',
    emulatorBackendId: 'pcsx2',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- WWE SmackDown! vs. RAW ---
  {
    id: 'ps2-wwe-smackdown-vs-raw-2p',
    gameId: 'ps2-wwe-smackdown-vs-raw',
    system: 'ps2',
    emulatorBackendId: 'pcsx2',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Kingdom Hearts ---
  {
    id: 'ps2-kingdom-hearts-1p',
    gameId: 'ps2-kingdom-hearts',
    system: 'ps2',
    emulatorBackendId: 'pcsx2',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- Shadow of the Colossus ---
  {
    id: 'ps2-shadow-of-the-colossus-1p',
    gameId: 'ps2-shadow-of-the-colossus',
    system: 'ps2',
    emulatorBackendId: 'pcsx2',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // --- GTA: San Andreas ---
  {
    id: 'ps2-gta-san-andreas-1p',
    gameId: 'ps2-gta-san-andreas',
    system: 'ps2',
    emulatorBackendId: 'pcsx2',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },

  // =========================================================================
  // Sony PlayStation Portable (PSP) templates
  // =========================================================================

  // --- PSP default ---
  {
    id: 'psp-default',
    gameId: 'psp-default',
    system: 'psp',
    emulatorBackendId: 'ppsspp',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Monster Hunter Freedom Unite ---
  {
    id: 'psp-monster-hunter-fu-4p',
    gameId: 'psp-monster-hunter-fu',
    system: 'psp',
    emulatorBackendId: 'ppsspp',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Metal Gear Solid: Peace Walker ---
  {
    id: 'psp-peace-walker-4p',
    gameId: 'psp-peace-walker',
    system: 'psp',
    emulatorBackendId: 'ppsspp',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Patapon ---
  {
    id: 'psp-patapon-4p',
    gameId: 'psp-patapon',
    system: 'psp',
    emulatorBackendId: 'ppsspp',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- WipEout Pulse ---
  {
    id: 'psp-wipeout-pulse-8p',
    gameId: 'psp-wipeout-pulse',
    system: 'psp',
    emulatorBackendId: 'ppsspp',
    playerCount: 8,
    controllerMappings: defaultMappings(8),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Tekken: Dark Resurrection ---
  {
    id: 'psp-tekken-dark-resurrection-2p',
    gameId: 'psp-tekken-dark-resurrection',
    system: 'psp',
    emulatorBackendId: 'ppsspp',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- GTA: Vice City Stories ---
  {
    id: 'psp-gta-vice-city-stories-2p',
    gameId: 'psp-gta-vice-city-stories',
    system: 'psp',
    emulatorBackendId: 'ppsspp',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Lumines ---
  {
    id: 'psp-lumines-2p',
    gameId: 'psp-lumines',
    system: 'psp',
    emulatorBackendId: 'ppsspp',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- God of War: Chains of Olympus ---
  {
    id: 'psp-god-of-war-chains-1p',
    gameId: 'psp-god-of-war-chains',
    system: 'psp',
    emulatorBackendId: 'ppsspp',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // --- Crisis Core: Final Fantasy VII ---
  {
    id: 'psp-crisis-core-ff7-1p',
    gameId: 'psp-crisis-core-ff7',
    system: 'psp',
    emulatorBackendId: 'ppsspp',
    playerCount: 1,
    controllerMappings: defaultMappings(1),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },

  // === Nintendo Wii U ===
  {
    id: 'wiiu-default-2p',
    gameId: 'wiiu-default',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-default-4p',
    gameId: 'wiiu-default-4p',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-mario-kart-8-4p',
    gameId: 'wiiu-mario-kart-8',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-super-smash-bros-wiiu-4p',
    gameId: 'wiiu-super-smash-bros-wiiu',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-super-smash-bros-wiiu-8p',
    gameId: 'wiiu-super-smash-bros-wiiu',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 8,
    controllerMappings: defaultMappings(8),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-new-super-mario-bros-u-4p',
    gameId: 'wiiu-new-super-mario-bros-u',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-nintendo-land-4p',
    gameId: 'wiiu-nintendo-land',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-splatoon-4p',
    gameId: 'wiiu-splatoon',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-pikmin-3-2p',
    gameId: 'wiiu-pikmin-3',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-mario-party-10-4p',
    gameId: 'wiiu-mario-party-10',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-super-mario-3d-world-4p',
    gameId: 'wiiu-super-mario-3d-world',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'wiiu-hyrule-warriors-2p',
    gameId: 'wiiu-hyrule-warriors',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-star-fox-zero-2p',
    gameId: 'wiiu-star-fox-zero',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'wiiu-yoshis-woolly-world-2p',
    gameId: 'wiiu-yoshis-woolly-world',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'wiiu-xenoblade-chronicles-x-4p',
    gameId: 'wiiu-xenoblade-chronicles-x',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 150,
  },
  {
    id: 'wiiu-captain-toad-2p',
    gameId: 'wiiu-captain-toad-treasure-tracker',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'wiiu-kirby-rainbow-curse-4p',
    gameId: 'wiiu-kirby-rainbow-curse',
    system: 'wiiu',
    emulatorBackendId: 'cemu',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  // === Nintendo 3DS (expanded) ===
  {
    id: '3ds-luigis-mansion-dark-moon-4p',
    gameId: '3ds-luigis-mansion-dark-moon',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: '3ds-kirby-triple-deluxe-4p',
    gameId: '3ds-kirby-triple-deluxe',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: '3ds-fire-emblem-awakening-2p',
    gameId: '3ds-fire-emblem-awakening',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 150,
  },
  {
    id: '3ds-tomodachi-life-4p',
    gameId: '3ds-tomodachi-life',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 150,
  },
  {
    id: '3ds-pokemon-ultra-sun-2p',
    gameId: '3ds-pokemon-ultra-sun',
    system: '3ds',
    emulatorBackendId: 'citra',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 150,
  },
  // === Nintendo Switch ===
  {
    id: 'switch-default-2p',
    gameId: 'switch-default',
    system: 'switch',
    emulatorBackendId: 'ryujinx',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'switch-default-4p',
    gameId: 'switch-default-4p',
    system: 'switch',
    emulatorBackendId: 'ryujinx',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 100,
  },
  {
    id: 'switch-mario-kart-8-deluxe-4p',
    gameId: 'switch-mario-kart-8-deluxe',
    system: 'switch',
    emulatorBackendId: 'ryujinx',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,
  },
  {
    id: 'switch-super-smash-bros-ultimate-4p',
    gameId: 'switch-super-smash-bros-ultimate',
    system: 'switch',
    emulatorBackendId: 'ryujinx',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,
  },
  {
    id: 'switch-super-smash-bros-ultimate-8p',
    gameId: 'switch-super-smash-bros-ultimate',
    system: 'switch',
    emulatorBackendId: 'ryujinx',
    playerCount: 8,
    controllerMappings: defaultMappings(8),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,
  },
  {
    id: 'switch-splatoon-3-4p',
    gameId: 'switch-splatoon-3',
    system: 'switch',
    emulatorBackendId: 'ryujinx',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: NO_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 80,
  },
  {
    id: 'switch-pokemon-scarlet-2p',
    gameId: 'switch-pokemon-scarlet',
    system: 'switch',
    emulatorBackendId: 'ryujinx',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'switch-pokemon-violet-2p',
    gameId: 'switch-pokemon-violet',
    system: 'switch',
    emulatorBackendId: 'ryujinx',
    playerCount: 2,
    controllerMappings: defaultMappings(2),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 120,
  },
  {
    id: 'switch-animal-crossing-4p',
    gameId: 'switch-animal-crossing-new-horizons',
    system: 'switch',
    emulatorBackendId: 'ryujinx',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: DEFAULT_SAVE_RULES,
    netplayMode: 'online-relay',
    latencyTarget: 150,
  },
  {
    id: 'switch-mario-party-superstars-4p',
    gameId: 'switch-mario-party-superstars',
    system: 'switch',
    emulatorBackendId: 'ryujinx',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
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

  /**
   * List all templates for a specific system.
   */
  getAllForSystem(system: string): SessionTemplateConfig[] {
    return Array.from(this.templates.values()).filter((t) => t.system === system);
  }
}
