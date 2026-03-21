/**
 * Phase 2 — Emulator Bridge Trustworthiness
 *
 * Tests for:
 *  - BackendCapabilities interface present on every BackendDefinition
 *  - Capability matrix correctness for the 5 priority backends:
 *      mGBA, FCEUX, Snes9x, Mupen64Plus, melonDS
 *  - Link-cable capability restricted to backends that expose a TCP link flag
 *      (mGBA, SameBoy, VisualBoyAdvance-M only)
 *  - All backends have localLaunch: true
 *  - EmulatorBridge extends EventEmitter (structured lifecycle events)
 *  - EmulatorLifecycleEvent shape
 *  - EmulatorBridge.launch() emits a 'launched' event via the 'event' channel
 *  - EmulatorBridge.stop() emits a 'stopped' event and sends SIGTERM
 *  - Hard-kill fallback: SIGKILL is sent when the process does not exit
 *    within the grace period
 *  - Natural process exit emits an 'exited' event and cancels the kill timer
 *  - Process error emits an 'error' event
 *  - Per-backend argument builders for the 5 priority backends
 */

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { EventEmitter } from 'events';
import { KNOWN_BACKENDS } from '../../../../packages/emulator-bridge/src/backends';
import type { BackendCapabilities } from '../../../../packages/emulator-bridge/src/backends';
import { createSystemAdapter } from '../../../../packages/emulator-bridge/src/adapters';
import { EmulatorBridge } from '../../../../packages/emulator-bridge/src/bridge';
import type { EmulatorLifecycleEvent } from '../../../../packages/emulator-bridge/src/bridge';

// ---------------------------------------------------------------------------
// Mock child_process.spawn so no real emulator binary is needed
// ---------------------------------------------------------------------------

// We need a controllable fake ChildProcess for lifecycle tests
function makeMockChild(opts: { exitImmediately?: boolean } = {}) {
  const ee = new EventEmitter() as ReturnType<typeof makeMockChild>;
  (ee as any).pid = 99999;
  (ee as any).killed = false;
  (ee as any).kill = vi.fn((signal?: string) => {
    (ee as any).killed = true;
    // Simulate graceful exit on SIGTERM (unless test opts in to "hang" behaviour)
    if (opts.exitImmediately !== false) {
      setImmediate(() => ee.emit('exit', signal === 'SIGKILL' ? null : 0));
    }
  });
  return ee;
}

vi.mock('child_process', () => ({
  spawn: vi.fn(),
}));

import { spawn } from 'child_process';
const mockSpawn = spawn as ReturnType<typeof vi.fn>;

beforeEach(() => {
  vi.clearAllMocks();
});

afterEach(() => {
  vi.clearAllTimers();
});

// ---------------------------------------------------------------------------
// BackendCapabilities — all backends must declare them
// ---------------------------------------------------------------------------

describe('BackendCapabilities presence', () => {
  it('every BackendDefinition has a capabilities object', () => {
    for (const backend of KNOWN_BACKENDS) {
      expect(
        backend.capabilities,
        `Backend '${backend.id}' is missing capabilities`
      ).toBeDefined();
    }
  });

  it('capabilities object has all required fields', () => {
    const requiredFields: (keyof BackendCapabilities)[] = [
      'localLaunch',
      'netplay',
      'savePath',
      'controllerProfile',
      'linkCable',
    ];
    for (const backend of KNOWN_BACKENDS) {
      for (const field of requiredFields) {
        expect(
          typeof backend.capabilities[field],
          `Backend '${backend.id}' capabilities.${field} should be boolean`
        ).toBe('boolean');
      }
    }
  });

  it('all backends support local launch', () => {
    for (const backend of KNOWN_BACKENDS) {
      expect(
        backend.capabilities.localLaunch,
        `Backend '${backend.id}' should have localLaunch: true`
      ).toBe(true);
    }
  });

  it('all backends support save path', () => {
    for (const backend of KNOWN_BACKENDS) {
      expect(
        backend.capabilities.savePath,
        `Backend '${backend.id}' should have savePath: true`
      ).toBe(true);
    }
  });
});

