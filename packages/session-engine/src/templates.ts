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
  };
}

/** Default save rules shared by most Phase 1 games. */
const DEFAULT_SAVE_RULES: SessionTemplateConfig['saveRules'] = {
  autoSave: true,
  autoSaveIntervalSeconds: 60,
  allowSaveStates: true,
  syncSavesToCloud: false,
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
  {
    id: 'snes-super-bomberman-4p',
    gameId: 'snes-super-bomberman',
    system: 'snes',
    emulatorBackendId: 'snes9x',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: { ...DEFAULT_SAVE_RULES, autoSave: false, allowSaveStates: false },
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
  // === GBA Zelda Four Swords — 4-player link co-op ===
  {
    id: 'gba-zelda-four-swords-4p',
    gameId: 'gba-zelda-four-swords',
    system: 'gba',
    emulatorBackendId: 'mgba',
    playerCount: 4,
    controllerMappings: defaultMappings(4),
    saveRules: { ...DEFAULT_SAVE_RULES, autoSave: false, allowSaveStates: false },
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
    saveRules: { ...DEFAULT_SAVE_RULES, autoSave: false, allowSaveStates: false },
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
    saveRules: { ...DEFAULT_SAVE_RULES, autoSave: false, allowSaveStates: false },
    netplayMode: 'online-relay',
    latencyTarget: 120,
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
    saveRules: { ...DEFAULT_SAVE_RULES, autoSave: false, allowSaveStates: false },
    netplayMode: 'online-relay',
    latencyTarget: 120,
    uiLayoutOptions: { dsScreenLayout: 'stacked' },
    wfcConfig: {
      dnsServer: '178.62.43.212',
      description: 'Uses Wiimmfi for Mario Kart DS online racing',
    },
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
