/** Supported Nintendo systems */
export type SupportedSystem =
  | 'nes'
  | 'snes'
  | 'gb'
  | 'gbc'
  | 'gba'
  | 'n64'
  | 'nds'
  | 'gc'
  | '3ds';

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
  '3ds': {
    id: '3ds',
    name: 'Nintendo 3DS',
    shortName: '3DS',
    generation: 8,
    maxLocalPlayers: 8,
    supportsLink: false,
    color: '#CC0000',
  },
};
