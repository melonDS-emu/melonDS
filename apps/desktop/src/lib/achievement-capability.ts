/**
 * Achievement capability registry — maps each SupportedSystem to its
 * RetroAchievements.org support level.
 */

export type AchievementSupportLevel = 'full' | 'partial' | 'none';

export interface AchievementCapability {
  system: string;
  level: AchievementSupportLevel;
  /** Human-readable notes, e.g. "Supported via RetroArch+mGBA" */
  notes: string;
}

export const ACHIEVEMENT_CAPABILITIES: Record<string, AchievementCapability> = {
  nes: {
    system: 'nes',
    level: 'full',
    notes: 'Supported via RetroArch+FCEUmm',
  },
  snes: {
    system: 'snes',
    level: 'full',
    notes: 'Supported via RetroArch+Snes9x',
  },
  gb: {
    system: 'gb',
    level: 'full',
    notes: 'Supported via RetroArch+Gambatte',
  },
  gbc: {
    system: 'gbc',
    level: 'full',
    notes: 'Supported via RetroArch+Gambatte',
  },
  gba: {
    system: 'gba',
    level: 'full',
    notes: 'Supported via RetroArch+mGBA',
  },
  n64: {
    system: 'n64',
    level: 'full',
    notes: 'Supported via RetroArch+Mupen64Plus',
  },
  psx: {
    system: 'psx',
    level: 'full',
    notes: 'Supported via RetroArch+PCSX-ReARMed',
  },
  sms: {
    system: 'sms',
    level: 'full',
    notes: 'Supported via RetroArch+Genesis Plus GX',
  },
  genesis: {
    system: 'genesis',
    level: 'full',
    notes: 'Supported via RetroArch+Genesis Plus GX',
  },
  nds: {
    system: 'nds',
    level: 'partial',
    notes: 'Limited support via RetroArch+melonDS — not all games have achievements',
  },
  dreamcast: {
    system: 'dreamcast',
    level: 'partial',
    notes: 'Partial support via RetroArch+Flycast — growing library',
  },
  ps2: {
    system: 'ps2',
    level: 'partial',
    notes: 'Partial support via RetroArch+PCSX2 — limited game coverage',
  },
  psp: {
    system: 'psp',
    level: 'partial',
    notes: 'Partial support via RetroArch+PPSSPP — limited game coverage',
  },
  gc: {
    system: 'gc',
    level: 'none',
    notes: 'GameCube not supported by RetroAchievements.org',
  },
  wii: {
    system: 'wii',
    level: 'partial',
    notes: 'Partial support via RetroArch+Dolphin — select titles only',
  },
  wiiu: {
    system: 'wiiu',
    level: 'none',
    notes: 'Wii U not supported by RetroAchievements.org',
  },
  '3ds': {
    system: '3ds',
    level: 'none',
    notes: '3DS not supported by RetroAchievements.org',
  },
  switch: {
    system: 'switch',
    level: 'none',
    notes: 'Nintendo Switch not supported by RetroAchievements.org',
  },
};

export function getAchievementCapability(system: string): AchievementCapability {
  const key = system.toLowerCase();
  return (
    ACHIEVEMENT_CAPABILITIES[key] ?? {
      system: key,
      level: 'none',
      notes: 'No achievement support for this system',
    }
  );
}

/** Returns true if the system has full or partial achievement support. */
export function systemSupportsAchievements(system: string): boolean {
  const cap = getAchievementCapability(system);
  return cap.level === 'full' || cap.level === 'partial';
}
