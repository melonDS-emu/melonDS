import { SessionTemplateStore } from './templates';
import type { SessionTemplateConfig } from './templates';

export interface ActiveSession {
  id: string;
  templateId: string;
  lobbyId?: string;
  gameId: string;
  system: string;
  players: SessionPlayerInfo[];
  state: 'preparing' | 'launching' | 'active' | 'paused' | 'ended';
  startedAt: string;
  endedAt?: string;
}

export interface SessionPlayerInfo {
  id: string;
  displayName: string;
  slot: number;
  isLocal: boolean;
  isHost: boolean;
}

/**
 * SessionEngine coordinates the full lifecycle of a multiplayer gaming session.
 * It connects lobby state, session templates, and emulator launching.
 */
export class SessionEngine {
  private sessions: Map<string, ActiveSession> = new Map();
  private templateStore: SessionTemplateStore;

  constructor(templateStore?: SessionTemplateStore) {
    this.templateStore = templateStore ?? new SessionTemplateStore();
  }

  /**
   * Create a new session from a lobby and template.
   */
  createSession(
    gameId: string,
    system: string,
    players: SessionPlayerInfo[],
    lobbyId?: string,
    templateId?: string
  ): ActiveSession {
    const id = crypto.randomUUID();

    const session: ActiveSession = {
      id,
      templateId: templateId ?? '',
      lobbyId,
      gameId,
      system,
      players,
      state: 'preparing',
      startedAt: new Date().toISOString(),
    };

    this.sessions.set(id, session);
    return session;
  }

  /**
   * Get the session template configuration.
   */
  getTemplate(templateId: string): SessionTemplateConfig | null {
    return this.templateStore.get(templateId);
  }

  /**
   * Transition a session to launching state.
   */
  markLaunching(sessionId: string): boolean {
    const session = this.sessions.get(sessionId);
    if (!session || session.state !== 'preparing') return false;
    session.state = 'launching';
    return true;
  }

  /**
   * Transition a session to active state.
   */
  markActive(sessionId: string): boolean {
    const session = this.sessions.get(sessionId);
    if (!session || session.state !== 'launching') return false;
    session.state = 'active';
    return true;
  }

  /**
   * End a session.
   */
  endSession(sessionId: string): boolean {
    const session = this.sessions.get(sessionId);
    if (!session) return false;
    session.state = 'ended';
    session.endedAt = new Date().toISOString();
    return true;
  }

  /**
   * Get a session by ID.
   */
  getSession(sessionId: string): ActiveSession | null {
    return this.sessions.get(sessionId) ?? null;
  }

  /**
   * List all active sessions.
   */
  listActiveSessions(): ActiveSession[] {
    return Array.from(this.sessions.values()).filter(
      (s) => s.state === 'active' || s.state === 'launching' || s.state === 'preparing'
    );
  }
}
