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
  /** NDS-specific: screen layout ('stacked', 'side-by-side', 'top-focus', 'bottom-focus')
   *  Maps directly to the melonDS CLI `--screen-layout=<value>` flag. */
  screenLayout?: 'stacked' | 'side-by-side' | 'top-focus' | 'bottom-focus';
  /** NDS-specific: enable touchscreen input via mouse/touchpad */
  touchEnabled?: boolean;
  /** NDS-specific: WFC DNS server override (e.g. Wiimmfi '178.62.43.212') */
  wfcDnsServer?: string;
  /**
   * NDS-specific: launch in DSi mode.
   * Requires DSi BIOS files (bios7i.bin, bios9i.bin, nand.bin) in the melonDS config directory.
   * Maps to the melonDS CLI `--dsi-mode` flag.
   */
  dsiMode?: boolean;
  /**
   * Enable debug / developer mode for the emulator backend.
   * - mGBA:      opens a GDB stub server on port 2345 (--gdb 2345)
   * - FCEUX:     enables internal debugger at startup (--debug)
   * - Snes9x:    enables verbose logging (-v)
   * - Nestopia:  enables verbose log output (--log-level=debug)
   * - higan/ares: enables verbose output (--verbose)
   * - SameBoy:   enables built-in debugger (--debug)
   * - VisualBoyAdvance-M: enables debug logging (--debug)
   * - RetroArch: enables verbose log output (--verbose)
   * - Mupen64Plus: enables verbose console output (--verbose)
   * - melonDS:   enables verbose output (--verbose)
   */
  debug?: boolean;
  /**
   * Path to the libretro core shared library used by RetroArch.
   * Required when the effective backend is 'retroarch'.
   * Example: '/usr/lib/libretro/snes9x_libretro.so'
   */
  coreLibraryPath?: string;
}

/**
 * Build launch arguments for RetroArch with a libretro core.
 * RetroArch netplay convention:
 *   Host:   --host --port <port>
 *   Client: --connect <host> --port <port>
 * If no coreLibraryPath is provided the -L flag is omitted and RetroArch
 * will open its menu to let the user pick a core manually.
 */
function buildRetroArchArgs(romPath: string, options: AdapterOptions): string[] {
  const args: string[] = [];
  if (options.coreLibraryPath) {
    args.push('-L', options.coreLibraryPath);
  }
  args.push(romPath);
  if (options.fullscreen) args.push('--fullscreen');
  if (options.netplayHost && options.netplayPort) {
    // Client: connect to an existing host
    args.push('--connect', options.netplayHost, '--port', String(options.netplayPort));
    if (options.playerSlot !== undefined) {
      args.push('--nick', `Player${options.playerSlot + 1}`);
    }
  } else if (options.netplayPort) {
    // Host: start a netplay session on this port
    args.push('--host', '--port', String(options.netplayPort));
  }
  if (options.debug) args.push('--verbose');
  return args;
}

