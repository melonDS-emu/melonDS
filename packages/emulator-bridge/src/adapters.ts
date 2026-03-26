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
   * Wii-specific: emulate MotionPlus extension on Wiimotes.
   * Maps to Dolphin's --wii-motionplus flag.
   */
  wiiMotionPlus?: boolean;
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
  /**
   * Performance preset controlling accuracy vs. speed trade-off.
   * - 'fast':     Maximum performance; may sacrifice accuracy (dynarec, fewer checks).
   * - 'balanced': Default emulator behaviour (no extra flags added).
   * - 'accurate': Strict timing / interpreter mode for best compatibility.
   *
   * Backend-specific mappings:
   *  - Mupen64Plus (N64): 'fast' → --emumode 2 (dynarec), 'accurate' → --emumode 0 (interpreter),
   *                       'balanced' → emulator default (no --emumode flag appended)
   *  - All other backends (FCEUX, Snes9x, mGBA, melonDS, etc.): field is acknowledged but
   *    no additional flags are appended; use `compatibilityFlags` for manual overrides.
   */
  performancePreset?: 'fast' | 'balanced' | 'accurate';
  /**
   * Additional compatibility flags passed verbatim to the emulator binary.
   * Appended at the end of the argument list for maximum compatibility.
   * Example: ['--no-audio', '--single-core']
   */
  compatibilityFlags?: string[];
  /**
   * N64-specific: enable Transfer Pak emulation.
   * Required for Pokémon Stadium and Pokémon Stadium 2 to read Game Boy cartridge save data.
   * Maps to Mupen64Plus `--transfer-pak` flag.
   */
  transferPakEnabled?: boolean;
  /**
   * N64-specific: controller pak accessory mode.
   * - 'none':     No pak inserted (default for cartridge-save games).
   * - 'standard': Standard Controller Pak (memory card); used by most multiplayer games.
   * - 'rumble':   Rumble Pak; replaces Controller Pak slot.
   * Maps to Mupen64Plus `--controller-pak none|standard|rumble` flag.
   */
  controllerPakMode?: 'none' | 'standard' | 'rumble';
  /**
   * N64-specific: hint the cartridge save type so Mupen64Plus does not need to auto-detect.
   * - 'eeprom-4k':  Small EEPROM (512 bytes), used by most early N64 titles.
   * - 'eeprom-16k': Large EEPROM (2 KB), used by later games (e.g. Star Fox 64).
   * - 'sram':       256 KB SRAM (used by e.g. Zelda: Ocarina of Time).
   * - 'flash':      1 MB Flash ROM (used by e.g. Pokémon Snap).
   * Maps to Mupen64Plus `--savetype <type>` flag.
   */
  n64SaveType?: 'eeprom-4k' | 'eeprom-16k' | 'sram' | 'flash';
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
 *                         --netplay-player <slot>  (1-based player slot)
 *                         --emumode <0|1|2>  (interpreter / cached / dynarec)
 *                         --transfer-pak  (enable Transfer Pak for Pokémon Stadium)
 *                         --controller-pak <none|standard|rumble>
 *                         --savetype <eeprom-4k|eeprom-16k|sram|flash>
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
 *  - Dolphin:             -b -e <rom>  (batch mode launch)
 *                         (netplay handled by relay at TCP level)
 *                         --debugger  (open integrated debugger)
 *                         --wii-motionplus  (emulate MotionPlus extension)
 *  - Citra / Lime3DS:     <rom>  (direct ROM path)
 *                         (multiplayer via relay or citra-room server)
 *                         --single-window  (single window mode)
 *  - Flycast:             <rom>  (standalone) or RetroArch with flycast_libretro
 *                         (relay netplay via RetroArch --host/--connect)
 *                         --fullscreen  (fullscreen mode)
 *                         --verbose  (debug output)
 *  - DuckStation:         <rom>  (no built-in netplay; relay-only)
 *                         --fullscreen  (fullscreen mode)
 *                         --verbose  (debug output)
 *  - PCSX2:               -- <rom>  (ROM path after double dash)
 *                         (relay netplay at TCP level; no CLI netplay flags)
 *                         --fullscreen  (fullscreen mode)
 *                         --verbose  (debug output)
 *  - PPSSPP:              <rom>  (ROM path as first argument)
 *                         --adhoc-server <host>:<port>  (ad-hoc relay)
 *                         --fullscreen  (fullscreen mode)
 *                         --verbose  (debug output)
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
 *  - Dolphin:   --debugger  opens the integrated debugger
 *  - Citra:     no dedicated debug flag; use RetroArch with citra_libretro for verbose output
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
          if (options.compatibilityFlags) args.push(...options.compatibilityFlags);
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
          if (options.compatibilityFlags) args.push(...options.compatibilityFlags);
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
            if (options.compatibilityFlags) args.push(...options.compatibilityFlags);
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
        // parallel-n64 is a high-accuracy Vulkan-based libretro core (via RetroArch)
        fallbackBackendIds: ['parallel-n64', 'project64', 'retroarch'],
        buildLaunchArgs: (romPath, options) => {
          const effectiveN64Backend = backendId ?? 'mupen64plus';
          // parallel-n64 and retroarch both use the RetroArch CLI convention
          if (effectiveN64Backend === 'retroarch' || effectiveN64Backend === 'parallel-n64') {
            return buildRetroArchArgs(romPath, options);
          }
          // ── Mupen64Plus ────────────────────────────────────────────────────
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
          // Performance preset → Mupen64Plus --emumode flag
          // 0 = pure interpreter (accurate), 1 = cached (balanced), 2 = dynamic recompiler (fast)
          if (options.performancePreset && options.performancePreset !== 'balanced') {
            const emuMode = options.performancePreset === 'fast' ? '2' : '0';
            args.push('--emumode', emuMode);
          }
          // N64 accessory / save-type hints
          if (options.transferPakEnabled) args.push('--transfer-pak');
          if (options.controllerPakMode && options.controllerPakMode !== 'none') {
            args.push('--controller-pak', options.controllerPakMode);
          }
          if (options.n64SaveType) args.push('--savetype', options.n64SaveType);
          if (options.compatibilityFlags) args.push(...options.compatibilityFlags);
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
          if (options.compatibilityFlags) args.push(...options.compatibilityFlags);
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/nds/${gameId}`,
      };
    }

    case 'gc': {
      const effectiveGcBackend = backendId ?? 'dolphin';
      return {
        system: 'gc',
        preferredBackendId: 'dolphin',
        fallbackBackendIds: ['retroarch'],
        buildLaunchArgs: (romPath, options) => {
          if (effectiveGcBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          // Dolphin: batch mode (-b) with explicit ROM path (-e)
          // Netplay is handled at TCP relay level; no native CLI netplay flags used here.
          const args = ['-b', '-e', romPath];
          if (options.fullscreen) args.push('--no-gui');
          if (options.debug) args.push('--debugger');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/gc/${gameId}`,
      };
    }

    case 'wii': {
      const effectiveWiiBackend = backendId ?? 'dolphin';
      return {
        system: 'wii',
        preferredBackendId: 'dolphin',
        fallbackBackendIds: ['retroarch'],
        buildLaunchArgs: (romPath, options) => {
          if (effectiveWiiBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          // Dolphin: batch mode (-b) with explicit ROM path (-e)
          // Netplay is handled at TCP relay level.
          // Dolphin auto-detects Wii vs GC from the disc header — no extra flag needed.
          const args = ['-b', '-e', romPath];
          if (options.fullscreen) args.push('--no-gui');
          // MotionPlus emulation (Wii MotionPlus games, e.g. Wii Sports Resort)
          if (options.wiiMotionPlus) args.push('--wii-motionplus');
          if (options.debug) args.push('--debugger');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/wii/${gameId}`,
      };
    }

    case '3ds': {
      const effectiveCitraBackend = backendId ?? 'citra';
      return {
        system: '3ds',
        preferredBackendId: 'citra',
        fallbackBackendIds: ['retroarch'],
        buildLaunchArgs: (romPath, options) => {
          if (effectiveCitraBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          // Citra / Lime3DS: ROM path as first argument.
          // Multiplayer is handled at relay level or via citra-room server.
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          // Relay netplay: connect to relay host/port for bridged sessions
          if (options.netplayHost && options.netplayPort) {
            args.push('--multiplayer-server', options.netplayHost);
            args.push('--multiplayer-port', String(options.netplayPort));
          }
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/3ds/${gameId}`,
      };
    }

    case 'genesis': {
      // SEGA Genesis / Mega Drive uses RetroArch with the Genesis Plus GX core.
      // Netplay uses the standard RetroArch --host / --connect relay flags.
      return {
        system: 'genesis',
        preferredBackendId: 'retroarch',
        fallbackBackendIds: [],
        buildLaunchArgs: (romPath, options) => buildRetroArchArgs(romPath, options),
        getSavePath: (gameId, baseDir) => `${baseDir}/genesis/${gameId}`,
      };
    }

    case 'sms': {
      // Sega Master System / Game Gear uses RetroArch with the Genesis Plus GX core.
      // Genesis Plus GX supports SMS, Game Gear, and SG-1000 in addition to Genesis/MD.
      // Netplay uses the standard RetroArch --host / --connect relay flags.
      return {
        system: 'sms',
        preferredBackendId: 'retroarch',
        fallbackBackendIds: [],
        buildLaunchArgs: (romPath, options) => buildRetroArchArgs(romPath, options),
        getSavePath: (gameId, baseDir) => `${baseDir}/sms/${gameId}`,
      };
    }

    case 'dreamcast': {
      // Flycast: standalone or via RetroArch with flycast_libretro
      // Netplay: relay at TCP level; Flycast has experimental online but relay is recommended
      return {
        system: 'dreamcast',
        preferredBackendId: 'retroarch',
        fallbackBackendIds: ['flycast'],
        buildLaunchArgs: (romPath, options) => {
          const effectiveDcBackend = backendId ?? 'retroarch';
          if (effectiveDcBackend === 'flycast') {
            const args = [romPath];
            if (options.fullscreen) args.push('--fullscreen');
            if (options.debug) args.push('--verbose');
            return args;
          }
          return buildRetroArchArgs(romPath, options);
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/dreamcast/${gameId}`,
      };
    }

    case 'psx': {
      const effectivePsxBackend = backendId ?? 'duckstation';
      return {
        system: 'psx',
        preferredBackendId: 'duckstation',
        fallbackBackendIds: ['retroarch'],
        buildLaunchArgs: (romPath, options) => {
          if (effectivePsxBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          // DuckStation: relay-only; no built-in netplay CLI flags
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.debug) args.push('--verbose');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/psx/${gameId}`,
      };
    }

    case 'ps2': {
      const effectivePs2Backend = backendId ?? 'pcsx2';
      return {
        system: 'ps2',
        preferredBackendId: 'pcsx2',
        fallbackBackendIds: ['retroarch'],
        buildLaunchArgs: (romPath, options) => {
          if (effectivePs2Backend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          // PCSX2: relay at TCP level; -- separates PCSX2 options from the ROM path
          const args: string[] = [];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.debug) args.push('--verbose');
          args.push('--', romPath);
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/ps2/${gameId}`,
      };
    }

    case 'psp': {
      const effectivePspBackend = backendId ?? 'ppsspp';
      return {
        system: 'psp',
        preferredBackendId: 'ppsspp',
        fallbackBackendIds: ['retroarch'],
        buildLaunchArgs: (romPath, options) => {
          if (effectivePspBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          // PPSSPP: ad-hoc relay via --adhoc-server
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.netplayHost && options.netplayPort) {
            args.push('--adhoc-server', `${options.netplayHost}:${options.netplayPort}`);
          }
          if (options.debug) args.push('--verbose');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/psp/${gameId}`,
      };
    }

    case 'wiiu': {
      const effectiveWiiUBackend = backendId ?? 'cemu';
      return {
        system: 'wiiu',
        preferredBackendId: 'cemu',
        fallbackBackendIds: ['retroarch'],
        buildLaunchArgs: (romPath, options) => {
          if (effectiveWiiUBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          // Cemu: -g <rom> launches the game directly; relay handles TCP-level netplay
          const args = ['-g', romPath];
          if (options.fullscreen) args.push('-f');
          if (options.debug) args.push('--verbose');
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/wiiu/${gameId}`,
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
