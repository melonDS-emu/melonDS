export type MultiplayerMode = 'local' | 'online' | 'hybrid' | 'link' | 'trade' | 'battle';

export type CompatibilityBadge =
  | 'Great Online'
  | 'Good Local'
  | 'Best with Friends'
  | 'Link Mode'
  | 'Experimental'
  | 'Host Only Recommended'
  | 'Touch Heavy'
  | 'Party Favorite';

export interface MultiplayerProfile {
  id: string;
  gameId: string;
  maxPlayers: number;
  mode: MultiplayerMode;
  recommendedBackend: string;
  notes: string[];
  supportsPublicLobby: boolean;
  supportsPrivateLobby: boolean;
  badges: CompatibilityBadge[];
}

/**
 * MultiplayerProfileStore manages per-game multiplayer configuration and metadata.
 * This drives the lobby system's knowledge of what multiplayer modes a game supports.
 */
export class MultiplayerProfileStore {
  private profiles: Map<string, MultiplayerProfile> = new Map();

  get(id: string): MultiplayerProfile | null {
    return this.profiles.get(id) ?? null;
  }

  getForGame(gameId: string): MultiplayerProfile | null {
    for (const profile of this.profiles.values()) {
      if (profile.gameId === gameId) return profile;
    }
    return null;
  }

  save(profile: MultiplayerProfile): void {
    this.profiles.set(profile.id, profile);
  }

  delete(id: string): boolean {
    return this.profiles.delete(id);
  }

  listAll(): MultiplayerProfile[] {
    return Array.from(this.profiles.values());
  }

  /**
   * Get all games that support a specific multiplayer mode.
   */
  getByMode(mode: MultiplayerMode): MultiplayerProfile[] {
    return Array.from(this.profiles.values()).filter((p) => p.mode === mode);
  }

  /**
   * Get all games with a specific badge.
   */
  getByBadge(badge: CompatibilityBadge): MultiplayerProfile[] {
    return Array.from(this.profiles.values()).filter((p) => p.badges.includes(badge));
  }
}
