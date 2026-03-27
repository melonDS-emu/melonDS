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
  /**
   * PSX-specific: enable PGXP (Parallel/Precision Geometry Transform Pipeline).
   * Corrects 3-D perspective on polygons, eliminating the characteristic PS1 texture warping.
   * Supported by DuckStation natively via `--pgxp`.
   */
  pgxpEnabled?: boolean;
  /**
   * PSX-specific: skip the BIOS boot animation and jump directly into the game.
   * Maps to DuckStation `--fast-boot` flag.
   * Not available when using Mednafen PSX (accuracy-focused emulator always runs the full BIOS).
   */
  cdRomFastBoot?: boolean;
  /**
   * PSX-specific: enable the PlayStation Multitap accessory, allowing 3 or 4 controllers.
   * Required for 3-P and 4-P titles (e.g. Crash Bash, Crash Team Racing).
   * Maps to DuckStation `--multitap` flag.
   */
  multitapEnabled?: boolean;
  /**
   * PSX-specific: use DualShock analog controller mode instead of the default digital pad.
   * Needed for games that read the analog sticks directly (e.g. Ape Escape, Gran Turismo 2).
   * Maps to DuckStation `--analog` flag.
   */
  analogControllerEnabled?: boolean;
  /**
   * Dreamcast-specific: force a region for the emulated console.
   * Flycast / flycast_libretro auto-detects region from the disc header, but some region-locked
   * titles (or region-free hacks) benefit from an explicit override.
   * Maps to Flycast `--region=<value>` flag or the `flycast_region` RetroArch core option.
   *  - 'ntsc-j': Japan
   *  - 'ntsc-u': North America
   *  - 'pal':    Europe (50 Hz)
   */
  dcRegion?: 'ntsc-j' | 'ntsc-u' | 'pal';
  /**
   * Dreamcast-specific: enable VMU (Visual Memory Unit) emulation.
   * When true, Flycast creates virtual VMU save files in the save directory.
   * Defaults to true; set false to disable for standalone / single-player sessions
   * where VMU I/O adds unnecessary overhead.
   * Maps to Flycast `--vmu=<0|1>` flag.
   */
  vmuEnabled?: boolean;
  /**
   * Dreamcast-specific: select the video output cable type.
   * Affects the display mode negotiated by the emulated console.
   *  - 'vga':       VGA output — 480p progressive; highest quality; not compatible with all games
   *  - 'rgb':       RGB SCART (480i interlaced) — good colour accuracy
   *  - 'composite': Composite (480i) — widest compatibility, lower quality
   * Maps to Flycast `--cable-type=<value>` flag.
   * Default when omitted: Flycast uses its configured default (usually VGA).
   */
  dcCableType?: 'vga' | 'rgb' | 'composite';
  /**
   * Wii U-specific: GamePad controller mode for asymmetric multiplayer.
   * - 'gamepad':  Emulate the Wii U GamePad (second screen + touch input via mouse).
   * - 'pro':      Use a standard Pro Controller layout (no GamePad second-screen).
   * - 'wiimote':  Emulate a Wiimote (for games that support Wii-mode play on Wii U).
   * Cemu maps this to its controller profile configuration; no CLI flag equivalent.
   */
  wiiuGamepadMode?: 'gamepad' | 'pro' | 'wiimote';
  /**
   * Wii U-specific: enable the Pro Controller input profile.
   * When true, Cemu uses an Xbox/PS gamepad mapped as a Wii U Pro Controller
   * rather than emulating the GamePad.  Use for games that support Pro Controller.
   */
  wiiuProControllerEnabled?: boolean;
  /**
   * Wii U-specific: force HDMI colour output mode.
   * - 'rgb':    Full-range RGB (TV mode); standard for most displays.
   * - 'yuv':    YUV 4:2:0 (broadcast TV format); use if colours look washed out.
   * Cemu graphics pack or config option; no CLI flag.
   */
  wiiuHdmiMode?: 'rgb' | 'yuv';
  /**
   * 3DS-specific: screen layout mode for Citra/Lime3DS.
   * - 'default':      Top screen above bottom (hardware layout).
   * - 'single':       Top screen only (good for 2D games without touch input).
   * - 'large':        Top screen large, bottom screen small corner overlay.
   * - 'side-by-side': Top and bottom screens placed horizontally side by side.
   * Maps to Citra Display Settings → Layout.
   */
  threeDsLayout?: 'default' | 'single' | 'large' | 'side-by-side';
  /**
   * 3DS-specific: force a system region for the emulated 3DS.
   * - 'auto':   Auto-detect from ROM (default).
   * - 'jpn':    Japan
   * - 'usa':    North America
   * - 'eur':    Europe
   * - 'aus':    Australia
   * - 'kor':    Korea
   * - 'twn':    Taiwan / China
   * Maps to Citra System → Region configuration.
   */
  threeDsRegion?: 'auto' | 'jpn' | 'usa' | 'eur' | 'aus' | 'kor' | 'twn';
  /**
   * 3DS-specific: enable stereoscopic 3D rendering.
   * - 'off':          No 3D effect (fastest).
   * - 'anaglyph':     Red/cyan anaglyph glasses.
   * - 'interlaced':   For passive 3D monitors.
   * - 'side-by-side': For VR headsets or side-by-side 3D displays.
   * Maps to Citra Display Settings → Stereoscopy.
   */
  threeDsStereoscopic?: 'off' | 'anaglyph' | 'interlaced' | 'side-by-side';
  /**
   * Switch-specific: path to the Ryujinx prod.keys file (title key derivation).
   * Required for decrypting NSP/XCI game files.  If omitted, Ryujinx uses its
   * configured key directory ($HOME/.config/Ryujinx/system/prod.keys).
   */
  switchProdKeys?: string;
  /**
   * Switch-specific: enable Ryujinx LDN (Local Device Network) mode.
   * When true, Ryujinx emulates the Switch's local-wireless stack, allowing
   * players on the same LAN (or over a VPN) to discover and join each other
   * as if using local wireless mode on real hardware.
   */
  switchLdnEnabled?: boolean;
  /**
   * Switch-specific: Resolution scale multiplier for Ryujinx.
   * - 1: Native (720p handheld / 1080p docked)
   * - 2: 1440p handheld / 2160p (4K) docked
   * - 3: 2160p (4K) handheld
   * Passed as --resolution-scale <n> when Ryujinx supports it.
   */
  switchResolutionScale?: 1 | 2 | 3;
  /**
   * PS2-specific: hardware renderer for PCSX2.
   * - 'Vulkan': Best performance on modern GPUs (default).
   * - 'OpenGL': Good compatibility fallback.
   * - 'Software': Most accurate; very slow.
   * Maps to PCSX2 --renderer=<value> flag.
   */
  ps2HwRenderer?: 'Vulkan' | 'OpenGL' | 'Software';
  /**
   * PS2-specific: region override for PCSX2.
   * - 'NTSC-U': North America
   * - 'NTSC-J': Japan
   * - 'PAL':    Europe
   * Maps to PCSX2 --region=<value> flag.
   */
  ps2Region?: 'NTSC-U' | 'NTSC-J' | 'PAL';
  /**
   * PS2-specific: skip the PS2 BIOS splash screen.
   * Maps to PCSX2 --fast-boot flag.
   */
  ps2FastBoot?: boolean;
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
 *                         --region=ntsc-u|ntsc-j|pal  (force console region)
 *                         --cable-type=vga|rgb|composite  (output cable type)
 *                         --vmu=0|1  (disable/enable VMU emulation)
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
 *  - Flycast:   --verbose  enables debug output; --no-dynarec forces interpreter mode
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
          // Layout mode (Citra/Lime3DS --layout-option flag)
          if (options.threeDsLayout && options.threeDsLayout !== 'default') {
            const layoutMap: Record<string, string> = {
              single: '1',
              large: '2',
              'side-by-side': '3',
            };
            const layoutVal = layoutMap[options.threeDsLayout];
            if (layoutVal) args.push('--layout-option', layoutVal);
          }
          // Region override
          if (options.threeDsRegion && options.threeDsRegion !== 'auto') {
            args.push('--region', options.threeDsRegion);
          }
          if (options.compatibilityFlags) args.push(...options.compatibilityFlags);
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

    case 'segacd': {
      // SEGA CD / Mega-CD uses RetroArch with the Genesis Plus GX core.
      // Genesis Plus GX natively supports Sega CD disc images (.cue/.bin pairs and .chd).
      // Requires a Sega CD BIOS image (bios_CD_U.bin / bios_CD_E.bin / bios_CD_J.bin)
      // placed in the RetroArch system directory.
      // Netplay uses the standard RetroArch --host / --connect relay flags.
      return {
        system: 'segacd',
        preferredBackendId: 'retroarch',
        fallbackBackendIds: [],
        buildLaunchArgs: (romPath, options) => buildRetroArchArgs(romPath, options),
        getSavePath: (gameId, baseDir) => `${baseDir}/segacd/${gameId}`,
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
      // RetroArch + flycast_libretro is the preferred netplay path.
      // Standalone Flycast is supported as a fallback for local / rollback sessions.
      // Relay netplay: TCP-level bridging; use RetroArch --host/--connect flags.
      // Standalone Flycast 2.x supports experimental rollback netcode (NAOMI/DC).
      const effectiveDcBackend = backendId ?? 'retroarch';
      return {
        system: 'dreamcast',
        preferredBackendId: 'retroarch',
        fallbackBackendIds: ['flycast'],
        buildLaunchArgs: (romPath, options) => {
          if (effectiveDcBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          // ── Standalone Flycast ────────────────────────────────────────────
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.debug) args.push('--verbose');
          // Region override (auto-detected from disc when omitted)
          if (options.dcRegion) args.push(`--region=${options.dcRegion}`);
          // Cable type (VGA / RGB / composite)
          if (options.dcCableType) args.push(`--cable-type=${options.dcCableType}`);
          // VMU emulation toggle (enabled by default in Flycast)
          if (options.vmuEnabled === false) args.push('--vmu=0');
          // Performance preset: interpreter mode for debugging accuracy
          if (options.performancePreset === 'accurate') args.push('--no-dynarec');
          if (options.compatibilityFlags) args.push(...options.compatibilityFlags);
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/dreamcast/${gameId}`,
      };
    }

    case 'psx': {
      const effectivePsxBackend = backendId ?? 'duckstation';
      return {
        system: 'psx',
        preferredBackendId: 'duckstation',
        fallbackBackendIds: ['mednafen-psx', 'retroarch'],
        buildLaunchArgs: (romPath, options) => {
          if (effectivePsxBackend === 'retroarch' || effectivePsxBackend === 'mednafen-psx') {
            return buildRetroArchArgs(romPath, options);
          }
          // DuckStation: relay-only; no built-in P2P netplay CLI flags
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.debug) args.push('--verbose');
          if (options.pgxpEnabled) args.push('--pgxp');
          if (options.cdRomFastBoot) args.push('--fast-boot');
          if (options.multitapEnabled) args.push('--multitap');
          if (options.analogControllerEnabled) args.push('--analog');
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
          // Hardware renderer (default: Vulkan)
          if (options.ps2HwRenderer) args.push(`--renderer=${options.ps2HwRenderer}`);
          // Region override (NTSC-U/NTSC-J/PAL)
          if (options.ps2Region) args.push(`--region=${options.ps2Region}`);
          // Fast boot skips the PS2 BIOS splash
          if (options.ps2FastBoot) args.push('--fast-boot');
          // Multitap: enables 8-port multitap for 3–8 player games
          if (options.multitapEnabled) args.push('--multitap');
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
          // GamePad mode: no CLI flag — handled via Cemu controller config; documented for UI
          // Pro Controller: no CLI flag — controller profile selection in Cemu UI
          // Performance preset: no Cemu CLI equivalent; documented for UI
          if (options.performancePreset === 'accurate') args.push('--cpu-mode', 'interpreter');
          if (options.compatibilityFlags) args.push(...options.compatibilityFlags);
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/wiiu/${gameId}`,
      };
    }

    case 'switch': {
      const effectiveSwitchBackend = backendId ?? 'ryujinx';
      return {
        system: 'switch',
        preferredBackendId: 'ryujinx',
        fallbackBackendIds: ['retroarch'],
        buildLaunchArgs: (romPath, options) => {
          if (effectiveSwitchBackend === 'retroarch') {
            return buildRetroArchArgs(romPath, options);
          }
          // Ryujinx: ROM path as first positional argument
          const args = [romPath];
          if (options.fullscreen) args.push('--fullscreen');
          // LDN local-wireless netplay (Ryujinx built-in)
          if (options.switchLdnEnabled) args.push('--multiplayer-mode', 'ldn');
          // Resolution scale (Ryujinx config; no standard CLI flag yet — pass as compatibility flag if needed)
          // prod.keys path override
          if (options.switchProdKeys) args.push('--prod-keys', options.switchProdKeys);
          if (options.compatibilityFlags) args.push(...options.compatibilityFlags);
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/switch/${gameId}`,
      };
    }

    case 'ps3': {
      return {
        system: 'ps3',
        preferredBackendId: 'rpcs3',
        fallbackBackendIds: [],
        buildLaunchArgs: (romPath, options) => {
          // RPCS3: relay at TCP level via RPCN; no built-in P2P CLI flags
          const args = ['--no-gui', romPath];
          if (options.fullscreen) args.push('--fullscreen');
          if (options.debug) args.push('--verbose');
          if (options.compatibilityFlags) args.push(...options.compatibilityFlags);
          return args;
        },
        getSavePath: (gameId, baseDir) => `${baseDir}/ps3/${gameId}`,
      };
    }

    case 'sega32x': {
      // Sega 32X uses RetroArch with the PicoDrive core for both 32X and combo Sega CD+32X discs.
      // PicoDrive handles 32X ROMs (.32x) as well as Sega CD+32X combos.
      // Netplay uses the standard RetroArch --host / --connect relay flags.
      return {
        system: 'sega32x',
        preferredBackendId: 'retroarch',
        fallbackBackendIds: [],
        buildLaunchArgs: (romPath, options) => buildRetroArchArgs(romPath, options),
        getSavePath: (gameId, baseDir) => `${baseDir}/sega32x/${gameId}`,
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
