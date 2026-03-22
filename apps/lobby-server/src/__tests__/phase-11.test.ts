/**
 * Phase 11 — Cloud and Account Features tests
 *
 * Covers:
 *   - Optional account system (AccountStore)
 *     - Registration, login, session tokens
 *     - Profile management
 *     - Password change
 *     - Identity linking (cloud profile ↔ anonymous identity)
 *   - Lightweight moderation/reporting (ModerationStore)
 *     - Filing reports against players and rooms
 *     - Querying and filtering reports
 *     - Auto-flag threshold
 *     - Moderator review and dismiss actions
 *   - Cloud save sync already covered by cloud-sync.test.ts
 *   - Friend graph persistence already covered by friend-store.test.ts
 *   - Session history persistence already covered by session-history.test.ts
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { openDatabase } from '../db';
import {
  AccountStore,
  hashPassword,
  verifyPassword,
} from '../account-store';
import {
  ModerationStore,
  AUTO_FLAG_THRESHOLD,
} from '../moderation-store';
import type { DatabaseType } from '../db';

// ---------------------------------------------------------------------------
// Test database setup
// ---------------------------------------------------------------------------

let db: DatabaseType;

beforeEach(() => {
  // Each test gets a clean in-memory database with the full schema applied.
  db = openDatabase(':memory:');
});

// ===========================================================================
// hashPassword / verifyPassword
// ===========================================================================

describe('hashPassword / verifyPassword', () => {
  it('produces a non-empty hash string', () => {
    const hash = hashPassword('mypassword');
    expect(hash).toBeTruthy();
    expect(hash).toContain(':');
  });

  it('verifies the correct plaintext password', () => {
    const hash = hashPassword('correcthorsebatterystaple');
    expect(verifyPassword('correcthorsebatterystaple', hash)).toBe(true);
  });

  it('rejects a wrong password', () => {
    const hash = hashPassword('secretpass');
    expect(verifyPassword('wrongpass', hash)).toBe(false);
  });

  it('returns false for a malformed hash string', () => {
    expect(verifyPassword('anypass', 'not-a-valid-hash')).toBe(false);
  });

  it('produces different hashes for the same password (salt randomisation)', () => {
    const h1 = hashPassword('samepassword');
    const h2 = hashPassword('samepassword');
    expect(h1).not.toBe(h2);
    // But both verify correctly
    expect(verifyPassword('samepassword', h1)).toBe(true);
    expect(verifyPassword('samepassword', h2)).toBe(true);
  });
});

// ===========================================================================
// AccountStore — registration
// ===========================================================================

describe('AccountStore — register', () => {
  it('creates a new account and returns it', () => {
    const store = new AccountStore(db);
    const account = store.register('user@example.com', 'Player1', 'pass123');
    expect(account).not.toBeNull();
    expect(account!.email).toBe('user@example.com');
    expect(account!.displayName).toBe('Player1');
    expect(account!.id).toBeTruthy();
    expect(account!.isVerified).toBe(false);
    expect(account!.linkedIdentityToken).toBeNull();
  });

  it('normalises the email to lower-case', () => {
    const store = new AccountStore(db);
    const account = store.register('USER@Example.COM', 'Player1', 'pass123');
    expect(account!.email).toBe('user@example.com');
  });

  it('returns null for a duplicate email', () => {
    const store = new AccountStore(db);
    store.register('dup@example.com', 'Player1', 'pass1');
    const second = store.register('dup@example.com', 'Player2', 'pass2');
    expect(second).toBeNull();
  });

  it('returns null for an empty email', () => {
    const store = new AccountStore(db);
    expect(store.register('', 'Player1', 'pass')).toBeNull();
  });
});

// ===========================================================================
// AccountStore — login & session tokens
// ===========================================================================

describe('AccountStore — login', () => {
  it('returns a session token on correct credentials', () => {
    const store = new AccountStore(db);
    store.register('login@example.com', 'Player1', 'goodpass');
    const session = store.login('login@example.com', 'goodpass');
    expect(session).not.toBeNull();
    expect(session!.token).toBeTruthy();
    expect(session!.token).toHaveLength(64);
    expect(session!.accountId).toBeTruthy();
  });

  it('returns null for an unknown email', () => {
    const store = new AccountStore(db);
    expect(store.login('nobody@example.com', 'pass')).toBeNull();
  });

  it('returns null for a wrong password', () => {
    const store = new AccountStore(db);
    store.register('auth@example.com', 'Player1', 'correct');
    expect(store.login('auth@example.com', 'wrong')).toBeNull();
  });

  it('updates lastLoginAt on successful login', () => {
    const store = new AccountStore(db);
    store.register('ts@example.com', 'Player1', 'pass');
    const profile1 = store.getByEmail('ts@example.com');
    expect(profile1!.lastLoginAt).toBeNull();

    store.login('ts@example.com', 'pass');
    const profile2 = store.getByEmail('ts@example.com');
    expect(profile2!.lastLoginAt).not.toBeNull();
  });
});

// ===========================================================================
// AccountStore — validateSession
// ===========================================================================

describe('AccountStore — validateSession', () => {
  it('returns the account profile for a valid session', () => {
    const store = new AccountStore(db);
    store.register('sess@example.com', 'Player1', 'pass');
    const session = store.login('sess@example.com', 'pass')!;
    const profile = store.validateSession(session.token);
    expect(profile).not.toBeNull();
    expect(profile!.email).toBe('sess@example.com');
  });

  it('returns null for an unknown token', () => {
    const store = new AccountStore(db);
    expect(store.validateSession('nonexistent-token')).toBeNull();
  });

  it('returns null after the session is revoked', () => {
    const store = new AccountStore(db);
    store.register('revoke@example.com', 'Player1', 'pass');
    const session = store.login('revoke@example.com', 'pass')!;
    store.revokeSession(session.token);
    expect(store.validateSession(session.token)).toBeNull();
  });

  it('revokeAllSessions invalidates all tokens for the account', () => {
    const store = new AccountStore(db);
    store.register('multi@example.com', 'Player1', 'pass');
    const s1 = store.login('multi@example.com', 'pass')!;
    const s2 = store.login('multi@example.com', 'pass')!;

    store.revokeAllSessions(s1.accountId);

    expect(store.validateSession(s1.token)).toBeNull();
    expect(store.validateSession(s2.token)).toBeNull();
  });
});

// ===========================================================================
// AccountStore — getById / getByEmail
// ===========================================================================

describe('AccountStore — profile lookup', () => {
  it('getById returns the profile without the password hash', () => {
    const store = new AccountStore(db);
    const created = store.register('lookup@example.com', 'Player1', 'pass')!;
    const profile = store.getById(created.id);
    expect(profile).not.toBeNull();
    expect((profile as unknown as Record<string, unknown>).passwordHash).toBeUndefined();
  });

  it('getById returns null for an unknown id', () => {
    const store = new AccountStore(db);
    expect(store.getById('unknown-id')).toBeNull();
  });

  it('getByEmail finds the account by normalised email', () => {
    const store = new AccountStore(db);
    store.register('EMAIL@Example.com', 'Player1', 'pass');
    const profile = store.getByEmail('email@example.com');
    expect(profile).not.toBeNull();
    expect(profile!.email).toBe('email@example.com');
  });
});

// ===========================================================================
// AccountStore — updateProfile
// ===========================================================================

describe('AccountStore — updateProfile', () => {
  it('updates displayName', () => {
    const store = new AccountStore(db);
    const created = store.register('upd@example.com', 'OldName', 'pass')!;
    const updated = store.updateProfile(created.id, { displayName: 'NewName' });
    expect(updated!.displayName).toBe('NewName');
  });

  it('marks account as verified', () => {
    const store = new AccountStore(db);
    const created = store.register('verify@example.com', 'Player1', 'pass')!;
    expect(created.isVerified).toBe(false);
    const updated = store.updateProfile(created.id, { isVerified: true });
    expect(updated!.isVerified).toBe(true);
  });

  it('returns null for an unknown account', () => {
    const store = new AccountStore(db);
    expect(store.updateProfile('no-such-id', { displayName: 'x' })).toBeNull();
  });
});

// ===========================================================================
// AccountStore — changePassword
// ===========================================================================

describe('AccountStore — changePassword', () => {
  it('accepts the new password after change', () => {
    const store = new AccountStore(db);
    const created = store.register('pw@example.com', 'Player1', 'oldpass')!;
    const ok = store.changePassword(created.id, 'oldpass', 'newpass');
    expect(ok).toBe(true);
    expect(store.login('pw@example.com', 'newpass')).not.toBeNull();
  });

  it('rejects the old password after change', () => {
    const store = new AccountStore(db);
    const created = store.register('pw2@example.com', 'Player1', 'oldpass')!;
    store.changePassword(created.id, 'oldpass', 'newpass');
    expect(store.login('pw2@example.com', 'oldpass')).toBeNull();
  });

  it('returns false when oldPassword is wrong', () => {
    const store = new AccountStore(db);
    const created = store.register('pw3@example.com', 'Player1', 'realpass')!;
    expect(store.changePassword(created.id, 'wrongpass', 'newpass')).toBe(false);
  });

  it('revokes all sessions on password change', () => {
    const store = new AccountStore(db);
    const created = store.register('pwsess@example.com', 'Player1', 'pass')!;
    const session = store.login('pwsess@example.com', 'pass')!;
    store.changePassword(created.id, 'pass', 'newpass');
    expect(store.validateSession(session.token)).toBeNull();
  });
});

// ===========================================================================
// AccountStore — linkIdentity (cloud profile ↔ anonymous identity)
// ===========================================================================

describe('AccountStore — linkIdentity', () => {
  it('links an identity token to an account', () => {
    const store = new AccountStore(db);
    const created = store.register('link@example.com', 'Player1', 'pass')!;
    const token = 'anon-identity-token-uuid-1234';
    const ok = store.linkIdentity(created.id, token);
    expect(ok).toBe(true);
    const profile = store.getById(created.id);
    expect(profile!.linkedIdentityToken).toBe(token);
  });

  it('returns false for an unknown account', () => {
    const store = new AccountStore(db);
    expect(store.linkIdentity('no-such-id', 'some-token')).toBe(false);
  });
});

// ===========================================================================
// ModerationStore — filing reports
// ===========================================================================

describe('ModerationStore — report', () => {
  it('creates a new pending report', () => {
    const store = new ModerationStore(db);
    const report = store.report(
      'reporter1',
      'ReporterName',
      'target-player-1',
      'player',
      'harassment',
      'Kept sending offensive messages'
    );
    expect(report.id).toBeTruthy();
    expect(report.status).toBe('pending');
    expect(report.targetType).toBe('player');
    expect(report.reason).toBe('harassment');
    expect(report.description).toBe('Kept sending offensive messages');
    expect(report.reviewedAt).toBeNull();
  });

  it('truncates description to 1000 characters', () => {
    const store = new ModerationStore(db);
    const longDesc = 'x'.repeat(2000);
    const report = store.report('r1', 'Rname', 'target1', 'player', 'spam', longDesc);
    expect(report.description).toHaveLength(1000);
  });

  it('allows multiple reports for the same target from different reporters', () => {
    const store = new ModerationStore(db);
    store.report('r1', 'A', 'target1', 'room', 'spam', '');
    store.report('r2', 'B', 'target1', 'room', 'spam', '');
    expect(store.getReportsByTarget('target1')).toHaveLength(2);
  });

  it('allows reports against rooms', () => {
    const store = new ModerationStore(db);
    const report = store.report('r1', 'Reporter', 'room-abc', 'room', 'other', 'notes');
    expect(report.targetType).toBe('room');
  });
});

// ===========================================================================
// ModerationStore — querying
// ===========================================================================

describe('ModerationStore — getReports', () => {
  it('returns all reports when no status filter is given', () => {
    const store = new ModerationStore(db);
    store.report('r1', 'A', 'target1', 'player', 'spam', '');
    store.report('r2', 'B', 'target2', 'room', 'cheating', '');
    expect(store.getReports()).toHaveLength(2);
  });

  it('filters by pending status', () => {
    const store = new ModerationStore(db);
    const r1 = store.report('r1', 'A', 'target1', 'player', 'spam', '');
    store.report('r2', 'B', 'target2', 'room', 'cheating', '');
    store.reviewReport(r1.id);

    const pending = store.getReports('pending');
    expect(pending).toHaveLength(1);
    expect(pending[0].status).toBe('pending');
  });

  it('filters by reviewed status', () => {
    const store = new ModerationStore(db);
    const r1 = store.report('r1', 'A', 'target1', 'player', 'spam', '');
    store.reviewReport(r1.id, 'Actioned');

    const reviewed = store.getReports('reviewed');
    expect(reviewed).toHaveLength(1);
    expect(reviewed[0].resolvedNote).toBe('Actioned');
  });

  it('returns reports newest first', () => {
    const store = new ModerationStore(db);
    store.report('r1', 'A', 'target1', 'player', 'spam', '');
    store.report('r2', 'B', 'target2', 'room', 'cheating', '');

    const reports = store.getReports();
    expect(new Date(reports[0].createdAt).getTime()).toBeGreaterThanOrEqual(
      new Date(reports[1].createdAt).getTime()
    );
  });
});

describe('ModerationStore — getReportsByTarget', () => {
  it('returns only reports for the specified target', () => {
    const store = new ModerationStore(db);
    store.report('r1', 'A', 'target-A', 'player', 'spam', '');
    store.report('r2', 'B', 'target-B', 'player', 'harassment', '');

    const targetA = store.getReportsByTarget('target-A');
    expect(targetA).toHaveLength(1);
    expect(targetA[0].targetId).toBe('target-A');
  });

  it('returns empty array for an unreported target', () => {
    const store = new ModerationStore(db);
    expect(store.getReportsByTarget('no-such-target')).toHaveLength(0);
  });
});

// ===========================================================================
// ModerationStore — auto-flag threshold
// ===========================================================================

describe('ModerationStore — isFlagged', () => {
  it(`is not flagged below ${AUTO_FLAG_THRESHOLD} reports`, () => {
    const store = new ModerationStore(db);
    for (let i = 0; i < AUTO_FLAG_THRESHOLD - 1; i++) {
      store.report(`r${i}`, `Name${i}`, 'almost-flagged', 'player', 'spam', '');
    }
    expect(store.isFlagged('almost-flagged')).toBe(false);
  });

  it(`is flagged at ${AUTO_FLAG_THRESHOLD} or more pending reports`, () => {
    const store = new ModerationStore(db);
    for (let i = 0; i < AUTO_FLAG_THRESHOLD; i++) {
      store.report(`r${i}`, `Name${i}`, 'flagged-target', 'player', 'spam', '');
    }
    expect(store.isFlagged('flagged-target')).toBe(true);
    expect(store.getPendingCount('flagged-target')).toBe(AUTO_FLAG_THRESHOLD);
  });

  it('unflagged after reviewing reports', () => {
    const store = new ModerationStore(db);
    const ids: string[] = [];
    for (let i = 0; i < AUTO_FLAG_THRESHOLD; i++) {
      const r = store.report(`r${i}`, `Name${i}`, 'unflag-target', 'player', 'spam', '');
      ids.push(r.id);
    }
    for (const id of ids) {
      store.reviewReport(id);
    }
    expect(store.isFlagged('unflag-target')).toBe(false);
  });
});

// ===========================================================================
// ModerationStore — moderator actions
// ===========================================================================

describe('ModerationStore — reviewReport', () => {
  it('marks a report as reviewed', () => {
    const store = new ModerationStore(db);
    const report = store.report('r1', 'A', 'target1', 'player', 'spam', '');
    const updated = store.reviewReport(report.id, 'Warning issued');
    expect(updated!.status).toBe('reviewed');
    expect(updated!.reviewedAt).not.toBeNull();
    expect(updated!.resolvedNote).toBe('Warning issued');
  });

  it('returns null for a non-existent report', () => {
    const store = new ModerationStore(db);
    expect(store.reviewReport('no-such-id')).toBeNull();
  });
});

describe('ModerationStore — dismissReport', () => {
  it('marks a report as dismissed', () => {
    const store = new ModerationStore(db);
    const report = store.report('r1', 'A', 'target1', 'player', 'spam', '');
    const updated = store.dismissReport(report.id, 'No violation found');
    expect(updated!.status).toBe('dismissed');
    expect(updated!.resolvedNote).toBe('No violation found');
  });

  it('returns null for a non-existent report', () => {
    const store = new ModerationStore(db);
    expect(store.dismissReport('no-such-id')).toBeNull();
  });
});

describe('ModerationStore — clearReports', () => {
  it('dismisses all pending reports for a target', () => {
    const store = new ModerationStore(db);
    store.report('r1', 'A', 'clear-target', 'player', 'spam', '');
    store.report('r2', 'B', 'clear-target', 'player', 'spam', '');

    const cleared = store.clearReports('clear-target');
    expect(cleared).toBe(2);

    const pending = store.getReports('pending').filter((r) => r.targetId === 'clear-target');
    expect(pending).toHaveLength(0);
  });

  it('does not touch already-reviewed reports', () => {
    const store = new ModerationStore(db);
    const r1 = store.report('r1', 'A', 'mixed-target', 'player', 'spam', '');
    store.report('r2', 'B', 'mixed-target', 'player', 'spam', '');
    store.reviewReport(r1.id);

    const cleared = store.clearReports('mixed-target');
    expect(cleared).toBe(1); // Only the remaining pending report

    const reviewed = store.getReports('reviewed').filter((r) => r.targetId === 'mixed-target');
    expect(reviewed).toHaveLength(1);
  });
});

// ===========================================================================
// AccountStore — no bleed between independent stores / databases
// ===========================================================================

describe('AccountStore — isolation', () => {
  it('accounts in one database do not leak into another', () => {
    const db1 = openDatabase(':memory:');
    const db2 = openDatabase(':memory:');
    const store1 = new AccountStore(db1);
    const store2 = new AccountStore(db2);

    store1.register('isolated@example.com', 'Player1', 'pass');
    expect(store2.getByEmail('isolated@example.com')).toBeNull();
  });
});
