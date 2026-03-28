import { spawn, type ChildProcess } from 'child_process';
import { EventEmitter } from 'events';
import { createSystemAdapter } from './adapters';

/** Grace period (ms) between SIGTERM and the SIGKILL hard-kill fallback. */
const STOP_GRACE_MS = 5_000;

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
  /** NDS-specific */
  screenLayout?: 'stacked' | 'side-by-side' | 'top-focus' | 'bottom-focus';
  touchEnabled?: boolean;
  /**
   * Relay authentication token for this player's session.
   * Injected into the emulator process as the `RETRO_OASIS_SESSION_TOKEN`
   * environment variable so the emulator (or a wrapper) can authenticate
   * its TCP connection to the netplay relay.
   */
  sessionToken?: string;
}

export interface LaunchResult {
  success: boolean;
  process?: EmulatorProcess;
  error?: string;
}

/**
 * Structured lifecycle events emitted by EmulatorBridge on the `'event'` channel.
 *
 * - `launched`:     Emitted immediately after the child process is spawned successfully.
 * - `exited`:       Emitted when the process terminates on its own (clean exit or crash).
 *                   `exitCode` is `null` when the process was killed by a signal.
 * - `stopped`:      Emitted when stop() is called to end the session.
 * - `error`:        Emitted when the child process reports an OS-level error
 *                   (e.g. ENOENT — executable not found).
 * - `kill-forced`:  Emitted when SIGTERM did not stop the process within the
 *                   grace period and SIGKILL was sent as a hard fallback.
 */
export type EmulatorEventType = 'launched' | 'exited' | 'stopped' | 'error' | 'kill-forced';

export interface EmulatorLifecycleEvent {
  type: EmulatorEventType;
  sessionId: string;
  pid: number;
  backendId: string;
  system: string;
  /** Exit code — only present on `exited` events. null when killed by signal. */
  exitCode?: number | null;
  /** Error message — only present on `error` events. */
  errorMessage?: string;
  timestamp: string;
}

/**
 * EmulatorBridge is the main orchestration layer for launching and managing
 * emulator processes. It abstracts away the details of individual emulator
 * backends and provides a unified interface for the app.
 *
 * Extends EventEmitter and emits structured `EmulatorLifecycleEvent` objects
 * on the `'event'` channel so callers can react to process state changes
 * without polling.
 */
export class EmulatorBridge extends EventEmitter {
  private runningProcesses: Map<string, EmulatorProcess> = new Map();
  private childProcesses: Map<string, ChildProcess> = new Map();
  /** Kill-fallback timers keyed by sessionId. */
  private killTimers: Map<string, ReturnType<typeof setTimeout>> = new Map();

  /** Emit a structured lifecycle event on the `'event'` channel. */
  private _emitLifecycleEvent(event: EmulatorLifecycleEvent): void {
    this.emit('event', event);
  }

  /**
   * Launch a game using the specified backend.
   * Spawns a real child process using the system adapter's launch arguments.
   * Emits a `launched` lifecycle event on success, or returns an error result
   * without emitting if the adapter or spawn fails.
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

    // Build the child-process environment, injecting the relay session token
    // when one is provided so the emulator (or a thin wrapper script) can
    // present it on its TCP connection to the netplay relay.
    const childEnv: NodeJS.ProcessEnv = { ...process.env };
    if (options.sessionToken) {
      childEnv['RETRO_OASIS_SESSION_TOKEN'] = options.sessionToken;
    }

    // Spawn the emulator as a real child process
    let child: ChildProcess;
    try {
      child = spawn(options.backendId, args, {
        detached: true,   // allow emulator to outlive the Node process if desired
        stdio: 'ignore',
        env: childEnv,
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
      this._emitLifecycleEvent({
        type: 'error',
        sessionId,
        pid: emulatorProcess.pid,
        backendId: options.backendId,
        system: options.system,
        errorMessage: err.message,
        timestamp: new Date().toISOString(),
      });
    });

    child.on('exit', (code: number | null) => {
      // Cancel any pending SIGKILL timer — the process exited on its own
      const timer = this.killTimers.get(sessionId);
      if (timer !== undefined) {
        clearTimeout(timer);
        this.killTimers.delete(sessionId);
      }

      emulatorProcess.state = 'stopped';
      emulatorProcess.exitCode = code ?? undefined;
      this.runningProcesses.delete(sessionId);
      this.childProcesses.delete(sessionId);
      this._emitLifecycleEvent({
        type: 'exited',
        sessionId,
        pid: emulatorProcess.pid,
        backendId: options.backendId,
        system: options.system,
        exitCode: code,
        timestamp: new Date().toISOString(),
      });
    });

    this.childProcesses.set(sessionId, child);

    this._emitLifecycleEvent({
      type: 'launched',
      sessionId,
      pid: emulatorProcess.pid,
      backendId: options.backendId,
      system: options.system,
      timestamp: new Date().toISOString(),
    });

    return { success: true, process: emulatorProcess };
  }

  /**
   * Stop a running emulator session.
   *
   * Sends SIGTERM to request a graceful exit. If the process does not exit
   * within `gracePeriodMs` milliseconds, SIGKILL is sent as a hard fallback
   * to guarantee the process is terminated regardless of its state.
   *
   * Emits a `stopped` lifecycle event immediately, and a `kill-forced` event
   * if the hard-kill fallback is triggered.
   */
  async stop(sessionId: string, gracePeriodMs = STOP_GRACE_MS): Promise<boolean> {
    const emulatorProcess = this.runningProcesses.get(sessionId);
    if (!emulatorProcess) return false;

    const child = this.childProcesses.get(sessionId);
    if (child && !child.killed) {
      // Request graceful exit
      child.kill('SIGTERM');

      // Schedule hard-kill as a fallback in case the process hangs
      const timer = setTimeout(() => {
        if (!child.killed) {
          child.kill('SIGKILL');
          this._emitLifecycleEvent({
            type: 'kill-forced',
            sessionId,
            pid: emulatorProcess.pid,
            backendId: emulatorProcess.backendId,
            system: emulatorProcess.system,
            timestamp: new Date().toISOString(),
          });
        }
        this.killTimers.delete(sessionId);
      }, gracePeriodMs);

      this.killTimers.set(sessionId, timer);
    }

    emulatorProcess.state = 'stopped';
    this.runningProcesses.delete(sessionId);
    this.childProcesses.delete(sessionId);

    this._emitLifecycleEvent({
      type: 'stopped',
      sessionId,
      pid: emulatorProcess.pid,
      backendId: emulatorProcess.backendId,
      system: emulatorProcess.system,
      timestamp: new Date().toISOString(),
    });

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
