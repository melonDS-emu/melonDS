import type { SupportedSystem } from './systems';

export interface SessionTemplate {
  id: string;
  gameId: string;
  system: SupportedSystem;
  emulatorBackendId: string;
  playerCount: number;
  controllerMappings: ControllerMapping[];
  saveRules: SessionSaveRules;
  netplayMode: NetplayMode;
  latencyTarget?: number;
  uiLayoutOptions?: UILayoutOptions;
}

export interface ControllerMapping {
  slot: number;
  profile: string;
  inputDevice?: string;
}

export interface SessionSaveRules {
  autoSave: boolean;
  autoSaveIntervalSeconds?: number;
  allowSaveStates: boolean;
  syncSavesToCloud: boolean;
}

export type NetplayMode = 'local-only' | 'online-p2p' | 'online-relay' | 'hybrid';

export interface UILayoutOptions {
  /** NDS-specific screen layout */
  dsScreenLayout?: 'stacked' | 'side-by-side' | 'top-focus' | 'bottom-focus';
  dsScreenSwap?: boolean;
  dsScreenGap?: number;
  /** General layout */
  showOverlay?: boolean;
  overlayPosition?: 'top' | 'bottom';
}

export interface LaunchRequest {
  sessionTemplateId: string;
  lobbyId?: string;
  gameId: string;
  romPath: string;
  system: SupportedSystem;
  players: SessionPlayer[];
}

export interface SessionPlayer {
  id: string;
  displayName: string;
  slot: number;
  isLocal: boolean;
}

export interface LaunchResult {
  success: boolean;
  sessionId?: string;
  error?: string;
  pid?: number;
}