/**
 * Factory to create a system adapter for a given system.
 * Each adapter encapsulates the launch arguments and save path logic
 * specific to that system's preferred emulator backend.
 *
 * Netplay / link cable argument conventions:
 *  - FCEUX:               --net <host>:<port> --player <slot>
 *  - Nestopia UE:         no built-in netplay; relay only
 *  - Snes9x:              -netplay <host> <port>
 *  - higan / ares:        no built-in netplay; relay only
 *  - mGBA:                --link-host <host>:<port>  (link cable over TCP)
 *  - SameBoy:             --link-address <host>:<port>  (link cable over TCP)
 *  - VisualBoyAdvance-M:  --link-host <host>:<port>  (link cable over TCP)
 *  - Mupen64Plus:         --netplay-host <host> --netplay-port <port>
 *  - melonDS:             --wifi-host <host> --wifi-port <port>
 *                         --screen-layout=<layout>  (stacked|side-by-side|top-focus|bottom-focus)
 *                         --touch-mouse  (enable touchscreen via mouse click)
 *                         --wfc-dns=<ip>  (WFC DNS override for Pokémon / Nintendo Wi-Fi Connection)
 *  - RetroArch:           -L <core.so>  (libretro core path)
 *                         --host --port <port>  (netplay host)
 *                         --connect <host> --port <port>  (netplay client)
 *                         --verbose  (debug output)
 *  - DeSmuME:             --cflash-image=<sav>  (no built-in netplay; relay only)
 *                         --num-cores=<n>  (optional performance tuning)
 *
 * Debug argument conventions:
 *  - mGBA:      --gdb <port>  opens a GDB remote-debugging stub
 *  - FCEUX:     --debug  enables the internal debugger
 *  - Snes9x:    -v  verbose / debug output
 *  - Nestopia:  --log-level=debug
 *  - higan/ares: --verbose
 *  - SameBoy:   --debug  enables built-in debugger
 *  - VisualBoyAdvance-M: --debug  enables debug logging
 *  - RetroArch: --verbose
 *  - Mupen64Plus: --verbose
 *  - melonDS:   --verbose
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
        fallbackBackendIds: ['nestopia', 'retroarch'],
        buildLaunchArgs: (romPath, options) => {
          const effectiveNesBackend = backendId ?? 'fceux';
          if (effectiveNesBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          if (effectiveNesBackend === 'nestopia') {
            // Nestopia UE: no built-in netplay; relay only
            const args = [romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.debug) args.push('--log-level=debug');
            return args;
          }
          // FCEUX (default)
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.netplayHost && options.netplayPort) {
            args.push('--net', `${options.netplayHost}:${options.netplayPort}`);
            if (options.playerSlot !== undefined) {
              args.push('--player', String(options.playerSlot + 1));
            }
          }
          if (options.debug) args.push('--debug');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/nes/${gameId}`,
      };

    case 'snes':
      return {
        system: 'snes',
        preferredBackendId: 'snes9x',
        fallbackBackendIds: ['bsnes', 'higan', 'retroarch'],
        buildLaunchArgs: (romPath, options) => {
          const effectiveSnesBackend = backendId ?? 'snes9x';
          if (effectiveSnesBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          if (effectiveSnesBackend === 'higan') {
            // higan / ares: --system SNES <rom> [--verbose for debug]
            const args = ['--system', 'SNES', romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.debug) args.push('--verbose');
            return args;
          }
          // Snes9x (default) or bsnes
          const args = [romPath];
          if (options.fullscreen) args.push('-fullscreen');
          if (options.netplayHost && options.netplayPort) {
            args.push('-netplay', options.netplayHost, String(options.netplayPort));
          }
          if (options.debug) args.push('-v');
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
        fallbackBackendIds: system === 'gba' ? ['vbam', 'higan', 'retroarch'] : ['sameboy', 'gambatte', 'higan', 'retroarch'],
        buildLaunchArgs: (romPath, options) => {
          if (effectiveBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          if (effectiveBackend === 'higan') {
            // higan / ares: --system GB|GBC|GBA <rom>
            const systemLabel = system === 'gba' ? 'GBA' : system === 'gbc' ? 'GBC' : 'GB';
            const args = ['--system', systemLabel, romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.debug) args.push('--verbose');
            return args;
          }
          if (effectiveBackend === 'sameboy') {
            // SameBoy: link cable over TCP via --link-address <host:port>
            const args = [romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.netplayHost && options.netplayPort) {
              args.push('--link-address', `${options.netplayHost}:${options.netplayPort}`);
            }
            if (options.debug) args.push('--debug');
            return args;
          } else if (effectiveBackend === 'vbam') {
            // VisualBoyAdvance-M: link cable over TCP via --link-host <host:port>
            const args = [romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.netplayHost && options.netplayPort) {
              args.push('--link-host', `${options.netplayHost}:${options.netplayPort}`);
            }
            if (options.debug) args.push('--debug');
            return args;
          } else {
            // mGBA (default): link cable over TCP via --link-host <host:port>
            const args = ['-f', romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.netplayHost && options.netplayPort) {
              args.push('--link-host', `${options.netplayHost}:${options.netplayPort}`);
            }
            // mGBA debug: open GDB stub server on port 2345
            if (options.debug) args.push('--gdb', '2345');
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
        fallbackBackendIds: ['project64', 'retroarch'],
        buildLaunchArgs: (romPath, options) => {
          const effectiveN64Backend = backendId ?? 'mupen64plus';
          if (effectiveN64Backend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          const args = ['--rom', romPath];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.netplayHost && options.netplayPort) {
            args.push('--netplay-host', options.netplayHost);
            args.push('--netplay-port', String(options.netplayPort));
            // Player slot is 1-based in Mupen64Plus netplay
            if (options.playerSlot !== undefined) {
              args.push('--netplay-player', String(options.playerSlot + 1));
            }
          }
          if (options.debug) args.push('--verbose');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/n64/${gameId}`,
      };

    case 'nds': {
      const effectiveNdsBackend = backendId ?? 'melonds';
      return {
        system: 'nds',
        preferredBackendId: 'melonds',
        fallbackBackendIds: ['desmume'],
        buildLaunchArgs: (romPath, options) => {
          if (effectiveNdsBackend === 'desmume') {
            // DeSmuME: no built-in netplay; relay is handled at TCP level
            const args = [romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.saveDirectory) args.push(`--cflash-image=${options.saveDirectory}`);
            return args;
          }
          // melonDS (default): Wi-Fi relay + screen layout + touchscreen
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          // Map screen layout to melonDS CLI flag
          if (options.screenLayout) {
            args.push(`--screen-layout=${options.screenLayout}`);
          }
          // Enable mouse-driven touchscreen
          if (options.touchEnabled !== false) {
            args.push('--touch-mouse');
          }
          // Wi-Fi relay — used for NDS Pokémon WFC and local wireless
          if (options.netplayHost && options.netplayPort) {
            args.push('--wifi-host', options.netplayHost);
            args.push('--wifi-port', String(options.netplayPort));
          }
          // WFC DNS override (e.g. Wiimmfi) for Pokémon and Nintendo online titles
          if (options.wfcDnsServer) {
            args.push(`--wfc-dns=${options.wfcDnsServer}`);
          }
          // DSi mode — requires DSi BIOS files in the melonDS config directory
          if (options.dsiMode) {
            args.push('--dsi-mode');
          }
          if (options.debug) args.push('--verbose');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/nds/${gameId}`,
      };
    }

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
