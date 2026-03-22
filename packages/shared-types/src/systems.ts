/** Supported systems (Nintendo + SEGA + Sony) */
export type SupportedSystem =
  | 'nes'
  | 'snes'
  | 'gb'
  | 'gbc'
  | 'gba'
  | 'n64'
  | 'nds'
  | 'gc'
  | 'wii'
  | 'wiiu'
  | '3ds'
  | 'genesis'
  | 'sms'
  | 'dreamcast'
  | 'psx'
  | 'ps2'
  | 'psp';

export interface SystemInfo {
  id: SupportedSystem;
  name: string;
  shortName: string;
  generation: number;
  maxLocalPlayers: number;
  supportsLink: boolean;
  color: string;
}

export const SYSTEM_INFO: Record<SupportedSystem, SystemInfo> = {
  nes: {
    id: 'nes',
    name: 'Nintendo Entertainment System',
    shortName: 'NES',
    generation: 3,
    maxLocalPlayers: 4,
    supportsLink: false,
    color: '#E60012',
  },
  snes: {
    id: 'snes',
    name: 'Super Nintendo Entertainment System',
    shortName: 'SNES',
    generation: 4,
    maxLocalPlayers: 4,
    supportsLink: false,
    color: '#7B5EA7',
  },
  gb: {
    id: 'gb',
    name: 'Game Boy',
    shortName: 'GB',
    generation: 4,
    maxLocalPlayers: 2,
    supportsLink: true,
    color: '#8B956D',
  },
  gbc: {
    id: 'gbc',
    name: 'Game Boy Color',
    shortName: 'GBC',
    generation: 4,
    maxLocalPlayers: 2,
    supportsLink: true,
    color: '#6A5ACD',
  },
  gba: {
    id: 'gba',
    name: 'Game Boy Advance',
    shortName: 'GBA',
    generation: 6,
    maxLocalPlayers: 4,
    supportsLink: true,
    color: '#4B0082',
  },
  n64: {
    id: 'n64',
    name: 'Nintendo 64',
    shortName: 'N64',
    generation: 5,
    maxLocalPlayers: 4,
    supportsLink: false,
    color: '#009900',
  },
  nds: {
    id: 'nds',
    name: 'Nintendo DS',
    shortName: 'NDS',
    generation: 7,
    maxLocalPlayers: 4,
    supportsLink: true,
    color: '#A0A0A0',
  },
  gc: {
    id: 'gc',
    name: 'Nintendo GameCube',
    shortName: 'GC',
    generation: 6,
    maxLocalPlayers: 4,
    supportsLink: false,
    color: '#6A0DAD',
  },
  wii: {
    id: 'wii',
    name: 'Nintendo Wii',
    shortName: 'Wii',
    generation: 7,
    maxLocalPlayers: 4,
    supportsLink: false,
    color: '#E4E4E4',
  },
  wiiu: {
    id: 'wiiu',
    name: 'Nintendo Wii U',
    shortName: 'Wii U',
    generation: 8,
    maxLocalPlayers: 5,
    supportsLink: false,
    color: '#009AC7',
  },
  '3ds': {
    id: '3ds',
    name: 'Nintendo 3DS',
    shortName: '3DS',
    generation: 8,
    maxLocalPlayers: 8,
    supportsLink: false,
    color: '#CC0000',
  },
  genesis: {
    id: 'genesis',
    name: 'SEGA Genesis',
    shortName: 'Genesis',
    generation: 4,
    maxLocalPlayers: 2,
    supportsLink: false,
    color: '#0066CC',
  },
  sms: {
    id: 'sms',
    name: 'Sega Master System',
    shortName: 'SMS',
    generation: 3,
    maxLocalPlayers: 2,
    supportsLink: false,
    color: '#003399',
  },
  dreamcast: {
    id: 'dreamcast',
    name: 'SEGA Dreamcast',
    shortName: 'Dreamcast',
    generation: 6,
    maxLocalPlayers: 4,
    supportsLink: false,
    color: '#FF6600',
  },
  psx: {
    id: 'psx',
    name: 'Sony PlayStation',
    shortName: 'PSX',
    generation: 5,
    maxLocalPlayers: 4,
    supportsLink: false,
    color: '#808080',
  },
  ps2: {
    id: 'ps2',
    name: 'Sony PlayStation 2',
    shortName: 'PS2',
    generation: 6,
    maxLocalPlayers: 4,
    supportsLink: false,
    color: '#00439C',
  },
  psp: {
    id: 'psp',
    name: 'Sony PlayStation Portable',
    shortName: 'PSP',
    generation: 7,
    maxLocalPlayers: 6,
    supportsLink: false,
    color: '#1C1C1C',
  },
};
