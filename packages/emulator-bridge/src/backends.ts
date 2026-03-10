/**
 * Known emulator backends that RetroOasis can integrate with.
 * Each backend handles one or more Nintendo systems.
 *
 * The app does NOT bundle these backends — users must have them installed
 * or the app provides guided setup.
 */
export interface BackendDefinition {
  id: string;
  name: string;
  systems: string[];
  description: string;
  executableName: string;
  supportsNetplay: boolean;
  supportsSaveStates: boolean;
  website: string;
  notes: string[];
}

export const KNOWN_BACKENDS: BackendDefinition[] = [
  {
    id: 'fceux',
    name: 'FCEUX',
    systems: ['nes'],
    description: 'Highly compatible NES/Famicom emulator',
    executableName: 'fceux',
    supportsNetplay: true,
    supportsSaveStates: true,
    website: 'https://fceux.com',
    notes: ['Good netplay support', 'Lua scripting available'],
  },
  {
    id: 'snes9x',
    name: 'Snes9x',
    systems: ['snes'],
    description: 'Popular and accurate SNES emulator',
    executableName: 'snes9x',
    supportsNetplay: true,
    supportsSaveStates: true,
    website: 'https://www.snes9x.com',
    notes: ['Strong compatibility', 'Netplay via built-in support'],
  },
  {
    id: 'bsnes',
    name: 'bsnes',
    systems: ['snes'],
    description: 'Cycle-accurate SNES emulator',
    executableName: 'bsnes',
    supportsNetplay: false,
    supportsSaveStates: true,
    website: 'https://github.com/bsnes-emu/bsnes',
    notes: ['Highest accuracy', 'Higher system requirements'],
  },
  {
    id: 'mgba',
    name: 'mGBA',
    systems: ['gb', 'gbc', 'gba'],
    description: 'Fast and accurate Game Boy Advance emulator (also supports GB/GBC)',
    executableName: 'mgba',
    supportsNetplay: true,
    supportsSaveStates: true,
    website: 'https://mgba.io',
    notes: ['Link cable emulation', 'Excellent compatibility', 'Active development'],
  },
  {
    id: 'gambatte',
    name: 'Gambatte',
    systems: ['gb', 'gbc'],
    description: 'Accurate Game Boy / Game Boy Color emulator',
    executableName: 'gambatte',
    supportsNetplay: false,
    supportsSaveStates: true,
    website: 'https://github.com/sinamas/gambatte',
    notes: ['Very high accuracy for GB/GBC'],
  },
  {
    id: 'sameboy',
    name: 'SameBoy',
    systems: ['gb', 'gbc'],
    description: 'Highly accurate Game Boy and Game Boy Color emulator with link cable support',
    executableName: 'sameboy',
    supportsNetplay: true,
    supportsSaveStates: true,
    website: 'https://sameboy.github.io',
    notes: [
      'Best-in-class GB/GBC accuracy',
      'Link cable emulation via TCP (--link-address)',
      'Excellent for Pokémon Gen I/II trades and battles',
      'Open source (MIT license)',
    ],
  },
  {
    id: 'vbam',
    name: 'VisualBoyAdvance-M',
    systems: ['gb', 'gbc', 'gba'],
    description: 'Feature-rich Game Boy Advance (and GB/GBC) emulator with link cable netplay',
    executableName: 'visualboyadvance-m',
    supportsNetplay: true,
    supportsSaveStates: true,
    website: 'https://visualboyadvance-m.org',
    notes: [
      'Link cable emulation over network (--link-host)',
      'Supports GBA, GBC, and GB ROMs',
      'Good compatibility for Pokémon Gen III link features',
      'Open source (GPLv2)',
    ],
  },
  {
    id: 'mupen64plus',
    name: 'Mupen64Plus',
    systems: ['n64'],
    description: 'Cross-platform N64 emulator with plugin architecture',
    executableName: 'mupen64plus',
    supportsNetplay: true,
    supportsSaveStates: true,
    website: 'https://mupen64plus.org',
    notes: [
      'Install (Linux): sudo apt install mupen64plus mupen64plus-audio-sdl mupen64plus-input-sdl mupen64plus-video-glide64mk2',
      'Install (macOS): brew install mupen64plus',
      'Install (Windows): download from https://mupen64plus.org/releases/',
      'Detection: app checks for "mupen64plus" in PATH; ensure it is on your PATH after install',
      'Netplay args: --netplay-host <host> --netplay-port <port> --netplay-player <slot>',
      'Analog stick: requires mupen64plus-input-sdl plugin with "AnalogDeadzone" and "AnalogPeak" calibration',
      'Recommended video plugin: mupen64plus-video-glide64mk2 (default) or mupen64plus-video-rice',
      'For best party-game performance use GlideN64 (mupen64plus-video-gliden64) when available',
    ],
  },
  {
    id: 'project64',
    name: 'Project64',
    systems: ['n64'],
    description: 'Popular Windows N64 emulator',
    executableName: 'project64',
    supportsNetplay: false,
    supportsSaveStates: true,
    website: 'https://www.pj64-emu.com',
    notes: ['Windows only', 'Good compatibility'],
  },
  {
    id: 'melonds',
    name: 'melonDS',
    systems: ['nds'],
    description: 'Fast and accurate Nintendo DS emulator with local wireless support',
    executableName: 'melonDS',
    supportsNetplay: true,
    supportsSaveStates: true,
    website: 'https://melonds.kuribo64.net',
    notes: [
      'Core of this repository — the melonDS C++ source is embedded in /src',
      'Install (Linux): sudo apt install melonds  OR build from /src with cmake',
      'Install (macOS): brew install melonds',
      'Install (Windows): download from https://melonds.kuribo64.net',
      'Detection: app checks for "melonDS" in PATH; ensure it is on your PATH after install',
      'Local wireless: run multiple melonDS instances; they discover each other automatically',
      'Wi-Fi relay args: --wifi-host <host> --wifi-port <port>',
      'Screen layout: --screen-layout=stacked|side-by-side|top-focus|bottom-focus',
      'Touchscreen: --touch-mouse enables mouse-driven bottom screen input',
      'WFC DNS override: --wfc-dns=<ip> for Wiimmfi (178.62.43.212) or other WFC replacements',
      'BIOS/firmware: place NDS BIOS (bios7.bin, bios9.bin) and firmware.bin in ~/.config/melonDS/',
      'DSi mode: place DSi BIOS files (bios7i.bin, bios9i.bin, nand.bin) alongside the above',
      'Save states: Ctrl+1-8 to save slot, Shift+1-8 to load slot',
      'Screen swap hotkey: F11 swaps top/bottom screens while running',
    ],
  },
  {
    id: 'desmume',
    name: 'DeSmuME',
    systems: ['nds'],
    description: 'Mature Nintendo DS emulator with wide game compatibility',
    executableName: 'desmume',
    supportsNetplay: false,
    supportsSaveStates: true,
    website: 'https://desmume.org',
    notes: [
      'Wide game compatibility — good fallback when melonDS has issues',
      'Install (Linux): sudo apt install desmume',
      'Install (macOS): brew install --cask desmume  OR build from source',
      'Install (Windows): download from https://desmume.org/download/',
      'Detection: app checks for "desmume" in PATH',
      'No built-in Wi-Fi/WFC netplay support — relay-only sessions via RetroOasis TCP relay',
      'Use melonDS for WFC / Pokémon online features; DeSmuME is for local or single-player',
      'Performance: --num-cores=2 for dual-core CPU utilization on older hardware',
      'BIOS: DeSmuME can run without BIOS files (HLE mode), though BIOS improves compatibility',
    ],
  },
];
