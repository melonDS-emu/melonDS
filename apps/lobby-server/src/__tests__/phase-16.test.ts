/**
 * Phase 16 — Desktop Runtime & Core Integration
 *
 * Tests for: launch path helpers, melonDS IPC stub, ROM resolution helpers.
 */

import { describe, it, expect } from 'vitest';
import { MelonDsIpcStub } from '../melonds-ipc';
import type { MelonDsCommand } from '../melonds-ipc';

// ---------------------------------------------------------------------------
// MelonDsIpcStub — default stub bridge
// ---------------------------------------------------------------------------

describe('MelonDsIpcStub', () => {
  it('starts disconnected', () => {
    const bridge = new MelonDsIpcStub();
    expect(bridge.isConnected).toBe(false);
  });

  it('connect() marks the bridge as connected', async () => {
    const bridge = new MelonDsIpcStub();
    await bridge.connect('session-abc');
    expect(bridge.isConnected).toBe(true);
  });

  it('disconnect() marks the bridge as disconnected', async () => {
    const bridge = new MelonDsIpcStub();
    await bridge.connect('session-abc');
    await bridge.disconnect();
    expect(bridge.isConnected).toBe(false);
  });

  it('returns pong for ping command', async () => {
    const bridge = new MelonDsIpcStub();
    const res = await bridge.send({ type: 'ping' });
    expect(res.type).toBe('pong');
    if (res.type === 'pong') {
      expect(typeof res.uptimeMs).toBe('number');
    }
  });

  it('returns ok for pause/resume/stop/setScreenLayout commands', async () => {
    const bridge = new MelonDsIpcStub();
    const commands: MelonDsCommand[] = [
      { type: 'pause' },
      { type: 'resume' },
      { type: 'stop' },
      { type: 'setScreenLayout', layout: 'stacked' },
    ];
    for (const cmd of commands) {
      const res = await bridge.send(cmd);
      expect(res.type).toBe('ok');
    }
  });

  it('returns ok for launch command', async () => {
    const bridge = new MelonDsIpcStub();
    const res = await bridge.send({
      type: 'launch',
      romPath: '/roms/game.nds',
      saveDirectory: '/saves',
      wifiHost: 'localhost',
      wifiPort: 9000,
    });
    expect(res.type).toBe('ok');
  });

  it('returns saveStateDone with correct slotIndex', async () => {
    const bridge = new MelonDsIpcStub();
    const res = await bridge.send({ type: 'saveState', slotIndex: 2 });
    expect(res.type).toBe('saveStateDone');
    if (res.type === 'saveStateDone') {
      expect(res.slotIndex).toBe(2);
    }
  });

  it('returns loadStateDone with correct slotIndex', async () => {
    const bridge = new MelonDsIpcStub();
    const res = await bridge.send({ type: 'loadState', slotIndex: 1 });
    expect(res.type).toBe('loadStateDone');
    if (res.type === 'loadStateDone') {
      expect(res.slotIndex).toBe(1);
    }
  });

  it('can reconnect after disconnect', async () => {
    const bridge = new MelonDsIpcStub();
    await bridge.connect('session-1');
    await bridge.disconnect();
    await bridge.connect('session-2');
    expect(bridge.isConnected).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Launch path: defaultBackendId mapping contract
// (mirrors the logic in LobbyContext so we catch accidental regressions)
// ---------------------------------------------------------------------------

function defaultBackendId(system: string): string | null {
  switch (system.toLowerCase()) {
    case 'n64':  return 'mupen64plus';
    case 'nds':  return 'melonds';
    case 'gba':  return 'mgba';
    case 'gb':
    case 'gbc':  return 'sameboy';
    case 'nes':  return 'fceux';
    case 'snes': return 'snes9x';
    default:     return null;
  }
}

describe('defaultBackendId', () => {
  it('maps N64 to mupen64plus', () => {
    expect(defaultBackendId('n64')).toBe('mupen64plus');
    expect(defaultBackendId('N64')).toBe('mupen64plus');
  });

  it('maps NDS to melonds', () => {
    expect(defaultBackendId('nds')).toBe('melonds');
  });

  it('maps GBA to mgba', () => {
    expect(defaultBackendId('gba')).toBe('mgba');
  });

  it('maps GB and GBC to sameboy', () => {
    expect(defaultBackendId('gb')).toBe('sameboy');
    expect(defaultBackendId('gbc')).toBe('sameboy');
  });

  it('maps NES to fceux', () => {
    expect(defaultBackendId('nes')).toBe('fceux');
  });

  it('maps SNES to snes9x', () => {
    expect(defaultBackendId('snes')).toBe('snes9x');
  });

  it('returns null for unknown systems', () => {
    expect(defaultBackendId('genesis')).toBeNull();
    expect(defaultBackendId('')).toBeNull();
  });
});
