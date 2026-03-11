/**
 * Tests for the in-memory save store.
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { SaveStore } from '../save-store';

const GAME_ID = 'mk64';
const SAVE_DATA = Buffer.from('fake-save-data').toString('base64');

describe('SaveStore', () => {
  let store: SaveStore;

  beforeEach(() => {
    store = new SaveStore();
  });

  it('creates a new save record', () => {
    const result = store.put(GAME_ID, 'Slot 1', SAVE_DATA);
    expect('conflict' in result).toBe(false);
    const record = result as import('../save-store').SaveRecord;
    expect(record.gameId).toBe(GAME_ID);
    expect(record.name).toBe('Slot 1');
    expect(record.data).toBe(SAVE_DATA);
    expect(record.version).toBe(1);
    expect(record.id).toBeTruthy();
  });

  it('retrieves a save by ID', () => {
    const created = store.put(GAME_ID, 'Slot 1', SAVE_DATA) as import('../save-store').SaveRecord;
    const fetched = store.get(created.id);
    expect(fetched).not.toBeNull();
    expect(fetched!.data).toBe(SAVE_DATA);
  });

  it('overwrites an existing save slot', () => {
    store.put(GAME_ID, 'Slot 1', SAVE_DATA);
    const newData = Buffer.from('updated-data').toString('base64');
    const updated = store.put(GAME_ID, 'Slot 1', newData) as import('../save-store').SaveRecord;
    expect(updated.version).toBe(2);
    expect(updated.data).toBe(newData);
  });

  it('detects a conflict when expectedVersion is stale', () => {
    store.put(GAME_ID, 'Slot 1', SAVE_DATA); // version 1
    const newData = Buffer.from('v2').toString('base64');
    store.put(GAME_ID, 'Slot 1', newData);   // version 2

    // Attempt to overwrite as if we had version 1
    const thirdData = Buffer.from('v3').toString('base64');
    const result = store.put(GAME_ID, 'Slot 1', thirdData, 'application/octet-stream', 1);
    expect('conflict' in result && (result as import('../save-store').ConflictError).conflict).toBe(true);
    const err = result as import('../save-store').ConflictError;
    expect(err.serverVersion).toBe(2);
  });

  it('allows an overwrite when expectedVersion matches', () => {
    store.put(GAME_ID, 'Slot 1', SAVE_DATA); // version 1
    const newData = Buffer.from('updated').toString('base64');
    const result = store.put(GAME_ID, 'Slot 1', newData, 'application/octet-stream', 1);
    expect('conflict' in result).toBe(false);
    const record = result as import('../save-store').SaveRecord;
    expect(record.version).toBe(2);
  });

  it('lists saves for a game without data field', () => {
    store.put(GAME_ID, 'Slot 1', SAVE_DATA);
    store.put(GAME_ID, 'Slot 2', SAVE_DATA);
    store.put('other-game', 'Slot 1', SAVE_DATA);

    const list = store.listForGame(GAME_ID);
    expect(list.length).toBe(2);
    for (const item of list) {
      expect('data' in item).toBe(false);
    }
  });

  it('deletes a save', () => {
    const created = store.put(GAME_ID, 'Slot 1', SAVE_DATA) as import('../save-store').SaveRecord;
    const ok = store.delete(created.id, GAME_ID);
    expect(ok).toBe(true);
    expect(store.get(created.id)).toBeNull();
  });

  it('returns false when deleting a non-existent save', () => {
    expect(store.delete('ghost-id', GAME_ID)).toBe(false);
  });

  it('returns false when deleting a save with mismatched gameId', () => {
    const created = store.put(GAME_ID, 'Slot 1', SAVE_DATA) as import('../save-store').SaveRecord;
    expect(store.delete(created.id, 'different-game')).toBe(false);
  });
});
