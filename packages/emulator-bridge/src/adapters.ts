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
  /** Netplay relay or peer host to connect to */
  netplayHost?: string;
  /** Netplay relay or peer port */
  netplayPort?: number;
  /** NDS-specific */
  screenLayout?: string;
  touchEnabled?: boolean;
}

/**
 * Factory to create a system adapter for a given system.
 * Each adapter encapsulates the launch arguments and save path logic
 * specific to that system's preferred emulator backend.
 *
 * Netplay / link cable argument conventions:
 *  - FCEUX:               --net <host>:<port> --player <slot>
 *  - Snes9x:              -netplay <host> <port>
 *  - mGBA:                --link-host <host>:<port>  (link cable over TCP)
 *  - SameBoy:             --link-address <host>:<port>  (link cable over TCP)
 *  - VisualBoyAdvance-M:  --link-host <host>:<port>  (link cable over TCP)
 *  - Mupen64Plus:         --netplay-host <host> --netplay-port <port>
 *  - melonDS:             --wifi-host <host> --wifi-port <port>
 *
 * @param system    The system identifier (e.g. 'gb', 'gba', 'n64').
 * @param backendId Optional backend override. When provided, launch args are
 *                  tailored to that backend's CLI conventions. Defaults to the
 *                  system's preferred backend.
 */
export function createSystemAdapter(system: string, backendId?: string): SystemAdapterConfig {
  switch (system) {
    case 'nes':
      return {
        system: 'nes',
        preferredBackendId: 'fceux',
        fallbackBackendIds: [],
        buildLaunchArgs: (romPath, options) => {
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.netplayHost && options.netplayPort) {
            args.push('--net', `${options.netplayHost}:${options.netplayPort}`);
            if (options.playerSlot !== undefined) {
              args.push('--player', String(options.playerSlot + 1));
            }
          }
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
          if (options.netplayHost && options.netplayPort) {
            args.push('-netplay', options.netplayHost, String(options.netplayPort));
          }
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/snes/${gameId}`,
      };

    case 'gb':
    case 'gbc':
    case 'gba': {
      const effectiveBackend = backendId ?? 'mgba';
      return {
        system,
        preferredBackendId: 'mgba',
        fallbackBackendIds: system === 'gba' ? ['vbam'] : ['sameboy', 'gambatte'],
        buildLaunchArgs: (romPath, options) => {
          if (effectiveBackend === 'sameboy') {
            // SameBoy: link cable over TCP via --link-address <host:port>
            const args = [romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.netplayHost && options.netplayPort) {
              args.push('--link-address', `${options.netplayHost}:${options.netplayPort}`);
            }
            return args;
          } else if (effectiveBackend === 'vbam') {
            // VisualBoyAdvance-M: link cable over TCP via --link-host <host:port>
            const args = [romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.netplayHost && options.netplayPort) {
              args.push('--link-host', `${options.netplayHost}:${options.netplayPort}`);
            }
            return args;
          } else {
            // mGBA (default): link cable over TCP via --link-host <host:port>
            const args = ['-f', romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.netplayHost && options.netplayPort) {
              args.push('--link-host', `${options.netplayHost}:${options.netplayPort}`);
            }
            return args;
          }
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/${system}/${gameId}`,
      };
    }

    case 'n64':
      return {
        system: 'n64',
        preferredBackendId: 'mupen64plus',
        fallbackBackendIds: ['project64'],
        buildLaunchArgs: (romPath, options) => {
          const args = ['--rom', romPath];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.netplayHost && options.netplayPort) {
            args.push('--netplay-host', options.netplayHost);
            args.push('--netplay-port', String(options.netplayPort));
          }
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
          // melonDS Wi-Fi relay — used for NDS Pokémon WFC features
          if (options.netplayHost && options.netplayPort) {
            args.push('--wifi-host', options.netplayHost);
            args.push('--wifi-port', String(options.netplayPort));
          }
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
