/**
 * Unit tests for CloudSyncService and computeFileHash.
 */

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import { CloudSyncService, computeFileHash, type CloudStorageAdapter } from '@retro-oasis/save-system';

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

let tmpDir: string;

beforeEach(() => {
  tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'retro-oasis-test-'));
});

afterEach(() => {
  fs.rmSync(tmpDir, { recursive: true, force: true });
});

function writeTmp(name: string, content: string | Buffer): string {
  const p = path.join(tmpDir, name);
  fs.writeFileSync(p, content);
  return p;
}

// ---------------------------------------------------------------------------
// computeFileHash
// ---------------------------------------------------------------------------

describe('computeFileHash', () => {
  it('returns a 64-char hex string for an existing file', () => {
    const p = writeTmp('save.sav', 'hello');
    const hash = computeFileHash(p);
    expect(hash).toMatch(/^[0-9a-f]{64}$/);
  });

  it('returns an empty string for a missing file', () => {
    const hash = computeFileHash(path.join(tmpDir, 'nonexistent.sav'));
    expect(hash).toBe('');
  });

  it('produces the same hash for identical content', () => {
    const p1 = writeTmp('a.sav', 'identical');
    const p2 = writeTmp('b.sav', 'identical');
    expect(computeFileHash(p1)).toBe(computeFileHash(p2));
  });

  it('produces different hashes for different content', () => {
    const p1 = writeTmp('a.sav', 'content-A');
    const p2 = writeTmp('b.sav', 'content-B');
    expect(computeFileHash(p1)).not.toBe(computeFileHash(p2));
  });
});

// ---------------------------------------------------------------------------
// CloudSyncService.trackSave
// ---------------------------------------------------------------------------

describe('CloudSyncService — trackSave', () => {
  it('tracks a save and computes the file hash', () => {
    const p = writeTmp('slot0.sav', 'save data');
    const svc = new CloudSyncService();
    const rec = svc.trackSave('mk64', 'user1', p);

    expect(rec.gameId).toBe('mk64');
    expect(rec.userId).toBe('user1');
    expect(rec.fileHash).toMatch(/^[0-9a-f]{64}$/);
    expect(rec.fileSize).toBe(9); // 'save data' = 9 bytes
    expect(rec.syncStatus).toBe('synced');
  });

  it('stores an empty hash when the file does not exist', () => {
    const svc = new CloudSyncService();
    const rec = svc.trackSave('mk64', 'user1', path.join(tmpDir, 'missing.sav'));
    expect(rec.fileHash).toBe('');
    expect(rec.fileSize).toBe(0);
  });

  it('returns null for an untracked save', () => {
    const svc = new CloudSyncService();
    expect(svc.getSyncStatus('unknown-game', 'user1')).toBeNull();
  });
});

// ---------------------------------------------------------------------------
// CloudSyncService.markDirty
// ---------------------------------------------------------------------------

describe('CloudSyncService — markDirty', () => {
  it('returns false for an untracked save', () => {
    const svc = new CloudSyncService();
    expect(svc.markDirty('nofile', 'user1')).toBe(false);
  });

  it('marks a changed file as pending-upload', () => {
    const p = writeTmp('slot0.sav', 'original');
    const svc = new CloudSyncService();
    svc.trackSave('mk64', 'user1', p);

    // Modify the file on disk
    fs.writeFileSync(p, 'modified content');

    const changed = svc.markDirty('mk64', 'user1');
    expect(changed).toBe(true);

    const rec = svc.getSyncStatus('mk64', 'user1')!;
    expect(rec.syncStatus).toBe('pending-upload');
  });

  it('returns false when the file is unchanged', () => {
    const p = writeTmp('slot0.sav', 'same content');
    const svc = new CloudSyncService();
    svc.trackSave('mk64', 'user1', p);

    // File not touched — markDirty should detect no change
    const changed = svc.markDirty('mk64', 'user1');
    expect(changed).toBe(false);
    expect(svc.getSyncStatus('mk64', 'user1')!.syncStatus).toBe('synced');
  });
});

