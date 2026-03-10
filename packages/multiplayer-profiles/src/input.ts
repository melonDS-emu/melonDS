export type ControllerType = 'keyboard' | 'xbox' | 'playstation' | 'generic' | 'unknown';

export interface InputBinding {
  action: string;
  key?: string;
  button?: string;
  axis?: string;
}

export interface InputProfile {
  id: string;
  name: string;
  controllerType: ControllerType;
  system: string;
  bindings: InputBinding[];
  isDefault: boolean;
}

/**
 * InputProfileManager handles controller mapping and input configuration.
 * It provides per-game and per-system templates for unified input handling.
 */
export class InputProfileManager {
  private profiles: Map<string, InputProfile> = new Map();

  /**
   * Get an input profile by ID.
   */
  get(id: string): InputProfile | null {
    return this.profiles.get(id) ?? null;
  }

  /**
   * Get the default profile for a system and controller type.
   */
  getDefault(system: string, controllerType: ControllerType): InputProfile | null {
    for (const profile of this.profiles.values()) {
      if (
        profile.system === system &&
        profile.controllerType === controllerType &&
        profile.isDefault
      ) {
        return profile;
      }
    }
    return null;
  }

  /**
   * Save an input profile.
   */
  save(profile: InputProfile): void {
    this.profiles.set(profile.id, profile);
  }

  /**
   * Delete an input profile.
   */
  delete(id: string): boolean {
    return this.profiles.delete(id);
  }

  /**
   * List all profiles for a system.
   */
  listForSystem(system: string): InputProfile[] {
    return Array.from(this.profiles.values()).filter((p) => p.system === system);
  }

  /**
   * List all profiles.
   */
  listAll(): InputProfile[] {
    return Array.from(this.profiles.values());
  }
}
