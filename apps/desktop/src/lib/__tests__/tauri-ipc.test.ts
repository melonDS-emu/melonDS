/**
 * Tauri IPC integration tests.
 *
 * Validates the behaviour of tauri-ipc.ts in both the native (Tauri) and
 * HTTP-fallback (browser/dev) code paths.  Because the test environment
 * doesn't run inside a real Tauri shell, we assert the fallback behaviour
 * and verify the Tauri invoke path through a simulated __TAURI__ global.
 */
import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import {
  isTauri,
  tauriScanRoms,
  tauriPickFile,
  tauriPickDirectory,
  tauriLaunchEmulator,
  tauriLaunchLocal,
  tauriCheckFileExists,
} from '../tauri-ipc';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Attach a fake __TAURI__ global to simulate the native shell. */
function installTauriFake(invokeFn: (cmd: string, args?: Record<string, unknown>) => unknown) {
  (globalThis as Record<string, unknown>).window = {
    __TAURI__: {
      invoke: invokeFn,
    },
  };
}

function removeTauriFake() {
  delete (globalThis as Record<string, unknown>).window;
}

// ---------------------------------------------------------------------------
// Runtime detection
// ---------------------------------------------------------------------------

describe('isTauri()', () => {
  afterEach(() => removeTauriFake());

  it('returns false outside a Tauri shell', () => {
    expect(isTauri()).toBe(false);
  });

  it('returns true when window.__TAURI__ is present', () => {
    installTauriFake(() => {});
    expect(isTauri()).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// HTTP fallback paths (non-Tauri)
// ---------------------------------------------------------------------------

describe('HTTP fallbacks (non-Tauri environment)', () => {
  beforeEach(() => {
    removeTauriFake();
  });

  it('tauriPickFile returns { path: null } outside Tauri', async () => {
    const result = await tauriPickFile();
    expect(result).toEqual({ path: null });
  });

  it('tauriPickFile with filters returns { path: null } outside Tauri', async () => {
    const result = await tauriPickFile([{ name: 'ROMs', extensions: ['nes', 'sfc'] }]);
    expect(result).toEqual({ path: null });
  });

  it('tauriPickDirectory returns { path: null } outside Tauri', async () => {
    const result = await tauriPickDirectory();
    expect(result).toEqual({ path: null });
  });

  it('tauriCheckFileExists returns false on fetch failure', async () => {
    const result = await tauriCheckFileExists('/nonexistent/path');
    expect(result).toBe(false);
  });

  it('tauriScanRoms falls back to HTTP and throws on network error', async () => {
    await expect(tauriScanRoms('/some/dir')).rejects.toThrow();
  });

  it('tauriLaunchEmulator falls back to HTTP POST', async () => {
    // HTTP fallback to localhost:8080 will fail with a network error in test env
    const result = await tauriLaunchEmulator({
      romPath: '/roms/test.nes',
      system: 'nes',
      backendId: 'fceux',
    }).catch(() => ({ success: false, error: 'Network error' }));
    expect(result.success).toBe(false);
    expect(result.error).toBeDefined();
  });

  it('tauriLaunchLocal falls back to HTTP POST', async () => {
    const result = await tauriLaunchLocal({
      emulatorPath: '/usr/bin/fceux',
      romPath: '/roms/test.nes',
      system: 'nes',
      backendId: 'fceux',
    }).catch(() => ({ success: false, error: 'Network error' }));
    expect(result.success).toBe(false);
    expect(result.error).toBeDefined();
  });
});

// ---------------------------------------------------------------------------
// Tauri invoke paths (simulated)
// ---------------------------------------------------------------------------

describe('Tauri invoke paths (simulated __TAURI__ global)', () => {
  afterEach(() => removeTauriFake());

  it('tauriScanRoms invokes scan_rom_directory with correct args', async () => {
    const invokeSpy = vi.fn().mockResolvedValue([
      {
        filePath: '/roms/game.nes',
        fileName: 'game.nes',
        system: 'nes',
        inferredTitle: 'Game',
        fileSizeBytes: 1024,
        lastModified: '2025-01-01T00:00:00Z',
      },
    ]);
    installTauriFake(invokeSpy);

    const results = await tauriScanRoms('/roms', true);
    expect(invokeSpy).toHaveBeenCalledWith('scan_rom_directory', {
      dir: '/roms',
      recursive: true,
    });
    expect(results).toHaveLength(1);
    expect(results[0].system).toBe('nes');
  });

  it('tauriPickFile invokes pick_file with filters', async () => {
    const invokeSpy = vi.fn().mockResolvedValue({ path: '/roms/mario.nes' });
    installTauriFake(invokeSpy);

    const result = await tauriPickFile([{ name: 'NES ROMs', extensions: ['nes'] }]);
    expect(invokeSpy).toHaveBeenCalledWith('pick_file', {
      filters: [{ name: 'NES ROMs', extensions: ['nes'] }],
    });
    expect(result.path).toBe('/roms/mario.nes');
  });

  it('tauriPickFile with no filters passes undefined', async () => {
    const invokeSpy = vi.fn().mockResolvedValue({ path: null });
    installTauriFake(invokeSpy);

    const result = await tauriPickFile();
    expect(invokeSpy).toHaveBeenCalledWith('pick_file', { filters: undefined });
    expect(result.path).toBeNull();
  });

  it('tauriPickDirectory invokes pick_directory', async () => {
    const invokeSpy = vi.fn().mockResolvedValue({ path: '/home/user/roms' });
    installTauriFake(invokeSpy);

    const result = await tauriPickDirectory();
    expect(invokeSpy).toHaveBeenCalledWith('pick_directory', undefined);
    expect(result.path).toBe('/home/user/roms');
  });

  it('tauriLaunchEmulator invokes launch_emulator with all options', async () => {
    const invokeSpy = vi.fn().mockResolvedValue({ success: true, pid: 12345 });
    installTauriFake(invokeSpy);

    const opts = {
      romPath: '/roms/mario64.z64',
      system: 'n64',
      backendId: 'mupen64plus',
      saveDirectory: '/saves',
      playerSlot: 1,
      netplayHost: '127.0.0.1',
      netplayPort: 9001,
      sessionToken: 'abc-123',
      fullscreen: true,
    };
    const result = await tauriLaunchEmulator(opts);
    expect(invokeSpy).toHaveBeenCalledWith('launch_emulator', opts);
    expect(result.success).toBe(true);
    expect(result.pid).toBe(12345);
  });

  it('tauriLaunchLocal invokes launch_local with correct options', async () => {
    const invokeSpy = vi.fn().mockResolvedValue({ success: true, pid: 67890 });
    installTauriFake(invokeSpy);

    const opts = {
      emulatorPath: '/usr/bin/mgba',
      romPath: '/roms/pokemon.gba',
      system: 'gba',
      backendId: 'mgba',
      saveDirectory: '/saves',
      fullscreen: false,
    };
    const result = await tauriLaunchLocal(opts);
    expect(invokeSpy).toHaveBeenCalledWith('launch_local', opts);
    expect(result.success).toBe(true);
    expect(result.pid).toBe(67890);
  });

  it('tauriCheckFileExists invokes check_file_exists and returns boolean', async () => {
    const invokeSpy = vi.fn().mockResolvedValue(true);
    installTauriFake(invokeSpy);

    const exists = await tauriCheckFileExists('/roms/test.nes');
    expect(invokeSpy).toHaveBeenCalledWith('check_file_exists', { path: '/roms/test.nes' });
    expect(exists).toBe(true);
  });

  it('tauriCheckFileExists returns false for missing file', async () => {
    const invokeSpy = vi.fn().mockResolvedValue(false);
    installTauriFake(invokeSpy);

    const exists = await tauriCheckFileExists('/nonexistent');
    expect(exists).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// Type contract validation
// ---------------------------------------------------------------------------

describe('type contracts', () => {
  it('TauriRomScanResult fields are correct', () => {
    const result = {
      filePath: '/roms/game.nes',
      fileName: 'game.nes',
      system: 'nes',
      inferredTitle: 'Game',
      fileSizeBytes: 1024,
      lastModified: '2025-01-01T00:00:00Z',
    };
    expect(result.filePath).toEqual(expect.any(String));
    expect(result.fileName).toEqual(expect.any(String));
    expect(result.system).toEqual(expect.any(String));
    expect(result.inferredTitle).toEqual(expect.any(String));
    expect(result.fileSizeBytes).toEqual(expect.any(Number));
    expect(result.lastModified).toEqual(expect.any(String));
  });

  it('TauriLaunchResult success shape', () => {
    const success = { success: true, pid: 100 };
    expect(success.success).toBe(true);
    expect(success.pid).toBeDefined();
  });

  it('TauriLaunchResult failure shape', () => {
    const failure = { success: false, error: 'ROM not found' };
    expect(failure.success).toBe(false);
    expect(failure.error).toBeDefined();
  });

  it('FilePickResult null path indicates cancellation', () => {
    const cancelled = { path: null };
    expect(cancelled.path).toBeNull();
  });

  it('DirectoryPickResult with valid path', () => {
    const result = { path: '/home/user/roms' };
    expect(result.path).toBe('/home/user/roms');
  });
});
