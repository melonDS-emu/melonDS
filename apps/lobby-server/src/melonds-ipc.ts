/**
 * melonDS IPC bridge — TypeScript interface for the C++ core.
 *
 * The melonDS emulator core lives in `/src` of this repository (forked from
 * the upstream melonDS project). This module defines the **intended IPC
 * protocol** that will connect the TypeScript launcher / lobby server to the
 * C++ core once the native build and IPC channel are in place.
 *
 * ## Transport
 * The bridge communicates over a Unix domain socket (Linux/macOS) or a named
 * pipe (Windows). Messages are newline-delimited JSON objects.
 *
 * Socket path convention: `/tmp/retro-oasis-melonds-<sessionId>.sock`
 * Windows pipe name:      `\\.\pipe\retro-oasis-melonds-<sessionId>`
 *
 * ## Usage
 * ```ts
 * const bridge = createMelonDsIpc('session-123');
 * await bridge.connect('session-123');
 * await bridge.send({ type: 'launch', romPath: '/path/to/game.nds', saveDirectory: '/saves' });
 * await bridge.disconnect();
 * ```
 *
 * Use `createMelonDsIpc(sessionId, { forceStub: true })` in tests/CI to
 * skip the real socket and use the no-op stub instead.
 */

import * as net from 'net';
import * as os from 'os';
import * as path from 'path';

// ---------------------------------------------------------------------------
// Protocol message types
// ---------------------------------------------------------------------------

/** Commands that the TypeScript side can send to the C++ core. */
export type MelonDsCommand =
  | { type: 'launch';   romPath: string; saveDirectory: string; wifiHost?: string; wifiPort?: number; wfcDns?: string; screenLayout?: string; touchMouse?: boolean }
  | { type: 'pause' }
  | { type: 'resume' }
  | { type: 'stop' }
  | { type: 'saveState'; slotIndex: number }
  | { type: 'loadState'; slotIndex: number }
  | { type: 'setScreenLayout'; layout: 'stacked' | 'side-by-side' | 'top-focus' | 'bottom-focus' }
  | { type: 'ping' };

/** Responses that the C++ core sends back. */
export type MelonDsResponse =
  | { type: 'ok' }
  | { type: 'error'; message: string }
  | { type: 'pong'; uptimeMs: number }
  | { type: 'status'; state: MelonDsState; frameCount: number; fpsActual: number }
  | { type: 'saveStateDone'; slotIndex: number; path: string }
  | { type: 'loadStateDone'; slotIndex: number }
  | { type: 'exited'; exitCode: number };

export type MelonDsState = 'idle' | 'running' | 'paused' | 'error';

// ---------------------------------------------------------------------------
// Bridge interface
// ---------------------------------------------------------------------------

/**
 * Contract that any melonDS IPC implementation must satisfy.
 * The stub below fulfils this interface for dev/test builds.
 */
export interface MelonDsIpcBridge {
  /** Connect to (or launch) the melonDS process for the given session. */
  connect(sessionId: string): Promise<void>;

  /** Send a command and await the core's response. */
  send(command: MelonDsCommand): Promise<MelonDsResponse>;

  /** Disconnect and optionally terminate the core process. */
  disconnect(terminate?: boolean): Promise<void>;

  /** True while the bridge has an active connection. */
  readonly isConnected: boolean;
}

// ---------------------------------------------------------------------------
// Socket path helpers
// ---------------------------------------------------------------------------

/**
 * Returns the platform-appropriate IPC socket path or pipe name for the given
 * session ID.
 *
 * - Linux/macOS: `/tmp/retro-oasis-melonds-<sessionId>.sock`
 * - Windows:     `\\.\pipe\retro-oasis-melonds-<sessionId>`
 */
export function melonDsSocketPath(sessionId: string): string {
  if (os.platform() === 'win32') {
    return `\\\\.\\pipe\\retro-oasis-melonds-${sessionId}`;
  }
  return path.join(os.tmpdir(), `retro-oasis-melonds-${sessionId}.sock`);
}

// ---------------------------------------------------------------------------
// Real socket bridge implementation
// ---------------------------------------------------------------------------

/** Timeout (ms) to wait for a response from the C++ core per command. */
const RESPONSE_TIMEOUT_MS = 5000;

/** Maximum time (ms) to wait while connecting to the socket before failing. */
const CONNECT_TIMEOUT_MS = 3000;

/**
 * `MelonDsSocketBridge` is the production IPC implementation.
 *
 * It opens a Unix domain socket (or Windows named pipe) to a running
 * melonDS process, sends newline-delimited JSON commands, and parses
 * the newline-delimited JSON responses.
 *
 * Call `connect(sessionId)` first, then `send(command)` for each
 * interaction, and `disconnect()` to clean up.
 */
export class MelonDsSocketBridge implements MelonDsIpcBridge {
  private _socket: net.Socket | null = null;
  private _buffer = '';
  private _pending: Map<number, {
    resolve: (r: MelonDsResponse) => void;
    reject: (e: Error) => void;
    timer: ReturnType<typeof setTimeout>;
  }> = new Map();
  private _seq = 0;

  get isConnected(): boolean {
    return this._socket !== null && !this._socket.destroyed;
  }

  async connect(sessionId: string): Promise<void> {
    const socketPath = melonDsSocketPath(sessionId);

    return new Promise<void>((resolve, reject) => {
      const socket = net.createConnection(socketPath);

      const connectTimer = setTimeout(() => {
        socket.destroy();
        reject(new Error(`[melonDS IPC] connect timeout for session ${sessionId}`));
      }, CONNECT_TIMEOUT_MS);

      socket.once('connect', () => {
        clearTimeout(connectTimer);
        this._socket = socket;
        this._buffer = '';
        this._attachListeners();
        resolve();
      });

      socket.once('error', (err) => {
        clearTimeout(connectTimer);
        reject(new Error(`[melonDS IPC] connect error: ${err.message}`));
      });
    });
  }

