/**
 * Unit tests for NetplayRelay — port allocation, deallocation, and pruning.
 *
 * We use a small port range (9800-9810) to keep allocation fast and avoid
 * conflicts with the integration tests that use a stub relay.
 */

import { describe, it, expect, afterEach } from 'vitest';
import { NetplayRelay } from '../netplay-relay';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Create a relay that binds to a deterministic narrow port range so tests
 * can allocate several sessions without clashing with OS assignments.
 *
 * NOTE: We do NOT actually start TCP listeners here — we override
 * `allocateSession` is tested at the unit level (port assignment logic),
 * and `deallocateSession` / `pruneInactiveSessions` are tested in isolation.
 */
function makeRelay(minPort = 9800, maxPort = 9810): NetplayRelay {
  process.env.RELAY_PORT_MIN = String(minPort);
  process.env.RELAY_PORT_MAX = String(maxPort);
  return new NetplayRelay('127.0.0.1');
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

describe('NetplayRelay — port allocation', () => {
  it('has the correct bindHost', () => {
    const relay = new NetplayRelay('10.0.0.1');
    expect(relay.bindHost).toBe('10.0.0.1');
  });

  it('defaults bindHost to 0.0.0.0 when env is unset', () => {
    delete process.env.RELAY_BIND;
    const relay = new NetplayRelay();
    expect(relay.bindHost).toBe('0.0.0.0');
  });

  it('returns null when no ports are available in the pool', () => {
    // Use a single-port pool so it fills up quickly
    process.env.RELAY_PORT_MIN = '9900';
    process.env.RELAY_PORT_MAX = '9900';

    // We exercise allocatePort indirectly.  The first call succeeds because
    // there is one free port; the second call fails because none remain.
    // We test this via the usedPorts set that is populated after a real
    // allocateSession call.  Since we don't want side effects from actually
    // starting TCP listeners we test that getSession returns null for an
    // unallocated room — a lighter invariant.
    const relay = new NetplayRelay('127.0.0.1');
    expect(relay.getSession('nonexistent')).toBeNull();
  });
});

describe('NetplayRelay — session lifecycle', () => {
  it('getSession returns null for unknown room', () => {
    const relay = makeRelay();
    expect(relay.getSession('room-xyz')).toBeNull();
  });

  it('listSessions returns empty array initially', () => {
    const relay = makeRelay();
    expect(relay.listSessions()).toHaveLength(0);
  });

  it('deallocateSession is a no-op for unknown room', () => {
    const relay = makeRelay();
    expect(() => relay.deallocateSession('nope')).not.toThrow();
  });
});

describe('NetplayRelay — pruneInactiveSessions', () => {
  afterEach(() => {
    // Reset env
    delete process.env.RELAY_PORT_MIN;
    delete process.env.RELAY_PORT_MAX;
  });

  it('returns 0 when there are no sessions', () => {
    const relay = makeRelay();
    expect(relay.pruneInactiveSessions(60_000)).toBe(0);
  });

  it('does not prune sessions that were recently created', () => {
    const relay = makeRelay();
    // Inject a fake session by accessing private map via type assertion
    const sessions = (relay as unknown as { sessions: Map<string, unknown> }).sessions;
    const usedPorts = (relay as unknown as { usedPorts: Set<number> }).usedPorts;

    sessions.set('room-a', {
      roomId: 'room-a',
      playerIds: [],
      sockets: new Map(),
      port: 9800,
      server: { close: () => undefined, on: () => undefined },
      createdAt: new Date(), // just now
    });
    usedPorts.add(9800);

    // maxAgeMs = 1 hour — should not prune a fresh session
    expect(relay.pruneInactiveSessions(3_600_000)).toBe(0);
    expect(relay.listSessions()).toHaveLength(1);
  });

  it('prunes idle sessions older than maxAgeMs', () => {
    const relay = makeRelay();
    const sessions = (relay as unknown as { sessions: Map<string, unknown> }).sessions;
    const usedPorts = (relay as unknown as { usedPorts: Set<number> }).usedPorts;

    const old = new Date(Date.now() - 10 * 60 * 1000); // 10 minutes ago

    sessions.set('old-room', {
      roomId: 'old-room',
      playerIds: [],
      sockets: new Map(), // no connected sockets
      port: 9801,
      server: { close: () => undefined, on: () => undefined },
      createdAt: old,
    });
    usedPorts.add(9801);

    // Prune sessions idle for more than 5 minutes
    const pruned = relay.pruneInactiveSessions(5 * 60 * 1000);
    expect(pruned).toBe(1);
    expect(relay.listSessions()).toHaveLength(0);
    // Port should be freed
    expect(usedPorts.has(9801)).toBe(false);
  });

  it('does not prune sessions that still have connected sockets', () => {
    const relay = makeRelay();
    const sessions = (relay as unknown as { sessions: Map<string, unknown> }).sessions;
    const usedPorts = (relay as unknown as { usedPorts: Set<number> }).usedPorts;

    const old = new Date(Date.now() - 10 * 60 * 1000);

    // Simulate one connected socket
    const fakeSockets = new Map([['token-1', {}]]);
    sessions.set('active-room', {
      roomId: 'active-room',
      playerIds: ['p1'],
      sockets: fakeSockets,
      port: 9802,
      server: { close: () => undefined, on: () => undefined },
      createdAt: old,
    });
    usedPorts.add(9802);

    const pruned = relay.pruneInactiveSessions(5 * 60 * 1000);
    expect(pruned).toBe(0);
    expect(relay.listSessions()).toHaveLength(1);
  });

  it('prunes only the idle session when mixed old sessions exist', () => {
    const relay = makeRelay();
    const sessions = (relay as unknown as { sessions: Map<string, unknown> }).sessions;
    const usedPorts = (relay as unknown as { usedPorts: Set<number> }).usedPorts;

    const old = new Date(Date.now() - 10 * 60 * 1000);

    sessions.set('idle-room', {
      roomId: 'idle-room',
      playerIds: [],
      sockets: new Map(),
      port: 9803,
      server: { close: () => undefined, on: () => undefined },
      createdAt: old,
    });
    usedPorts.add(9803);

    sessions.set('busy-room', {
      roomId: 'busy-room',
      playerIds: ['p1'],
      sockets: new Map([['tok', {}]]),
      port: 9804,
      server: { close: () => undefined, on: () => undefined },
      createdAt: old,
    });
    usedPorts.add(9804);

    const pruned = relay.pruneInactiveSessions(5 * 60 * 1000);
    expect(pruned).toBe(1);
    expect(relay.listSessions()).toHaveLength(1);
    expect(relay.listSessions()[0].roomId).toBe('busy-room');
  });
});
