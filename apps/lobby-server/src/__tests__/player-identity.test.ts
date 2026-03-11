/**
 * Tests for the player identity store.
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { openDatabase } from '../db';
import { PlayerIdentityStore } from '../player-identity';

describe('PlayerIdentityStore', () => {
  let store: PlayerIdentityStore;

  beforeEach(() => {
    const db = openDatabase(':memory:');
    store = new PlayerIdentityStore(db);
  });

  it('creates a new identity on first resolve', () => {
    const token = PlayerIdentityStore.generateToken();
    const identity = store.resolve(token, 'Alice');
    expect(identity.displayName).toBe('Alice');
    expect(identity.id).toBe(token);
    expect(identity.createdAt).toBeTruthy();
    expect(identity.lastSeenAt).toBeTruthy();
  });

  it('returns the same identity on subsequent resolves', () => {
    const token = PlayerIdentityStore.generateToken();
    const first = store.resolve(token, 'Alice');
    const second = store.resolve(token, 'IrrelevantName');
    expect(second.id).toBe(first.id);
    expect(second.displayName).toBe('Alice');
  });

  it('updates display name when updateName=true', () => {
    const token = PlayerIdentityStore.generateToken();
    store.resolve(token, 'Alice');
    const updated = store.resolve(token, 'AliceUpdated', true);
    expect(updated.displayName).toBe('AliceUpdated');
  });

  it('updates last_seen_at on each resolve', async () => {
    const token = PlayerIdentityStore.generateToken();
    const first = store.resolve(token, 'Alice');
    await new Promise((r) => setTimeout(r, 10));
    const second = store.resolve(token, 'Alice');
    expect(second.lastSeenAt >= first.lastSeenAt).toBe(true);
  });

  it('getById returns existing identity', () => {
    const token = PlayerIdentityStore.generateToken();
    store.resolve(token, 'Bob');
    const fetched = store.getById(token);
    expect(fetched).not.toBeNull();
    expect(fetched!.displayName).toBe('Bob');
  });

  it('getById returns null for unknown id', () => {
    expect(store.getById('non-existent')).toBeNull();
  });

  it('updateDisplayName updates the name', () => {
    const token = PlayerIdentityStore.generateToken();
    store.resolve(token, 'Carol');
    const updated = store.updateDisplayName(token, 'Carol2');
    expect(updated?.displayName).toBe('Carol2');
  });

  it('updateDisplayName returns null for unknown player', () => {
    expect(store.updateDisplayName('ghost', 'name')).toBeNull();
  });

  it('generateToken produces unique UUIDs', () => {
    const tokens = new Set(Array.from({ length: 10 }, () => PlayerIdentityStore.generateToken()));
    expect(tokens.size).toBe(10);
  });
});
