/**
 * Phase 4 — Save System
 *
 * Tests for:
 *  - SaveBackupStore: create, get, list, delete backups
 *  - SaveBackupStore: per-slot eviction limit
 *  - SaveBackupStore: markAsLastKnownGood + getLastKnownGood
 *  - SaveBackupStore: preSessionBackup creates pre-session records
 *  - SaveBackupStore: postSessionSync creates post-session records + marks LKG on clean exit
 *  - SqliteSaveBackupStore: identical interface backed by SQLite (via in-memory DB)
 *  - save_backups table exists in the schema (db.ts)
 *  - REST: POST /api/saves/:gameId/backup
 *  - REST: GET  /api/saves/:gameId/backups
 *  - REST: DELETE /api/saves/:gameId/backups/:backupId
 *  - REST: POST /api/saves/:gameId/backups/:backupId/restore
 *  - REST: POST /api/saves/:gameId/backups/:backupId/mark-lkg
 *  - REST: GET  /api/saves/:gameId/last-known-good?name=<slot>
 *  - REST: POST /api/saves/:gameId/session-start (pre-session backup)
 *  - REST: POST /api/saves/:gameId/session-end   (post-session sync)
 *  - discoverSaveFiles: returns correct paths sorted by backend preference
 *  - getBackendSaveExtensions: known backends return expected extensions
 *  - buildBackendSavePath: constructs correct paths
 *  - inferBackendFromExtension: maps extensions to backends
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { SaveBackupStore } from '../save-backup-store';
import { SqliteSaveBackupStore } from '../sqlite-save-backup-store';
import { openDatabase } from '../db';
import { SaveStore } from '../save-store';
import {
  getBackendSaveExtensions,
  buildBackendSavePath,
  inferBackendFromExtension,
  discoverSaveFiles,
} from '@retro-oasis/save-system';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const GAME_ID = 'n64-mario-kart-64';
const SAVE_NAME = 'Slot 1';
const DATA = Buffer.from('fake-save-bytes').toString('base64');
const DATA2 = Buffer.from('updated-bytes').toString('base64');

// ---------------------------------------------------------------------------
// SaveBackupStore — in-memory
// ---------------------------------------------------------------------------

describe('SaveBackupStore — in-memory', () => {
  let store: SaveBackupStore;

  beforeEach(() => {
    store = new SaveBackupStore();
  });

  it('creates a backup and returns it', () => {
    const b = store.createBackup(GAME_ID, SAVE_NAME, DATA);
    expect(b.id).toBeTruthy();
    expect(b.gameId).toBe(GAME_ID);
    expect(b.saveName).toBe(SAVE_NAME);
    expect(b.data).toBe(DATA);
    expect(b.reason).toBe('manual');
    expect(b.isLastKnownGood).toBe(false);
  });

  it('auto-marks as last-known-good when reason is last-known-good', () => {
    const b = store.createBackup(GAME_ID, SAVE_NAME, DATA, { reason: 'last-known-good' });
    expect(b.isLastKnownGood).toBe(true);
  });

  it('get() retrieves a backup by ID', () => {
    const created = store.createBackup(GAME_ID, SAVE_NAME, DATA);
    const fetched = store.get(created.id);
    expect(fetched).not.toBeNull();
    expect(fetched!.data).toBe(DATA);
  });

  it('get() returns null for unknown ID', () => {
    expect(store.get('ghost-id')).toBeNull();
  });

  it('listForGame() returns all backups for a game in descending order', () => {
    store.createBackup(GAME_ID, 'Slot 1', DATA, { reason: 'pre-session' });
    store.createBackup(GAME_ID, 'Slot 2', DATA, { reason: 'manual' });
    store.createBackup('other-game', 'Slot 1', DATA);

    const list = store.listForGame(GAME_ID);
    expect(list.length).toBe(2);
    expect(list.every((b) => b.gameId === GAME_ID)).toBe(true);
    // data field should not be included in listForGame
    expect('data' in list[0]).toBe(false);
  });

  it('listForSlot() returns only backups for a specific slot', () => {
    store.createBackup(GAME_ID, 'Slot 1', DATA);
    store.createBackup(GAME_ID, 'Slot 2', DATA);

    const list = store.listForSlot(GAME_ID, 'Slot 1');
    expect(list.length).toBe(1);
    expect(list[0].saveName).toBe('Slot 1');
  });

  it('delete() removes a backup', () => {
    const b = store.createBackup(GAME_ID, SAVE_NAME, DATA);
    expect(store.delete(b.id)).toBe(true);
    expect(store.get(b.id)).toBeNull();
  });

  it('delete() returns false for unknown ID', () => {
    expect(store.delete('ghost')).toBe(false);
  });

  it('enforces MAX_BACKUPS_PER_SLOT by evicting oldest non-LKG backup', () => {
    // Fill to limit (20)
    for (let i = 0; i < 20; i++) {
      store.createBackup(GAME_ID, SAVE_NAME, DATA);
    }
    const before = store.listForSlot(GAME_ID, SAVE_NAME);
    expect(before.length).toBe(20);

    // Adding one more should evict the oldest; total stays at 20
    store.createBackup(GAME_ID, SAVE_NAME, DATA2);
    const after = store.listForSlot(GAME_ID, SAVE_NAME);
    expect(after.length).toBe(20);
    // The new backup must exist somewhere in the slot list
    const hasData2 = after.some((b) => store.get(b.id)?.data === DATA2);
    expect(hasData2).toBe(true);
  });

  it('eviction preserves last-known-good backups', () => {
    // Add 19 normal backups
    for (let i = 0; i < 19; i++) {
      store.createBackup(GAME_ID, SAVE_NAME, DATA);
    }
    // Add 1 LKG backup
    const lkg = store.createBackup(GAME_ID, SAVE_NAME, DATA, { reason: 'last-known-good' });

    // Slot is full (20). Adding one more should evict oldest non-LKG
    store.createBackup(GAME_ID, SAVE_NAME, DATA2);
    const list = store.listForSlot(GAME_ID, SAVE_NAME);
    expect(list.length).toBe(20);

    // LKG should still be present
    const lkgStillPresent = list.find((b) => b.id === lkg.id);
    expect(lkgStillPresent).toBeDefined();
  });
});

// ---------------------------------------------------------------------------
// markAsLastKnownGood + getLastKnownGood
// ---------------------------------------------------------------------------

describe('SaveBackupStore — last-known-good', () => {
  let store: SaveBackupStore;

  beforeEach(() => {
    store = new SaveBackupStore();
  });

  it('markAsLastKnownGood() marks a backup', () => {
    const b = store.createBackup(GAME_ID, SAVE_NAME, DATA);
    expect(store.markAsLastKnownGood(b.id)).toBe(true);
    expect(store.get(b.id)!.isLastKnownGood).toBe(true);
  });

  it('markAsLastKnownGood() clears previous LKG flag', () => {
    const b1 = store.createBackup(GAME_ID, SAVE_NAME, DATA, { reason: 'last-known-good' });
    const b2 = store.createBackup(GAME_ID, SAVE_NAME, DATA2);
    store.markAsLastKnownGood(b2.id);

    expect(store.get(b1.id)!.isLastKnownGood).toBe(false);
    expect(store.get(b2.id)!.isLastKnownGood).toBe(true);
  });

  it('getLastKnownGood() returns the most recent LKG backup', () => {
    store.createBackup(GAME_ID, SAVE_NAME, DATA);
    const lkg = store.createBackup(GAME_ID, SAVE_NAME, DATA, { reason: 'last-known-good' });

    const found = store.getLastKnownGood(GAME_ID, SAVE_NAME);
    expect(found).not.toBeNull();
    expect(found!.id).toBe(lkg.id);
  });

  it('getLastKnownGood() returns null when none is marked', () => {
    store.createBackup(GAME_ID, SAVE_NAME, DATA);
    expect(store.getLastKnownGood(GAME_ID, SAVE_NAME)).toBeNull();
  });

  it('markAsLastKnownGood() returns false for unknown backup', () => {
    expect(store.markAsLastKnownGood('ghost')).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// Session lifecycle
// ---------------------------------------------------------------------------

describe('SaveBackupStore — session lifecycle', () => {
  let store: SaveBackupStore;

  beforeEach(() => {
    store = new SaveBackupStore();
  });

  it('preSessionBackup() creates pre-session backups for all slots', () => {
    const slots = [
      { saveName: 'Slot 1', data: DATA },
      { saveName: 'Slot 2', data: DATA2 },
    ];
    const results = store.preSessionBackup(GAME_ID, slots);
    expect(results.length).toBe(2);
    expect(results.every((b) => b.reason === 'pre-session')).toBe(true);
    expect(results.every((b) => !b.isLastKnownGood)).toBe(true);
    // data field should be stripped
    expect(results.every((b) => !('data' in b))).toBe(true);
  });

  it('postSessionSync() creates post-session backups', () => {
    const slots = [{ saveName: 'Slot 1', data: DATA }];
    const results = store.postSessionSync(GAME_ID, slots, false);
    expect(results.length).toBe(1);
    expect(results[0].reason).toBe('post-session');
  });

  it('postSessionSync() marks LKG when cleanExit=true', () => {
    const slots = [{ saveName: 'Slot 1', data: DATA }];
    const results = store.postSessionSync(GAME_ID, slots, true);
    expect(results.length).toBe(1);
    const lkg = store.getLastKnownGood(GAME_ID, 'Slot 1');
    expect(lkg).not.toBeNull();
    expect(lkg!.id).toBeDefined();
  });

  it('postSessionSync() returned metadata reflects isLastKnownGood=true on clean exit', () => {
    const slots = [{ saveName: 'Slot 1', data: DATA }];
    const results = store.postSessionSync(GAME_ID, slots, true);
    expect(results[0].isLastKnownGood).toBe(true);
    expect(results[0].reason).toBe('last-known-good');
  });

  it('postSessionSync() returned metadata has isLastKnownGood=false on dirty exit', () => {
    const slots = [{ saveName: 'Slot 1', data: DATA }];
    const results = store.postSessionSync(GAME_ID, slots, false);
    expect(results[0].isLastKnownGood).toBe(false);
    expect(results[0].reason).toBe('post-session');
  });

  it('postSessionSync() does NOT mark LKG when cleanExit=false', () => {
    const slots = [{ saveName: 'Slot 1', data: DATA }];
    store.postSessionSync(GAME_ID, slots, false);
    expect(store.getLastKnownGood(GAME_ID, 'Slot 1')).toBeNull();
  });
});

// ---------------------------------------------------------------------------
// SqliteSaveBackupStore — SQLite backed
// ---------------------------------------------------------------------------

describe('SqliteSaveBackupStore — SQLite backed', () => {
  let store: SqliteSaveBackupStore;

  beforeEach(() => {
    const db = openDatabase(':memory:');
    store = new SqliteSaveBackupStore(db);
  });

  it('creates and retrieves a backup', () => {
    const b = store.createBackup(GAME_ID, SAVE_NAME, DATA, { reason: 'pre-session' });
    const found = store.get(b.id);
    expect(found).not.toBeNull();
    expect(found!.data).toBe(DATA);
    expect(found!.reason).toBe('pre-session');
  });

  it('listForGame() returns backups without data', () => {
    store.createBackup(GAME_ID, 'Slot 1', DATA);
    store.createBackup(GAME_ID, 'Slot 2', DATA2);
    store.createBackup('other', 'Slot 1', DATA);

    const list = store.listForGame(GAME_ID);
    expect(list.length).toBe(2);
    expect(list.every((b) => !('data' in b))).toBe(true);
  });

  it('delete() removes a backup', () => {
    const b = store.createBackup(GAME_ID, SAVE_NAME, DATA);
    expect(store.delete(b.id)).toBe(true);
    expect(store.get(b.id)).toBeNull();
  });

  it('markAsLastKnownGood() and getLastKnownGood() work correctly', () => {
    const b1 = store.createBackup(GAME_ID, SAVE_NAME, DATA);
    const b2 = store.createBackup(GAME_ID, SAVE_NAME, DATA2);

    store.markAsLastKnownGood(b1.id);
    expect(store.get(b1.id)!.isLastKnownGood).toBe(true);

    // Promote b2 — should clear b1's flag
    store.markAsLastKnownGood(b2.id);
    expect(store.get(b1.id)!.isLastKnownGood).toBe(false);
    expect(store.get(b2.id)!.isLastKnownGood).toBe(true);

    const lkg = store.getLastKnownGood(GAME_ID, SAVE_NAME);
    expect(lkg!.id).toBe(b2.id);
  });

  it('preSessionBackup() persists to SQLite', () => {
    const results = store.preSessionBackup(GAME_ID, [
      { saveName: 'Slot 1', data: DATA, version: 3 },
    ]);
    expect(results[0].reason).toBe('pre-session');
    expect(results[0].saveVersion).toBe(3);
  });

  it('postSessionSync() marks LKG on clean exit', () => {
    store.postSessionSync(GAME_ID, [{ saveName: 'Slot 1', data: DATA }], true);
    const lkg = store.getLastKnownGood(GAME_ID, 'Slot 1');
    expect(lkg).not.toBeNull();
    expect(lkg!.reason).toBe('last-known-good');
  });

  it('postSessionSync() returned metadata reflects isLastKnownGood=true on clean exit', () => {
    const results = store.postSessionSync(GAME_ID, [{ saveName: 'Slot 1', data: DATA }], true);
    expect(results[0].isLastKnownGood).toBe(true);
    expect(results[0].reason).toBe('last-known-good');
  });

  it('postSessionSync() returned metadata has isLastKnownGood=false on dirty exit', () => {
    const results = store.postSessionSync(GAME_ID, [{ saveName: 'Slot 1', data: DATA }], false);
    expect(results[0].isLastKnownGood).toBe(false);
    expect(results[0].reason).toBe('post-session');
  });

  it('enforces MAX_BACKUPS_PER_SLOT via eviction', () => {
    for (let i = 0; i < 21; i++) {
      store.createBackup(GAME_ID, SAVE_NAME, DATA);
    }
    const list = store.listForSlot(GAME_ID, SAVE_NAME);
    expect(list.length).toBe(20);
  });
});

// ---------------------------------------------------------------------------
// save_backups table exists in schema
// ---------------------------------------------------------------------------

describe('Database schema — save_backups table', () => {
  it('save_backups table is created by openDatabase()', () => {
    const db = openDatabase(':memory:');
    const row = db
      .prepare<[], { name: string }>(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='save_backups'"
      )
      .get();
    expect(row).toBeDefined();
    expect(row!.name).toBe('save_backups');
  });

  it('save_backups has all required columns', () => {
    const db = openDatabase(':memory:');
    const cols = db
      .prepare<[], { name: string }>('PRAGMA table_info(save_backups)')
      .all()
      .map((c) => c.name);
    expect(cols).toContain('id');
    expect(cols).toContain('game_id');
    expect(cols).toContain('save_name');
    expect(cols).toContain('data');
    expect(cols).toContain('reason');
    expect(cols).toContain('is_last_known_good');
    expect(cols).toContain('save_version');
    expect(cols).toContain('created_at');
  });
});

// ---------------------------------------------------------------------------
// REST API integration (direct handler tests via http-like simulation)
// ---------------------------------------------------------------------------

// We test the REST layer by exercising the SaveBackupStore + SaveStore
// together the way the index.ts route handlers do, without spinning up
// a real HTTP server.

describe('REST backup logic — SaveBackupStore + SaveStore integration', () => {
  let backupStore: SaveBackupStore;
  let saveStore: SaveStore;

  beforeEach(() => {
    backupStore = new SaveBackupStore();
    saveStore = new SaveStore();
  });

  it('backup then restore writes backup data back to the save store', () => {
    // Simulate POST /api/saves/:gameId/backup
    const backup = backupStore.createBackup(GAME_ID, SAVE_NAME, DATA, { reason: 'pre-session' });

    // Simulate POST /api/saves/:gameId/backups/:id/restore
    const restored = saveStore.put(GAME_ID, backup.saveName, backup.data, backup.mimeType);
    expect('conflict' in restored).toBe(false);
    const record = restored as ReturnType<typeof saveStore.get>;
    expect(record).toBeTruthy();

    // Data was written back to canonical save
    const fetched = saveStore.get((restored as { id: string }).id);
    expect(fetched!.data).toBe(DATA);
  });

  it('session-start creates pre-session backups for each slot', () => {
    const slots = [
      { saveName: 'Slot 1', data: DATA },
      { saveName: 'Slot 2', data: DATA2 },
    ];
    const backups = backupStore.preSessionBackup(GAME_ID, slots);
    expect(backups.length).toBe(2);
    expect(backups.every((b) => b.reason === 'pre-session')).toBe(true);
  });

  it('session-end updates canonical save and creates post-session backup', () => {
    const slots = [{ saveName: 'Slot 1', data: DATA2 }];

    // Simulate session-end: update saves + create post-session backup
    for (const slot of slots) {
      saveStore.put(GAME_ID, slot.saveName, slot.data);
    }
    const backups = backupStore.postSessionSync(GAME_ID, slots, true);
    // On a clean exit the reason is promoted to 'last-known-good' (matches returned metadata)
    expect(backups[0].reason).toBe('last-known-good');

    // LKG should be marked
    const lkg = backupStore.getLastKnownGood(GAME_ID, 'Slot 1');
    expect(lkg).not.toBeNull();

    // Canonical save has updated data
    const listed = saveStore.listForGame(GAME_ID);
    expect(listed.length).toBe(1);
    expect(saveStore.get(listed[0].id)!.data).toBe(DATA2);
  });

  it('mark-lkg endpoint logic works with the backup store', () => {
    const b = backupStore.createBackup(GAME_ID, SAVE_NAME, DATA);
    const ok = backupStore.markAsLastKnownGood(b.id);
    expect(ok).toBe(true);
    const lkg = backupStore.getLastKnownGood(GAME_ID, SAVE_NAME);
    expect(lkg!.id).toBe(b.id);
  });

  it('listing last-known-good returns null when no backup is marked', () => {
    backupStore.createBackup(GAME_ID, SAVE_NAME, DATA, { reason: 'pre-session' });
    expect(backupStore.getLastKnownGood(GAME_ID, SAVE_NAME)).toBeNull();
  });

  it('deleting a backup removes it from the list', () => {
    const b = backupStore.createBackup(GAME_ID, SAVE_NAME, DATA);
    backupStore.delete(b.id);
    expect(backupStore.listForGame(GAME_ID).length).toBe(0);
  });
});

// ---------------------------------------------------------------------------
// Save path discovery  (packages/save-system/src/discovery.ts)
// ---------------------------------------------------------------------------

describe('getBackendSaveExtensions()', () => {
  it('returns expected SRAM extension for fceux', () => {
    const { sramExtensions } = getBackendSaveExtensions('fceux');
    expect(sramExtensions).toContain('.sav');
  });

  it('returns expected SRAM extension for snes9x', () => {
    const { sramExtensions } = getBackendSaveExtensions('snes9x');
    expect(sramExtensions).toContain('.srm');
  });

  it('returns expected state extension for mupen64plus', () => {
    const { stateExtensions } = getBackendSaveExtensions('mupen64plus');
    expect(stateExtensions.length).toBeGreaterThan(0);
  });

  it('returns a fallback for unknown backends', () => {
    const { sramExtensions, stateExtensions } = getBackendSaveExtensions('unknown-backend');
    expect(sramExtensions.length).toBeGreaterThan(0);
    expect(stateExtensions.length).toBeGreaterThan(0);
  });

  it('dolphin returns .gci as a SRAM extension', () => {
    const { sramExtensions } = getBackendSaveExtensions('dolphin');
    expect(sramExtensions).toContain('.gci');
  });

  it('melonds returns .sav and .dsv as SRAM extensions', () => {
    const { sramExtensions } = getBackendSaveExtensions('melonds');
    expect(sramExtensions).toContain('.sav');
    expect(sramExtensions).toContain('.dsv');
  });

  it('retroarch returns .srm as a SRAM extension', () => {
    const { sramExtensions } = getBackendSaveExtensions('retroarch');
    expect(sramExtensions).toContain('.srm');
  });
});

describe('buildBackendSavePath()', () => {
  it('builds a correct SRAM path for snes9x', () => {
    const p = buildBackendSavePath('super-mario-world', 'snes9x', '/saves');
    expect(p).toBe('/saves/super-mario-world.srm');
  });

  it('builds a correct SRAM path for fceux', () => {
    const p = buildBackendSavePath('contra', 'fceux', '/saves');
    expect(p).toBe('/saves/contra.sav');
  });

  it('builds a save state path with slot index', () => {
    const p = buildBackendSavePath('mario-kart', 'snes9x', '/saves', 'state', 2);
    expect(p).toBe('/saves/mario-kart.ss2');
  });

  it('strips trailing slash from saveDir', () => {
    const p = buildBackendSavePath('game', 'fceux', '/saves/');
    expect(p).toBe('/saves/game.sav');
  });
});

describe('inferBackendFromExtension()', () => {
  it('infers dolphin from .gci', () => {
    expect(inferBackendFromExtension('.gci')).toBe('dolphin');
  });

  it('infers snes9x from .srm for snes system', () => {
    expect(inferBackendFromExtension('.srm', 'snes')).toBe('snes9x');
  });

  it('infers mupen64plus from .eep', () => {
    expect(inferBackendFromExtension('.eep')).toBe('mupen64plus');
  });

  it('infers pcsx2 from .mcd', () => {
    expect(inferBackendFromExtension('.mcd')).toBe('pcsx2');
  });

  it('infers flycast from .vms (Dreamcast VMU format)', () => {
    expect(inferBackendFromExtension('.vms')).toBe('flycast');
  });

  it('returns null for a generic .sav extension', () => {
    expect(inferBackendFromExtension('.sav')).toBeNull();
  });
});

describe('discoverSaveFiles()', () => {
  it('discovers SRAM files for the preferred backend', async () => {
    const fakeDir = async (_dir: string) => ['mario-kart.sav', 'mario-kart.state', 'other.sav'];

    const results = await discoverSaveFiles('mario-kart', '/saves', 'mgba', fakeDir);
    const sram = results.filter((r) => r.saveType === 'sram');
    expect(sram.length).toBeGreaterThan(0);
    expect(sram[0].gameBasename).toBe('mario-kart');
    expect(sram[0].backendId).toBe('mgba');
  });

  it('returns results sorted: preferred backend SRAM first', async () => {
    const fakeDir = async (_dir: string) => ['tetris.sav', 'tetris.srm'];
    const results = await discoverSaveFiles('tetris', '/saves', 'snes9x', fakeDir);
    // snes9x prefers .srm for SRAM
    expect(results[0].saveType).toBe('sram');
    expect(results[0].extension).toBe('.srm');
  });

  it('returns empty array when the directory does not exist', async () => {
    const fakeDir = async (_dir: string): Promise<string[]> => { throw new Error('ENOENT'); };
    const results = await discoverSaveFiles('game', '/no-such-dir', 'fceux', fakeDir);
    expect(results).toEqual([]);
  });

  it('ignores files that do not match the game basename', async () => {
    const fakeDir = async (_dir: string) => ['other-game.sav', 'pokemon.sav'];
    const results = await discoverSaveFiles('zelda', '/saves', 'mgba', fakeDir);
    expect(results).toEqual([]);
  });

  it('discovers dolphin .gci files as sram via extension inference', async () => {
    const fakeDir = async (_dir: string) => ['twilight-princess.gci'];
    const results = await discoverSaveFiles('twilight-princess', '/saves', 'dolphin', fakeDir);
    expect(results.length).toBe(1);
    expect(results[0].saveType).toBe('sram');
    expect(results[0].backendId).toBe('dolphin');
  });
});