// ---------------------------------------------------------------------------
// CloudSyncService.checkForConflict
// ---------------------------------------------------------------------------

describe('CloudSyncService — checkForConflict', () => {
  it('marks conflict when local is dirty and remote has a different newer hash', () => {
    const p = writeTmp('slot0.sav', 'local v1');
    const svc = new CloudSyncService();
    svc.trackSave('mk64', 'user1', p);

    // Simulate local modification
    fs.writeFileSync(p, 'local v2');
    svc.markDirty('mk64', 'user1');

    const future = new Date(Date.now() + 10_000).toISOString();
    const isConflict = svc.checkForConflict('mk64', 'user1', 'remote-hash-abc', future);

    expect(isConflict).toBe(true);
    expect(svc.getSyncStatus('mk64', 'user1')!.syncStatus).toBe('conflict');
  });

  it('schedules a download when remote is newer and local is clean', () => {
    const p = writeTmp('slot0.sav', 'local v1');
    const svc = new CloudSyncService();
    svc.trackSave('mk64', 'user1', p);

    const future = new Date(Date.now() + 10_000).toISOString();
    svc.checkForConflict('mk64', 'user1', 'remote-hash-different', future);

    expect(svc.getSyncStatus('mk64', 'user1')!.syncStatus).toBe('pending-download');
  });

  it('returns false for untracked save', () => {
    const svc = new CloudSyncService();
    expect(svc.checkForConflict('x', 'y', 'hash', new Date().toISOString())).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// CloudSyncService.upload / download (no adapter)
// ---------------------------------------------------------------------------

describe('CloudSyncService — upload/download without adapter', () => {
  it('upload returns true and marks synced', async () => {
    const p = writeTmp('slot0.sav', 'data');
    const svc = new CloudSyncService();
    svc.trackSave('mk64', 'user1', p);
    fs.writeFileSync(p, 'updated data');
    svc.markDirty('mk64', 'user1');

    const ok = await svc.upload('mk64', 'user1');
    expect(ok).toBe(true);
    expect(svc.getSyncStatus('mk64', 'user1')!.syncStatus).toBe('synced');
  });

  it('download returns true and marks synced', async () => {
    const p = writeTmp('slot0.sav', 'data');
    const svc = new CloudSyncService();
    svc.trackSave('mk64', 'user1', p);
    const rec = svc.getSyncStatus('mk64', 'user1')!;
    rec.syncStatus = 'pending-download';

    const ok = await svc.download('mk64', 'user1');
    expect(ok).toBe(true);
    expect(svc.getSyncStatus('mk64', 'user1')!.syncStatus).toBe('synced');
  });

  it('upload returns false for untracked save', async () => {
    const svc = new CloudSyncService();
    expect(await svc.upload('nope', 'user1')).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// CloudSyncService.upload / download (with mock adapter)
// ---------------------------------------------------------------------------

describe('CloudSyncService — with CloudStorageAdapter', () => {
  it('calls adapter.upload and stores the returned hash', async () => {
    const p = writeTmp('slot0.sav', 'game save bytes');

    const mockAdapter: CloudStorageAdapter = {
      upload: async (_key, _data) => 'mock-remote-hash',
      download: async (_key) => Buffer.from('downloaded bytes'),
    };

    const svc = new CloudSyncService(mockAdapter);
    svc.trackSave('smb3', 'user2', p);
    fs.writeFileSync(p, 'game save bytes v2');
    svc.markDirty('smb3', 'user2');

    const ok = await svc.upload('smb3', 'user2');
    expect(ok).toBe(true);
    // The adapter returned 'mock-remote-hash' — that should be stored
    expect(svc.getSyncStatus('smb3', 'user2')!.fileHash).toBe('mock-remote-hash');
  });

  it('calls adapter.download and writes data to localPath', async () => {
    const p = writeTmp('slot0.sav', 'old data');

    const mockAdapter: CloudStorageAdapter = {
      upload: async () => '',
      download: async (_key) => Buffer.from('fresh cloud data'),
    };

    const svc = new CloudSyncService(mockAdapter);
    svc.trackSave('zelda', 'user2', p);
    const rec = svc.getSyncStatus('zelda', 'user2')!;
    rec.syncStatus = 'pending-download';

    const ok = await svc.download('zelda', 'user2');
    expect(ok).toBe(true);
    expect(fs.readFileSync(p).toString()).toBe('fresh cloud data');
  });

  it('returns false and sets error status when adapter.upload throws', async () => {
    const p = writeTmp('slot0.sav', 'oops');

    const badAdapter: CloudStorageAdapter = {
      upload: async () => { throw new Error('Network error'); },
      download: async () => Buffer.alloc(0),
    };

    const svc = new CloudSyncService(badAdapter);
    svc.trackSave('smb3', 'user2', p);
    fs.writeFileSync(p, 'oops v2');
    svc.markDirty('smb3', 'user2');

    const ok = await svc.upload('smb3', 'user2');
    expect(ok).toBe(false);
    expect(svc.getSyncStatus('smb3', 'user2')!.syncStatus).toBe('error');
  });
});

// ---------------------------------------------------------------------------
// CloudSyncService.resolveConflict
// ---------------------------------------------------------------------------

describe('CloudSyncService — resolveConflict', () => {
  it('returns false when there is no conflict', async () => {
    const p = writeTmp('slot0.sav', 'data');
    const svc = new CloudSyncService();
    svc.trackSave('mk64', 'user1', p);
    expect(await svc.resolveConflict('mk64', 'user1', 'keep-local')).toBe(false);
  });

  it('resolves keep-local by uploading', async () => {
    const p = writeTmp('slot0.sav', 'local wins');
    const svc = new CloudSyncService();
    svc.trackSave('mk64', 'user1', p);
    const rec = svc.getSyncStatus('mk64', 'user1')!;
    rec.syncStatus = 'conflict';

    const ok = await svc.resolveConflict('mk64', 'user1', 'keep-local');
    expect(ok).toBe(true);
    expect(svc.getSyncStatus('mk64', 'user1')!.syncStatus).toBe('synced');
  });

  it('resolves keep-cloud by downloading', async () => {
    const p = writeTmp('slot0.sav', 'cloud wins');
    const svc = new CloudSyncService();
    svc.trackSave('mk64', 'user1', p);
    const rec = svc.getSyncStatus('mk64', 'user1')!;
    rec.syncStatus = 'conflict';

    const ok = await svc.resolveConflict('mk64', 'user1', 'keep-cloud');
    expect(ok).toBe(true);
    expect(svc.getSyncStatus('mk64', 'user1')!.syncStatus).toBe('synced');
  });
});

// ---------------------------------------------------------------------------
// CloudSyncService.listTrackedSaves
// ---------------------------------------------------------------------------

describe('CloudSyncService — listTrackedSaves', () => {
  it('returns only saves for the requested user', () => {
    const svc = new CloudSyncService();
    const p1 = writeTmp('a.sav', '');
    const p2 = writeTmp('b.sav', '');
    svc.trackSave('game1', 'alice', p1);
    svc.trackSave('game2', 'alice', p2);
    svc.trackSave('game1', 'bob', p1);

    const aliceSaves = svc.listTrackedSaves('alice');
    expect(aliceSaves).toHaveLength(2);
    expect(aliceSaves.every((s) => s.userId === 'alice')).toBe(true);
  });

  it('returns empty array for unknown user', () => {
    const svc = new CloudSyncService();
    expect(svc.listTrackedSaves('nobody')).toHaveLength(0);
  });
});
