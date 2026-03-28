import type { SupportedSystem } from './systems';

export interface EmulatorBackend {
  id: string;
  name: string;
  supportedSystems: SupportedSystem[];
  version: string;
  executablePath?: string;
  coreFile?: string;
  supportsNetplay: boolean;
  supportsSaveStates: boolean;
  notes: string[];
}

export interface SystemAdapter {
  system: SupportedSystem;
  preferredBackendId: string;
  fallbackBackendIds: string[];
  launchArgs: (romPath: string, options: AdapterLaunchOptions) => string[];
  savePath: (gameId: string) => string;
  configTemplatePath?: string;
}

export interface AdapterLaunchOptions {
  fullscreen?: boolean;
  saveDirectory?: string;
  configPath?: string;
  playerSlot?: number;
  netplayHost?: string;
  netplayPort?: number;
  /** NDS-specific */
  screenLayout?: string;
  touchEnabled?: boolean;
}

export type EmulatorState = 'idle' | 'launching' | 'running' | 'paused' | 'stopped' | 'error';
