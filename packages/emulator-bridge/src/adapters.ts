import { KNOWN_BACKENDS } from './backends';

/**
 * A SystemAdapter provides system-specific logic for launching games,
 * determining save paths, and configuring the emulator backend.
 */
export interface SystemAdapterConfig {
  system: string;
  preferredBackendId: string;
  fallbackBackendIds: string[];
  buildLaunchArgs: (romPath: string, options: AdapterOptions) => string[];
  getSavePath: (gameId: string, baseDir: string) => string;
}

export interface AdapterOptions {
  fullscreen?: boolean;
  saveDirectory?: string;
  configPath?: string;
  playerSlot?: number;
  /** NDS-specific */
  screenLayout?: string;
  touchEnabled?: boolean;
}

/**
 * Factory to create a system adapter for a given system.
 * Each adapter encapsulates the launch arguments and save path logic
 * specific to that system's preferred emulator backend.
 */
export function createSystemAdapter(system: string): SystemAdapterConfig {
  switch (system) {
    case 'nes':
      return {
        system: 'nes',
        preferredBackendId: 'fceux',
        fallbackBackendIds: [],
        buildLaunchArgs: (romPath, options) => {
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/nes/${gameId}`,
      };

    case 'snes':
      return {
        system: 'snes',
        preferredBackendId: 'snes9x',
        fallbackBackendIds: ['bsnes'],
        buildLaunchArgs: (romPath, options) => {
          const args = [romPath];
          if (options.fullscreen) args.push('-fullscreen');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/snes/${gameId}`,
      };

    case 'gb':
    case 'gbc':
    case 'gba':
      return {
        system,
        preferredBackendId: 'mgba',
        fallbackBackendIds: system === 'gba' ? [] : ['gambatte'],
        buildLaunchArgs: (romPath, options) => {
          const args = ['-f', romPath];
          if (options.fullscreen) args.push('--fullscreen');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/${system}/${gameId}`,
      };

    case 'n64':
      return {
        system: 'n64',
        preferredBackendId: 'mupen64plus',
        fallbackBackendIds: ['project64'],
        buildLaunchArgs: (romPath, options) => {
          const args = ['--rom', romPath];
          if (options.fullscreen) args.push('--fullscreen');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/n64/${gameId}`,
      };

    case 'nds':
      return {
        system: 'nds',
        preferredBackendId: 'melonds',
        fallbackBackendIds: ['desmume'],
        buildLaunchArgs: (romPath, options) => {
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.screenLayout) args.push(`--screen-layout=${options.screenLayout}`);
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/nds/${gameId}`,
      };

    default:
      throw new Error(`Unsupported system: ${system}`);
  }
}

/**
 * Get the backend definition for a given backend ID.
 */
export function getBackendInfo(backendId: string) {
  return KNOWN_BACKENDS.find((b) => b.id === backendId) ?? null;
}
