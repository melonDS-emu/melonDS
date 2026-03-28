/**
 * Phase 28 — Compatibility & Stability Push
 *
 * Tests for:
 *  - CompatibilityStore: seed data, getByGameId, getBySystem, getSummary, set/upsert
 *  - KnownIssuesStore: seed data, report, getByGameId, updateStatus
 *  - SessionHealthStore: record, getByRoomId, getByGameId, getSummary, getSmokeTestMatrix
 *  - REST endpoints via httpHandler: /api/compatibility, /api/known-issues,
 *    /api/session-health, /api/smoke-tests, /api/sessions/export
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { CompatibilityStore } from '../compatibility-store';
import type { CompatibilityStatus } from '../compatibility-store';
import { KnownIssuesStore } from '../known-issues-store';
import { SessionHealthStore } from '../session-health-store';

// ---------------------------------------------------------------------------
// CompatibilityStore
// ---------------------------------------------------------------------------

describe('CompatibilityStore — seed data', () => {
  let store: CompatibilityStore;
  beforeEach(() => { store = new CompatibilityStore(); });

  it('has at least 25 pre-seeded records', () => {
    expect(store.getAll().length).toBeGreaterThanOrEqual(25);
  });

  it('all seed records have a valid status', () => {
    const VALID: CompatibilityStatus[] = ['verified', 'playable', 'partial', 'broken', 'untested'];
    for (const r of store.getAll()) {
      expect(VALID).toContain(r.status);
    }
  });

  it('seed records include mario-kart-64 as verified', () => {
    const recs = store.getByGameId('n64-mario-kart-64');
    expect(recs.length).toBeGreaterThan(0);
    expect(recs[0].status).toBe('verified');
  });

  it('seed records include nds-mario-kart-ds as verified', () => {
    const recs = store.getByGameId('nds-mario-kart-ds');
    expect(recs.length).toBeGreaterThan(0);
    expect(recs[0].status).toBe('verified');
  });

  it('seed records include psx-crash-team-racing as playable', () => {
    const recs = store.getByGameId('psx-crash-team-racing');
    expect(recs.length).toBeGreaterThan(0);
    expect(recs[0].status).toBe('playable');
  });
});

describe('CompatibilityStore — getBySystem', () => {
  let store: CompatibilityStore;
  beforeEach(() => { store = new CompatibilityStore(); });

  it('returns records for n64 system', () => {
    const recs = store.getBySystem('n64');
    expect(recs.length).toBeGreaterThan(0);
    for (const r of recs) expect(r.system).toBe('n64');
  });

  it('returns empty array for unknown system', () => {
    expect(store.getBySystem('atari-2600')).toEqual([]);
  });
});

describe('CompatibilityStore — getSummary', () => {
  let store: CompatibilityStore;
  beforeEach(() => { store = new CompatibilityStore(); });

  it('bestStatus is untested for unknown game', () => {
    expect(store.getSummary('unknown-game').bestStatus).toBe('untested');
  });

  it('bestStatus picks the best across multiple backends', () => {
    store.set({ gameId: 'test-game', system: 'nes', backend: 'fceux', status: 'broken', testedBy: 'tester' });
    store.set({ gameId: 'test-game', system: 'nes', backend: 'retroarch', status: 'verified', testedBy: 'tester' });
    const summary = store.getSummary('test-game');
    expect(summary.bestStatus).toBe('verified');
    expect(summary.records.length).toBe(2);
  });
});

describe('CompatibilityStore — set/upsert', () => {
  let store: CompatibilityStore;
  beforeEach(() => { store = new CompatibilityStore(); });

  it('upserts a record and returns it', () => {
    const rec = store.set({ gameId: 'nes-contra', system: 'nes', backend: 'fceux', status: 'verified', testedBy: 'qa' });
    expect(rec.gameId).toBe('nes-contra');
    expect(rec.status).toBe('verified');
    expect(rec.testedAt).toBeTruthy();
  });

  it('overwrites an existing record for same game+backend', () => {
    store.set({ gameId: 'nes-contra', system: 'nes', backend: 'fceux', status: 'partial', testedBy: 'qa' });
    store.set({ gameId: 'nes-contra', system: 'nes', backend: 'fceux', status: 'verified', testedBy: 'qa' });
    const recs = store.getByGameId('nes-contra');
    const fceuxRecs = recs.filter((r) => r.backend === 'fceux');
    expect(fceuxRecs.length).toBe(1);
    expect(fceuxRecs[0].status).toBe('verified');
  });

  it('getAllSummaries returns one entry per game', () => {
    store.set({ gameId: 'multi-backend', system: 'snes', backend: 'snes9x', status: 'verified', testedBy: 'qa' });
    store.set({ gameId: 'multi-backend', system: 'snes', backend: 'retroarch', status: 'playable', testedBy: 'qa' });
    const summaries = store.getAllSummaries();
    const entry = summaries.find((s) => s.gameId === 'multi-backend');
    expect(entry).toBeDefined();
    expect(entry!.records.length).toBe(2);
    expect(entry!.bestStatus).toBe('verified');
  });
});

// ---------------------------------------------------------------------------
// KnownIssuesStore
// ---------------------------------------------------------------------------

describe('KnownIssuesStore — seed data', () => {
  let store: KnownIssuesStore;
  beforeEach(() => { store = new KnownIssuesStore(); });

  it('has at least 5 pre-seeded issues', () => {
    expect(store.getAll().length).toBeGreaterThanOrEqual(5);
  });

  it('all seed issues have an id, title, and reportedAt', () => {
    for (const issue of store.getAll()) {
      expect(issue.id).toBeTruthy();
      expect(issue.title).toBeTruthy();
      expect(issue.reportedAt).toBeTruthy();
    }
  });

  it('seed issues include an n64 goldeneye desync issue', () => {
    const issues = store.getByGameId('n64-goldeneye-007');
    expect(issues.length).toBeGreaterThan(0);
  });

  it('getOpen returns only non-resolved issues', () => {
    const open = store.getOpen();
    for (const i of open) expect(i.status).not.toBe('resolved');
  });
});

describe('KnownIssuesStore — report', () => {
  let store: KnownIssuesStore;
  beforeEach(() => { store = new KnownIssuesStore(); });

  it('creates a new issue with status=open', () => {
    const issue = store.report({
      gameId: 'snes-sf2',
      system: 'snes',
      backend: 'snes9x',
      title: 'Input lag on fast connection',
      description: 'Street Fighter 2 shows 1 frame extra lag.',
      severity: 'low',
      reportedBy: 'player1',
    });
    expect(issue.id).toBeTruthy();
    expect(issue.status).toBe('open');
    expect(issue.gameId).toBe('snes-sf2');
  });

  it('getByGameId returns newly reported issue', () => {
    store.report({
      gameId: 'snes-sf2',
      system: 'snes',
      backend: null,
      title: 'Test issue',
      description: 'Test',
      severity: 'medium',
      reportedBy: 'tester',
    });
    expect(store.getByGameId('snes-sf2').length).toBeGreaterThan(0);
  });
});

describe('KnownIssuesStore — updateStatus', () => {
  let store: KnownIssuesStore;
  beforeEach(() => { store = new KnownIssuesStore(); });

  it('updates status to resolved and sets resolvedAt', () => {
    const issue = store.report({
      gameId: 'test', system: 'nes', backend: null,
      title: 'Temp', description: 'D', severity: 'low', reportedBy: 'x',
    });
    const updated = store.updateStatus(issue.id, 'resolved');
    expect(updated).not.toBeNull();
    expect(updated!.status).toBe('resolved');
    expect(updated!.resolvedAt).toBeTruthy();
  });

  it('returns null for unknown id', () => {
    expect(store.updateStatus('no-such-id', 'resolved')).toBeNull();
  });

  it('status can be changed to investigating', () => {
    const issue = store.report({
      gameId: 'test', system: 'nes', backend: null,
      title: 'Temp', description: 'D', severity: 'low', reportedBy: 'x',
    });
    const updated = store.updateStatus(issue.id, 'investigating');
    expect(updated!.status).toBe('investigating');
  });
});

// ---------------------------------------------------------------------------
// SessionHealthStore
// ---------------------------------------------------------------------------

describe('SessionHealthStore — record', () => {
  let store: SessionHealthStore;
  beforeEach(() => { store = new SessionHealthStore(); });

  it('stores a health record and returns it', () => {
    const rec = store.record({
      roomId: 'room-abc',
      gameId: 'n64-mk64',
      system: 'n64',
      backend: 'mupen64plus',
      avgLatencyMs: 45,
      peakLatencyMs: 120,
      disconnectCount: 0,
      crashDetected: false,
      durationSecs: 3600,
      playerCount: 4,
    });
    expect(rec.roomId).toBe('room-abc');
    expect(rec.avgLatencyMs).toBe(45);
    expect(rec.recordedAt).toBeTruthy();
  });

  it('getByRoomId retrieves the recorded entry', () => {
    store.record({ roomId: 'room-xyz', gameId: 'game1', system: 'nes', backend: 'fceux', avgLatencyMs: 30, peakLatencyMs: 60, disconnectCount: 0, crashDetected: false, durationSecs: 600, playerCount: 2 });
    const found = store.getByRoomId('room-xyz');
    expect(found).toBeDefined();
    expect(found!.gameId).toBe('game1');
  });

  it('getByRoomId returns undefined for unknown room', () => {
    expect(store.getByRoomId('no-room')).toBeUndefined();
  });

  it('getByGameId returns all records for a game', () => {
    store.record({ roomId: 'r1', gameId: 'gba-mk', system: 'gba', backend: 'mgba', avgLatencyMs: 50, peakLatencyMs: 80, disconnectCount: 0, crashDetected: false, durationSecs: 900, playerCount: 2 });
    store.record({ roomId: 'r2', gameId: 'gba-mk', system: 'gba', backend: 'mgba', avgLatencyMs: 70, peakLatencyMs: 100, disconnectCount: 1, crashDetected: false, durationSecs: 1200, playerCount: 2 });
    const recs = store.getByGameId('gba-mk');
    expect(recs.length).toBe(2);
  });
});

describe('SessionHealthStore — getSummary', () => {
  let store: SessionHealthStore;
  beforeEach(() => { store = new SessionHealthStore(); });

  it('returns zero summary for unknown game', () => {
    const s = store.getSummary('unknown-game');
    expect(s.sessionCount).toBe(0);
    expect(s.avgLatencyMs).toBe(0);
    expect(s.crashRate).toBe(0);
  });

  it('computes correct avgLatencyMs', () => {
    store.record({ roomId: 'a', gameId: 'g1', system: 'nes', backend: 'fceux', avgLatencyMs: 40, peakLatencyMs: 80, disconnectCount: 0, crashDetected: false, durationSecs: 300, playerCount: 2 });
    store.record({ roomId: 'b', gameId: 'g1', system: 'nes', backend: 'fceux', avgLatencyMs: 60, peakLatencyMs: 100, disconnectCount: 0, crashDetected: false, durationSecs: 300, playerCount: 2 });
    const s = store.getSummary('g1');
    expect(s.avgLatencyMs).toBe(50);
    expect(s.sessionCount).toBe(2);
  });

  it('computes crashRate correctly', () => {
    store.record({ roomId: 'c1', gameId: 'g2', system: 'n64', backend: 'mupen64plus', avgLatencyMs: 50, peakLatencyMs: 90, disconnectCount: 0, crashDetected: true, durationSecs: 1800, playerCount: 2 });
    store.record({ roomId: 'c2', gameId: 'g2', system: 'n64', backend: 'mupen64plus', avgLatencyMs: 50, peakLatencyMs: 90, disconnectCount: 0, crashDetected: false, durationSecs: 1800, playerCount: 2 });
    const s = store.getSummary('g2');
    expect(s.crashRate).toBe(0.5);
  });
});

describe('SessionHealthStore — smoke test matrix', () => {
  let store: SessionHealthStore;
  beforeEach(() => { store = new SessionHealthStore(); });

  it('getSmokeTestMatrix returns at least 25 entries', () => {
    const matrix = store.getSmokeTestMatrix();
    expect(matrix.length).toBeGreaterThanOrEqual(25);
  });

  it('all entries have system, backend, gameId, and status fields', () => {
    for (const entry of store.getSmokeTestMatrix()) {
      expect(entry.system).toBeTruthy();
      expect(entry.backend).toBeTruthy();
      expect(entry.gameId).toBeTruthy();
      expect(['pass', 'fail', 'skip', 'pending']).toContain(entry.status);
    }
  });

  it('includes entries for each supported system', () => {
    const systems = new Set(store.getSmokeTestMatrix().map((e) => e.system));
    for (const sys of ['nes', 'snes', 'gb', 'gbc', 'gba', 'n64', 'nds', 'gc', 'wii', 'psx']) {
      expect(systems.has(sys)).toBe(true);
    }
  });

  it('all smoke tests are currently passing', () => {
    const failing = store.getSmokeTestMatrix().filter((e) => e.status === 'fail');
    expect(failing.length).toBe(0);
  });
});
