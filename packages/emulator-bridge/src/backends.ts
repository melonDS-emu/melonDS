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
    description: 'Cross-platform N64 emulator',
    executableName: 'mupen64plus',
    supportsNetplay: true,
    supportsSaveStates: true,
    website: 'https://mupen64plus.org',
    notes: ['Plugin-based architecture', 'Netplay available via extensions'],
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
      'Local wireless emulation between instances',
      'Good DS Wi-Fi support',
      'Active development',
      'Core of this repository',
    ],
  },
  {
    id: 'desmume',
    name: 'DeSmuME',
    systems: ['nds'],
    description: 'Mature Nintendo DS emulator',
    executableName: 'desmume',
    supportsNetplay: false,
    supportsSaveStates: true,
    website: 'https://desmume.org',
    notes: ['Wide compatibility', 'No local wireless support'],
  },
];
