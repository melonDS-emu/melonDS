/**
 * Tests for the auth/identity utilities.
 */
import { describe, it, expect } from 'vitest';
import {
  normalizeDisplayName,
  validateDisplayName,
  generateOwnerToken,
  verifyOwnerToken,
  MAX_DISPLAY_NAME_LENGTH,
} from '../auth';

describe('normalizeDisplayName', () => {
  it('trims leading and trailing whitespace', () => {
    expect(normalizeDisplayName('  Alice  ')).toBe('Alice');
  });

  it('collapses internal whitespace to a single space', () => {
    expect(normalizeDisplayName('Bob   Smith')).toBe('Bob Smith');
  });

  it('returns empty string unchanged (no crash)', () => {
    expect(normalizeDisplayName('')).toBe('');
  });
});

describe('validateDisplayName', () => {
  it('accepts a valid name', () => {
    expect(validateDisplayName('Player1')).toBeNull();
  });

  it('rejects an empty string', () => {
    expect(validateDisplayName('')).not.toBeNull();
  });

  it('rejects a name that is too long', () => {
    const longName = 'A'.repeat(MAX_DISPLAY_NAME_LENGTH + 1);
    expect(validateDisplayName(longName)).not.toBeNull();
  });

  it('accepts a name exactly at the max length', () => {
    const maxName = 'A'.repeat(MAX_DISPLAY_NAME_LENGTH);
    expect(validateDisplayName(maxName)).toBeNull();
  });

  it('rejects names with control characters', () => {
    expect(validateDisplayName('Bad\x00Name')).not.toBeNull();
    expect(validateDisplayName('Tabs\tHere')).not.toBeNull();
  });
});

describe('generateOwnerToken / verifyOwnerToken', () => {
  it('generates a non-empty token for a room ID', () => {
    const token = generateOwnerToken('room-123');
    expect(typeof token).toBe('string');
    expect(token.length).toBeGreaterThan(0);
  });

  it('verifies a correct token', () => {
    const roomId = 'some-room-id';
    const token = generateOwnerToken(roomId);
    expect(verifyOwnerToken(roomId, token)).toBe(true);
  });

  it('rejects an incorrect token', () => {
    const roomId = 'some-room-id';
    expect(verifyOwnerToken(roomId, 'wrong-token-value-here-xxxxxx')).toBe(false);
  });

  it('rejects a token for a different room ID', () => {
    const token = generateOwnerToken('room-A');
    expect(verifyOwnerToken('room-B', token)).toBe(false);
  });

  it('is deterministic within a process', () => {
    const t1 = generateOwnerToken('room-xyz');
    const t2 = generateOwnerToken('room-xyz');
    expect(t1).toBe(t2);
  });
});