// ---------------------------------------------------------------------------
// Capability matrix — 5 priority backends
// ---------------------------------------------------------------------------

describe('mGBA capabilities', () => {
  const mgba = KNOWN_BACKENDS.find((b) => b.id === 'mgba')!;

  it('mGBA exists in KNOWN_BACKENDS', () => expect(mgba).toBeDefined());
  it('mGBA localLaunch is true', () => expect(mgba.capabilities.localLaunch).toBe(true));
  it('mGBA netplay is true', () => expect(mgba.capabilities.netplay).toBe(true));
  it('mGBA savePath is true', () => expect(mgba.capabilities.savePath).toBe(true));
  it('mGBA controllerProfile is true', () => expect(mgba.capabilities.controllerProfile).toBe(true));
  it('mGBA linkCable is true (--link-host TCP)', () => expect(mgba.capabilities.linkCable).toBe(true));
});

describe('FCEUX capabilities', () => {
  const fceux = KNOWN_BACKENDS.find((b) => b.id === 'fceux')!;

  it('FCEUX exists in KNOWN_BACKENDS', () => expect(fceux).toBeDefined());
  it('FCEUX localLaunch is true', () => expect(fceux.capabilities.localLaunch).toBe(true));
  it('FCEUX netplay is true (--net flag)', () => expect(fceux.capabilities.netplay).toBe(true));
  it('FCEUX savePath is true', () => expect(fceux.capabilities.savePath).toBe(true));
  it('FCEUX controllerProfile is true', () => expect(fceux.capabilities.controllerProfile).toBe(true));
  it('FCEUX linkCable is false', () => expect(fceux.capabilities.linkCable).toBe(false));
});

describe('Snes9x capabilities', () => {
  const snes9x = KNOWN_BACKENDS.find((b) => b.id === 'snes9x')!;

  it('Snes9x exists in KNOWN_BACKENDS', () => expect(snes9x).toBeDefined());
  it('Snes9x localLaunch is true', () => expect(snes9x.capabilities.localLaunch).toBe(true));
  it('Snes9x netplay is true (-netplay flag)', () => expect(snes9x.capabilities.netplay).toBe(true));
  it('Snes9x savePath is true', () => expect(snes9x.capabilities.savePath).toBe(true));
  it('Snes9x controllerProfile is true', () => expect(snes9x.capabilities.controllerProfile).toBe(true));
  it('Snes9x linkCable is false', () => expect(snes9x.capabilities.linkCable).toBe(false));
});

describe('Mupen64Plus capabilities', () => {
  const mupen = KNOWN_BACKENDS.find((b) => b.id === 'mupen64plus')!;

  it('Mupen64Plus exists in KNOWN_BACKENDS', () => expect(mupen).toBeDefined());
  it('Mupen64Plus localLaunch is true', () => expect(mupen.capabilities.localLaunch).toBe(true));
  it('Mupen64Plus netplay is true (--netplay-host flag)', () => expect(mupen.capabilities.netplay).toBe(true));
  it('Mupen64Plus savePath is true', () => expect(mupen.capabilities.savePath).toBe(true));
  it('Mupen64Plus controllerProfile is true', () => expect(mupen.capabilities.controllerProfile).toBe(true));
  it('Mupen64Plus linkCable is false (no link cable on N64)', () => expect(mupen.capabilities.linkCable).toBe(false));
});

