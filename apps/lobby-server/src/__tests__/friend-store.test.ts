/**
 * Tests for the friend store (friend list + request flow).
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { openDatabase } from '../db';
import { FriendStore } from '../friend-store';

describe('FriendStore', () => {
  let store: FriendStore;

  beforeEach(() => {
    const db = openDatabase(':memory:');
    store = new FriendStore(db);
  });

  it('sends a friend request', () => {
    const req = store.addRequest('alice', 'bob');
    expect(req).not.toBeNull();
    expect(req!.fromId).toBe('alice');
    expect(req!.toId).toBe('bob');
    expect(req!.status).toBe('pending');
  });

  it('does not duplicate a pending request', () => {
    const r1 = store.addRequest('alice', 'bob');
    const r2 = store.addRequest('alice', 'bob');
    expect(r1!.id).toBe(r2!.id);
  });

  it('accepts a friend request and creates friendship', () => {
    const req = store.addRequest('alice', 'bob')!;
    const accepted = store.acceptRequest(req.id, 'bob');
    expect(accepted?.status).toBe('accepted');
    expect(store.areFriends('alice', 'bob')).toBe(true);
    expect(store.areFriends('bob', 'alice')).toBe(true);
  });

  it('declines a friend request', () => {
    const req = store.addRequest('alice', 'bob')!;
    const declined = store.declineRequest(req.id, 'bob');
    expect(declined?.status).toBe('declined');
    expect(store.areFriends('alice', 'bob')).toBe(false);
  });

  it('cannot accept as wrong user', () => {
    const req = store.addRequest('alice', 'bob')!;
    expect(store.acceptRequest(req.id, 'alice')).toBeNull();
  });

  it('cannot send a request to yourself', () => {
    expect(store.addRequest('alice', 'alice')).toBeNull();
  });

  it('returns null when adding request between existing friends', () => {
    const req = store.addRequest('alice', 'bob')!;
    store.acceptRequest(req.id, 'bob');
    // Now they are friends — no new request should be created
    expect(store.addRequest('alice', 'bob')).toBeNull();
  });

  it('removes a friendship (both directions)', () => {
    const req = store.addRequest('alice', 'bob')!;
    store.acceptRequest(req.id, 'bob');
    const ok = store.removeFriend('alice', 'bob');
    expect(ok).toBe(true);
    expect(store.areFriends('alice', 'bob')).toBe(false);
    expect(store.areFriends('bob', 'alice')).toBe(false);
  });

  it('returns false when removing a non-existent friendship', () => {
    expect(store.removeFriend('alice', 'charlie')).toBe(false);
  });

  it('lists pending incoming requests', () => {
    store.addRequest('alice', 'bob');
    store.addRequest('carol', 'bob');
    const pending = store.getPendingRequests('bob');
    expect(pending.length).toBe(2);
  });

  it('lists sent requests', () => {
    store.addRequest('alice', 'bob');
    store.addRequest('alice', 'carol');
    const sent = store.getSentRequests('alice');
    expect(sent.length).toBe(2);
  });

  it('lists friends', () => {
    const req1 = store.addRequest('alice', 'bob')!;
    store.acceptRequest(req1.id, 'bob');
    const req2 = store.addRequest('alice', 'carol')!;
    store.acceptRequest(req2.id, 'carol');
    const friends = store.getFriends('alice');
    expect(friends.length).toBe(2);
  });
});
