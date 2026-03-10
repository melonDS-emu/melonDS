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