describe('melonDS capabilities', () => {
  const melonds = KNOWN_BACKENDS.find((b) => b.id === 'melonds')!;

  it('melonDS exists in KNOWN_BACKENDS', () => expect(melonds).toBeDefined());
  it('melonDS localLaunch is true', () => expect(melonds.capabilities.localLaunch).toBe(true));
  it('melonDS netplay is true (--wifi-host flag)', () => expect(melonds.capabilities.netplay).toBe(true));
  it('melonDS savePath is true', () => expect(melonds.capabilities.savePath).toBe(true));
  it('melonDS controllerProfile is true', () => expect(melonds.capabilities.controllerProfile).toBe(true));
  it('melonDS linkCable is false (NDS uses WiFi relay, not link cable)', () => expect(melonds.capabilities.linkCable).toBe(false));
});

// ---------------------------------------------------------------------------
// Link cable — only mGBA, SameBoy, and VBA-M
// ---------------------------------------------------------------------------

describe('link cable capability', () => {
  const linkCableBackends = ['mgba', 'sameboy', 'vbam'];

  it('exactly mGBA, SameBoy, and VBA-M have linkCable: true', () => {
    const withLink = KNOWN_BACKENDS.filter((b) => b.capabilities.linkCable).map((b) => b.id);
    expect(withLink.sort()).toEqual(linkCableBackends.sort());
  });

  it('SameBoy linkCable is true (--link-address TCP)', () => {
    const sameboy = KNOWN_BACKENDS.find((b) => b.id === 'sameboy')!;
    expect(sameboy.capabilities.linkCable).toBe(true);
  });

  it('VBA-M linkCable is true (--link-host TCP)', () => {
    const vbam = KNOWN_BACKENDS.find((b) => b.id === 'vbam')!;
    expect(vbam.capabilities.linkCable).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Per-backend argument builders — 5 priority backends
// ---------------------------------------------------------------------------

describe('mGBA argument builder', () => {
  it('includes -f and ROM path', () => {
    const adapter = createSystemAdapter('gba', 'mgba');
    const args = adapter.buildLaunchArgs('/roms/game.gba', {});
    expect(args).toContain('-f');
    expect(args).toContain('/roms/game.gba');
  });

  it('adds --link-host for netplay', () => {
    const adapter = createSystemAdapter('gba', 'mgba');
    const args = adapter.buildLaunchArgs('/roms/game.gba', {
      netplayHost: '127.0.0.1',
      netplayPort: 9001,
    });
    expect(args).toContain('--link-host');
    expect(args.some((a) => a.includes('127.0.0.1:9001'))).toBe(true);
  });

  it('adds --gdb 2345 in debug mode', () => {
    const adapter = createSystemAdapter('gba', 'mgba');
    const args = adapter.buildLaunchArgs('/roms/game.gba', { debug: true });
    expect(args).toContain('--gdb');
    expect(args).toContain('2345');
  });
});

describe('FCEUX argument builder', () => {
  it('includes ROM path', () => {
    const adapter = createSystemAdapter('nes', 'fceux');
    const args = adapter.buildLaunchArgs('/roms/game.nes', {});
    expect(args).toContain('/roms/game.nes');
  });

  it('adds --net host:port for netplay', () => {
    const adapter = createSystemAdapter('nes', 'fceux');
    const args = adapter.buildLaunchArgs('/roms/game.nes', {
      netplayHost: '127.0.0.1',
      netplayPort: 9000,
    });
    expect(args).toContain('--net');
    expect(args.some((a) => a.includes('127.0.0.1:9000'))).toBe(true);
  });

  it('adds --player flag with correct slot', () => {
    const adapter = createSystemAdapter('nes', 'fceux');
    const args = adapter.buildLaunchArgs('/roms/game.nes', {
      netplayHost: '127.0.0.1',
      netplayPort: 9000,
      playerSlot: 1,
    });
    expect(args).toContain('--player');
    expect(args).toContain('2');
  });

  it('adds --debug in debug mode', () => {
    const adapter = createSystemAdapter('nes', 'fceux');
    const args = adapter.buildLaunchArgs('/roms/game.nes', { debug: true });
    expect(args).toContain('--debug');
  });
});

describe('Snes9x argument builder', () => {
  it('includes ROM path', () => {
    const adapter = createSystemAdapter('snes', 'snes9x');
    const args = adapter.buildLaunchArgs('/roms/game.sfc', {});
    expect(args).toContain('/roms/game.sfc');
  });

  it('adds -netplay host port for netplay', () => {
    const adapter = createSystemAdapter('snes', 'snes9x');
    const args = adapter.buildLaunchArgs('/roms/game.sfc', {
      netplayHost: '127.0.0.1',
      netplayPort: 9002,
    });
    expect(args).toContain('-netplay');
    expect(args).toContain('127.0.0.1');
    expect(args).toContain('9002');
  });

  it('adds -v in debug mode', () => {
    const adapter = createSystemAdapter('snes', 'snes9x');
    const args = adapter.buildLaunchArgs('/roms/game.sfc', { debug: true });
    expect(args).toContain('-v');
  });
});

describe('Mupen64Plus argument builder', () => {
  it('includes --rom and ROM path', () => {
    const adapter = createSystemAdapter('n64', 'mupen64plus');
    const args = adapter.buildLaunchArgs('/roms/game.z64', {});
    expect(args).toContain('--rom');
    expect(args).toContain('/roms/game.z64');
  });

  it('adds --netplay-host, --netplay-port, --netplay-player', () => {
    const adapter = createSystemAdapter('n64', 'mupen64plus');
    const args = adapter.buildLaunchArgs('/roms/game.z64', {
      netplayHost: '127.0.0.1',
      netplayPort: 9003,
      playerSlot: 0,
    });
    expect(args).toContain('--netplay-host');
    expect(args).toContain('127.0.0.1');
    expect(args).toContain('--netplay-port');
    expect(args).toContain('9003');
    expect(args).toContain('--netplay-player');
    expect(args).toContain('1'); // slot 0 → player 1 (1-based)
  });

  it('adds --verbose in debug mode', () => {
    const adapter = createSystemAdapter('n64', 'mupen64plus');
    const args = adapter.buildLaunchArgs('/roms/game.z64', { debug: true });
    expect(args).toContain('--verbose');
  });
});

describe('melonDS argument builder', () => {
  it('includes ROM path', () => {
    const adapter = createSystemAdapter('nds', 'melonds');
    const args = adapter.buildLaunchArgs('/roms/game.nds', {});
    expect(args).toContain('/roms/game.nds');
  });

  it('adds --wifi-host and --wifi-port for netplay', () => {
    const adapter = createSystemAdapter('nds', 'melonds');
    const args = adapter.buildLaunchArgs('/roms/game.nds', {
      netplayHost: '127.0.0.1',
      netplayPort: 9004,
    });
    expect(args).toContain('--wifi-host');
    expect(args).toContain('127.0.0.1');
    expect(args).toContain('--wifi-port');
    expect(args).toContain('9004');
  });

  it('adds --screen-layout flag', () => {
    const adapter = createSystemAdapter('nds', 'melonds');
    const args = adapter.buildLaunchArgs('/roms/game.nds', {
      screenLayout: 'side-by-side',
    });
    expect(args.some((a) => a.includes('--screen-layout=side-by-side'))).toBe(true);
  });

  it('adds --verbose in debug mode', () => {
    const adapter = createSystemAdapter('nds', 'melonds');
    const args = adapter.buildLaunchArgs('/roms/game.nds', { debug: true });
    expect(args).toContain('--verbose');
  });
});

// ---------------------------------------------------------------------------
// EmulatorBridge — EventEmitter + structured lifecycle events
// ---------------------------------------------------------------------------

describe('EmulatorBridge inherits EventEmitter', () => {
  it('is an instance of EventEmitter', () => {
    const bridge = new EmulatorBridge();
    expect(bridge).toBeInstanceOf(EventEmitter);
  });

  it('has on/off/emit methods', () => {
    const bridge = new EmulatorBridge();
    expect(typeof bridge.on).toBe('function');
    expect(typeof bridge.off).toBe('function');
    expect(typeof bridge.emit).toBe('function');
  });
});

describe('EmulatorLifecycleEvent shape', () => {
  it('launched event has required fields', async () => {
    const mockChild = makeMockChild();
    mockSpawn.mockReturnValue(mockChild);

    const bridge = new EmulatorBridge();
    const events: EmulatorLifecycleEvent[] = [];
    bridge.on('event', (e: EmulatorLifecycleEvent) => events.push(e));

    await bridge.launch({
      romPath: '/roms/game.nes',
      system: 'nes',
      backendId: 'fceux',
      saveDirectory: '/saves',
    });

    const launched = events.find((e) => e.type === 'launched');
    expect(launched).toBeDefined();
    expect(launched!.sessionId).toBeTruthy();
    expect(launched!.backendId).toBe('fceux');
    expect(launched!.system).toBe('nes');
    expect(typeof launched!.pid).toBe('number');
    expect(typeof launched!.timestamp).toBe('string');
  });
});

describe('EmulatorBridge.launch() lifecycle events', () => {
  it('emits launched event after successful spawn', async () => {
    const mockChild = makeMockChild();
    mockSpawn.mockReturnValue(mockChild);

    const bridge = new EmulatorBridge();
    const events: EmulatorLifecycleEvent[] = [];
    bridge.on('event', (e) => events.push(e));

    const result = await bridge.launch({
      romPath: '/roms/game.nes',
      system: 'nes',
      backendId: 'fceux',
      saveDirectory: '/saves',
    });

    expect(result.success).toBe(true);
    expect(events.some((e) => e.type === 'launched')).toBe(true);
  });

  it('does not emit launched when adapter throws', async () => {
    const bridge = new EmulatorBridge();
    const events: EmulatorLifecycleEvent[] = [];
    bridge.on('event', (e) => events.push(e));

    const result = await bridge.launch({
      romPath: '/roms/game.xyz',
      system: 'unknownsystem',
      backendId: 'fakeid',
      saveDirectory: '/saves',
    });

    expect(result.success).toBe(false);
    expect(events).toHaveLength(0);
  });

  it('emits exited event when process exits naturally', async () => {
    const mockChild = makeMockChild({ exitImmediately: false });
    mockSpawn.mockReturnValue(mockChild);

    const bridge = new EmulatorBridge();
    const events: EmulatorLifecycleEvent[] = [];
    bridge.on('event', (e) => events.push(e));

    await bridge.launch({
      romPath: '/roms/game.nes',
      system: 'nes',
      backendId: 'fceux',
      saveDirectory: '/saves',
    });

    // Simulate natural exit
    mockChild.emit('exit', 0);

    const exited = events.find((e) => e.type === 'exited');
    expect(exited).toBeDefined();
    expect(exited!.exitCode).toBe(0);
  });

  it('emits error event when child process emits OS error', async () => {
    const mockChild = makeMockChild({ exitImmediately: false });
    mockSpawn.mockReturnValue(mockChild);

    const bridge = new EmulatorBridge();
    const events: EmulatorLifecycleEvent[] = [];
    bridge.on('event', (e) => events.push(e));

    await bridge.launch({
      romPath: '/roms/game.nes',
      system: 'nes',
      backendId: 'fceux',
      saveDirectory: '/saves',
    });

    // Simulate OS error (e.g. ENOENT)
    mockChild.emit('error', new Error('ENOENT: no such file or directory'));

    const errorEvent = events.find((e) => e.type === 'error');
    expect(errorEvent).toBeDefined();
    expect(errorEvent!.errorMessage).toContain('ENOENT');
  });
});

describe('EmulatorBridge.stop() lifecycle events', () => {
  it('emits stopped event when stop() is called', async () => {
    const mockChild = makeMockChild({ exitImmediately: false });
    mockSpawn.mockReturnValue(mockChild);

    const bridge = new EmulatorBridge();
    const events: EmulatorLifecycleEvent[] = [];
    bridge.on('event', (e) => events.push(e));

    const result = await bridge.launch({
      romPath: '/roms/game.nes',
      system: 'nes',
      backendId: 'fceux',
      saveDirectory: '/saves',
    });

    const sessionId = result.process!.sessionId;
    await bridge.stop(sessionId);

    expect(events.some((e) => e.type === 'stopped')).toBe(true);
  });

  it('sends SIGTERM on stop()', async () => {
    const mockChild = makeMockChild({ exitImmediately: false });
    mockSpawn.mockReturnValue(mockChild);

    const bridge = new EmulatorBridge();
    const result = await bridge.launch({
      romPath: '/roms/game.nes',
      system: 'nes',
      backendId: 'fceux',
      saveDirectory: '/saves',
    });

    await bridge.stop(result.process!.sessionId);
    expect((mockChild as any).kill).toHaveBeenCalledWith('SIGTERM');
  });

  it('returns false for unknown sessionId', async () => {
    const bridge = new EmulatorBridge();
    const stopped = await bridge.stop('non-existent-session-id');
    expect(stopped).toBe(false);
  });
});

describe('hard kill fallback', () => {
  it('sends SIGKILL when process does not exit within grace period', async () => {
    vi.useFakeTimers();

    // Create a child that never exits on kill
    const mockChild = makeMockChild({ exitImmediately: false });
    // Override kill to NOT emit exit — simulates a hung process
    (mockChild as any).kill = vi.fn();
    mockSpawn.mockReturnValue(mockChild);

    const bridge = new EmulatorBridge();
    const events: EmulatorLifecycleEvent[] = [];
    bridge.on('event', (e) => events.push(e));

    const result = await bridge.launch({
      romPath: '/roms/game.nes',
      system: 'nes',
      backendId: 'fceux',
      saveDirectory: '/saves',
    });

    // Stop with a very short grace period for the test
    bridge.stop(result.process!.sessionId, 1000);

    // Advance timers past the grace period
    vi.advanceTimersByTime(1500);

    expect((mockChild as any).kill).toHaveBeenCalledWith('SIGTERM');
    expect((mockChild as any).kill).toHaveBeenCalledWith('SIGKILL');
    expect(events.some((e) => e.type === 'kill-forced')).toBe(true);

    vi.useRealTimers();
  });

  it('does NOT send SIGKILL when process exits before grace period expires', async () => {
    vi.useFakeTimers();

    const mockChild = makeMockChild({ exitImmediately: false });
    // kill() causes the process to exit immediately (graceful shutdown)
    (mockChild as any).kill = vi.fn((signal?: string) => {
      (mockChild as any).killed = true;
      mockChild.emit('exit', 0);
    });
    mockSpawn.mockReturnValue(mockChild);

    const bridge = new EmulatorBridge();
    const events: EmulatorLifecycleEvent[] = [];
    bridge.on('event', (e) => events.push(e));

    const result = await bridge.launch({
      romPath: '/roms/game.nes',
      system: 'nes',
      backendId: 'fceux',
      saveDirectory: '/saves',
    });

    bridge.stop(result.process!.sessionId, 5000);

    // SIGTERM was sent; process exits; timer should be cancelled
    vi.advanceTimersByTime(6000);

    const kills = ((mockChild as any).kill as ReturnType<typeof vi.fn>).mock.calls;
    const sigkillCalls = kills.filter(([sig]: [string]) => sig === 'SIGKILL');
    expect(sigkillCalls).toHaveLength(0);
    expect(events.some((e) => e.type === 'kill-forced')).toBe(false);

    vi.useRealTimers();
  });
});