  async send(command: MelonDsCommand): Promise<MelonDsResponse> {
    if (!this._socket || this._socket.destroyed) {
      throw new Error('[melonDS IPC] not connected');
    }

    const seq = ++this._seq;
    const frame = JSON.stringify({ seq, ...command }) + '\n';

    return new Promise<MelonDsResponse>((resolve, reject) => {
      const timer = setTimeout(() => {
        this._pending.delete(seq);
        reject(new Error(`[melonDS IPC] response timeout for command: ${command.type}`));
      }, RESPONSE_TIMEOUT_MS);

      this._pending.set(seq, { resolve, reject, timer });
      this._socket!.write(frame);
    });
  }

  async disconnect(terminate = false): Promise<void> {
    if (!this._socket) return;
    if (terminate && !this._socket.destroyed) {
      try {
        await this.send({ type: 'stop' }).catch(() => {/* ignore errors on shutdown */});
      } catch {
        // best-effort
      }
    }
    this._rejectAll(new Error('[melonDS IPC] disconnected'));
    this._socket.destroy();
    this._socket = null;
  }

  // ── Private helpers ──────────────────────────────────────────────────────

  private _attachListeners(): void {
    const socket = this._socket!;

    socket.on('data', (chunk: Buffer) => {
      this._buffer += chunk.toString('utf8');
      const lines = this._buffer.split('\n');
      this._buffer = lines.pop() ?? '';
      for (const line of lines) {
        const trimmed = line.trim();
        if (!trimmed) continue;
        try {
          const msg = JSON.parse(trimmed) as MelonDsResponse & { seq?: number };
          const seq = msg.seq;
          if (seq !== undefined && this._pending.has(seq)) {
            const entry = this._pending.get(seq)!;
            clearTimeout(entry.timer);
            this._pending.delete(seq);
            // Remove seq from the response object before resolving
            const { seq: _, ...response } = msg;
            entry.resolve(response as MelonDsResponse);
          }
        } catch {
          // Malformed frame — ignore silently
        }
      }
    });

    socket.on('close', () => {
      this._rejectAll(new Error('[melonDS IPC] connection closed'));
      this._socket = null;
    });

    socket.on('error', (err) => {
      this._rejectAll(new Error(`[melonDS IPC] socket error: ${err.message}`));
    });
  }

  private _rejectAll(err: Error): void {
    for (const entry of this._pending.values()) {
      clearTimeout(entry.timer);
      entry.reject(err);
    }
    this._pending.clear();
  }
}

// ---------------------------------------------------------------------------
// Stub implementation  (development / CI — no native binary required)
// ---------------------------------------------------------------------------

/**
 * `MelonDsIpcStub` is a no-op implementation of `MelonDsIpcBridge` that logs
 * all commands and returns predictable placeholder responses. It is used:
 *
 *  - During frontend development when the C++ binary isn't compiled.
 *  - In unit tests that exercise session-engine logic without a real emulator.
 *  - As the fallback when `createMelonDsIpc` is called with `forceStub: true`.
 */
export class MelonDsIpcStub implements MelonDsIpcBridge {
  private _connected = false;

  get isConnected(): boolean {
    return this._connected;
  }

  async connect(sessionId: string): Promise<void> {
    console.warn(
      `[melonDS IPC stub] connect(${sessionId}) — ` +
      'C++ IPC bridge not yet implemented. Using stub.',
    );
    this._connected = true;
  }

  async send(command: MelonDsCommand): Promise<MelonDsResponse> {
    console.warn('[melonDS IPC stub] send:', command.type, '— returning stub response');

    switch (command.type) {
      case 'ping':
        return { type: 'pong', uptimeMs: 0 };
      case 'stop':
      case 'pause':
      case 'resume':
      case 'setScreenLayout':
        return { type: 'ok' };
      case 'launch':
        return { type: 'ok' };
      case 'saveState':
        return { type: 'saveStateDone', slotIndex: command.slotIndex, path: '/dev/null' };
      case 'loadState':
        return { type: 'loadStateDone', slotIndex: command.slotIndex };
      default:
        return { type: 'error', message: 'Unknown command (stub)' };
    }
  }

  async disconnect(terminate = false): Promise<void> {
    console.warn(`[melonDS IPC stub] disconnect(terminate=${terminate})`);
    this._connected = false;
  }
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

/**
 * Create the appropriate melonDS IPC bridge for the current environment.
 *
 * @param _sessionId  The session ID — used to derive the socket path in the
 *                    real implementation (currently unused by the factory).
 * @param opts.forceStub  When `true`, always return a `MelonDsIpcStub`.
 *                        Useful in tests / CI where no native binary exists.
 *
 * The factory currently returns a stub by default because the C++ native
 * binary is not yet compiled into the workspace. Switch the default once
 * the CMake build is wired into `npm run build`.
 */
export function createMelonDsIpc(
  _sessionId: string,
  opts: { forceStub?: boolean } = {},
): MelonDsIpcBridge {
  if (opts.forceStub) return new MelonDsIpcStub();
  // Return the real bridge when MELONDS_IPC_ENABLED is set; stub otherwise.
  if (process.env.MELONDS_IPC_ENABLED === '1') return new MelonDsSocketBridge();
  return new MelonDsIpcStub();
}

/** Default export: stub until the real bridge is wired up via MELONDS_IPC_ENABLED=1. */
export const melonDsIpc: MelonDsIpcBridge = new MelonDsIpcStub();
