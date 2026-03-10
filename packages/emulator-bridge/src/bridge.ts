import { spawn, type ChildProcess } from 'child_process';
import { createSystemAdapter } from './adapters';

/**
 * EmulatorBridge is the main orchestration layer for launching and managing
 * emulator processes. It abstracts away the details of individual emulator
 * backends and provides a unified interface for the app.
 */
export interface EmulatorProcess {
  pid: number;
  sessionId: string;
  system: string;
  backendId: string;
  romPath: string;
  state: 'launching' | 'running' | 'paused' | 'stopped' | 'error';
  startedAt: string;
  exitCode?: number;
}

export interface LaunchOptions {
  romPath: string;
  system: string;
  backendId: string;
  saveDirectory: string;
  configPath?: string;
  fullscreen?: boolean;
  playerSlot?: number;
  /** Netplay: connect to relay or peer at this host */
  netplayHost?: string;
  /** Netplay: port of the relay or peer */
  netplayPort?: number;
  /**
   * Pre-assigned session token (UUID) for the relay server.
   * The relay proxy sends this as the first 36 bytes on connect so the
   * relay can authenticate and route the emulator's TCP stream.
   */
  netplaySessionToken?: string;
  /** NDS-specific */
  screenLayout?: 'stacked' | 'side-by-side' | 'top-focus' | 'bottom-focus';
  touchEnabled?: boolean;
}

export interface LaunchResult {
  success: boolean;
  process?: EmulatorProcess;
  error?: string;
}

export class EmulatorBridge {
  private runningProcesses: Map<string, EmulatorProcess> = new Map();
  private childProcesses: Map<string, ChildProcess> = new Map();

  /**
   * Launch a game using the specified backend.
   * Spawns a real child process using the system adapter's launch arguments.
   */
  async launch(options: LaunchOptions): Promise<LaunchResult> {
    const sessionId = crypto.randomUUID();

    // Build launch args via the system adapter
    let adapter;
    try {
      adapter = createSystemAdapter(options.system, options.backendId);
    } catch (err) {
      return { success: false, error: String(err) };
    }

    const args = adapter.buildLaunchArgs(options.romPath, {
      fullscreen: options.fullscreen,
      saveDirectory: options.saveDirectory,
      configPath: options.configPath,
      playerSlot: options.playerSlot,
      netplayHost: options.netplayHost,
      netplayPort: options.netplayPort,
      screenLayout: options.screenLayout,
      touchEnabled: options.touchEnabled,
    });

    const emulatorProcess: EmulatorProcess = {
      pid: 0,
      sessionId,
      system: options.system,
      backendId: options.backendId,
      romPath: options.romPath,
      state: 'launching',
      startedAt: new Date().toISOString(),
    };

    this.runningProcesses.set(sessionId, emulatorProcess);

    // Spawn the emulator as a real child process
    let child: ChildProcess;
    try {
      child = spawn(options.backendId, args, {
        detached: true,   // allow emulator to outlive the Node process if desired
        stdio: 'ignore',
      });
    } catch (err) {
      emulatorProcess.state = 'error';
      this.runningProcesses.delete(sessionId);
      return {
        success: false,
        error: `Failed to spawn emulator '${options.backendId}': ${String(err)}`,
      };
    }

    emulatorProcess.pid = child.pid ?? 0;
    emulatorProcess.state = 'running';

    child.on('error', (err: Error) => {
      console.error(`[emulator] Session ${sessionId} error:`, err.message);
      emulatorProcess.state = 'error';
      this.childProcesses.delete(sessionId);
    });

    child.on('exit', (code: number | null) => {
      emulatorProcess.state = 'stopped';
      emulatorProcess.exitCode = code ?? undefined;
      this.runningProcesses.delete(sessionId);
      this.childProcesses.delete(sessionId);
    });

    this.childProcesses.set(sessionId, child);

    return { success: true, process: emulatorProcess };
  }

  /**
   * Stop a running emulator session by sending SIGTERM to the child process.
   */
  async stop(sessionId: string): Promise<boolean> {
    const emulatorProcess = this.runningProcesses.get(sessionId);
    if (!emulatorProcess) return false;

    const child = this.childProcesses.get(sessionId);
    if (child && !child.killed) {
      child.kill('SIGTERM');
    }

    emulatorProcess.state = 'stopped';
    this.runningProcesses.delete(sessionId);
    this.childProcesses.delete(sessionId);
    return true;
  }

  /**
   * Get the state of a running session.
   */
  getProcess(sessionId: string): EmulatorProcess | undefined {
    return this.runningProcesses.get(sessionId);
  }

  /**
   * List all running emulator sessions.
   */
  listRunning(): EmulatorProcess[] {
    return Array.from(this.runningProcesses.values()).filter(
      (p) => p.state === 'running' || p.state === 'launching'
    );
  }
}
