/**
 * melonDS IPC bridge — TypeScript interface for the C++ core.
 *
 * The melonDS emulator core lives in `/src` of this repository (forked from
 * the upstream melonDS project). This module defines the **intended IPC
 * protocol** that will connect the TypeScript launcher / lobby server to the
 * C++ core once the native build and IPC channel are in place.
 *
 * ## Carry-over blocker
 * Full C++ build integration and IPC plumbing are an explicit Phase 3 blocker.
 * The remaining work is:
 *   1. Integrate the CMake build (`CMakeLists.txt`) into the npm workspace so
 *      `npm run build` (or a Tauri build hook) also compiles the C++ core.
 *   2. Implement the IPC server in C++ (Unix domain socket or named pipe).
 *   3. Replace this stub implementation with a real socket client.
 *   4. Wire Tauri's sidecar or child_process launch into the bridge so the
 *      TypeScript side can start and control the melonDS process.
 *
 * ## IPC transport
 * The planned transport is a Unix domain socket (Linux/macOS) or a named pipe
 * (Windows). The C++ side will bind to a well-known path derived from the
 * session ID so multiple instances can coexist.
 *
 * Socket path convention: `/tmp/retro-oasis-melonds-<sessionId>.sock`
 * Windows pipe name:      `\\.\pipe\retro-oasis-melonds-<sessionId>`
 */

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
// Stub implementation  (development / CI — no native binary required)
// ---------------------------------------------------------------------------

/**
 * `MelonDsIpcStub` is a no-op implementation of `MelonDsIpcBridge` that logs
 * all commands and returns predictable placeholder responses. It is used:
 *
 *  - During frontend development when the C++ binary isn't compiled.
 *  - In unit tests that exercise session-engine logic without a real emulator.
 *  - As the default export until the real socket bridge is implemented.
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

/** Default export: stub until the real bridge is implemented. */
export const melonDsIpc: MelonDsIpcBridge = new MelonDsIpcStub();
