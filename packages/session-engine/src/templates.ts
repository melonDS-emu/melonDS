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

/**
 * SessionTemplateStore manages per-game session configurations.
 * Templates define how a game should be launched in a multiplayer session.
 */
export class SessionTemplateStore {
  private templates: Map<string, SessionTemplateConfig> = new Map();

  /**
   * Get a session template by ID.
   */
  get(id: string): SessionTemplateConfig | null {
    return this.templates.get(id) ?? null;
  }

  /**
   * Get the default template for a game.
   */
  getForGame(gameId: string): SessionTemplateConfig | null {
    for (const template of this.templates.values()) {
      if (template.gameId === gameId) return template;
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
