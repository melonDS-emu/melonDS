/**
 * Optional account system for RetroOasis.
 *
 * Accounts allow users to sign in on any machine and retain their saved
 * progress, friends, and profile data.  Accounts are completely optional —
 * the lobby works fine with anonymous identity tokens.
 *
 * Passwords are hashed with scrypt (N=16384, r=8, p=1) and stored as
 * hex strings.  Session tokens are random 32-byte hex strings.
 *
 * Flow:
 *   1. User registers with email + password → receives accountId + session token.
 *   2. On subsequent launches they log in with email + password → new session token.
 *   3. Desktop client sends session token in X-Account-Token header; server
 *      resolves it to an Account via `validateSession()`.
 *   4. Optionally, an existing anonymous identity token can be linked to the
 *      account so that pre-registration history is preserved.
 */

import { randomBytes, scryptSync, timingSafeEqual } from 'crypto';
import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

export interface Account {
  /** Stable UUID for this account. */
  id: string;
  /** Unique lower-cased email address. */
  email: string;
  /** Display name shown in-game. */
  displayName: string;
  /** scrypt-derived password hash (hex). */
  passwordHash: string;
  /** Optional link to a pre-existing anonymous identity token. */
  linkedIdentityToken: string | null;
  /** ISO timestamp of account creation. */
  createdAt: string;
  /** ISO timestamp of most recent successful login, or null. */
  lastLoginAt: string | null;
  /** Whether the account email has been verified. */
  isVerified: boolean;
}

export interface AccountSession {
  /** Random 64-char hex token sent to the client. */
  token: string;
  /** The account this session belongs to. */
  accountId: string;
  /** ISO timestamp when the session was created. */
  createdAt: string;
  /** ISO timestamp when the session expires. */
  expiresAt: string;
}

/** Public-facing account profile (no password hash). */
export interface AccountProfile {
  id: string;
  email: string;
  displayName: string;
  linkedIdentityToken: string | null;
  createdAt: string;
  lastLoginAt: string | null;
  isVerified: boolean;
}

// ---------------------------------------------------------------------------
// Internal DB row shapes
// ---------------------------------------------------------------------------

interface AccountRow {
  id: string;
  email: string;
  display_name: string;
  password_hash: string;
  linked_identity_token: string | null;
  created_at: string;
  last_login_at: string | null;
  is_verified: number;
}

interface SessionRow {
  token: string;
  account_id: string;
  created_at: string;
  expires_at: string;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Default session lifetime: 30 days. */
const SESSION_TTL_MS = 30 * 24 * 60 * 60 * 1000;

/** scrypt parameters (OWASP recommended minimum). */
const SCRYPT_N = 16384;
const SCRYPT_R = 8;
const SCRYPT_P = 1;
const SCRYPT_KEY_LEN = 64;

/**
 * Hash a plaintext password.
 * Format: `<salt_hex>:<hash_hex>` (128 + 128 = 256 hex chars separated by `:`)
 */
export function hashPassword(plaintext: string): string {
  const salt = randomBytes(64);
  const hash = scryptSync(plaintext, salt, SCRYPT_KEY_LEN, {
    N: SCRYPT_N,
    r: SCRYPT_R,
    p: SCRYPT_P,
  });
  return `${salt.toString('hex')}:${hash.toString('hex')}`;
}

/**
 * Verify a plaintext password against a stored hash string.
 * Returns true only when the password matches.
 */
export function verifyPassword(plaintext: string, storedHash: string): boolean {
  const parts = storedHash.split(':');
  if (parts.length !== 2) return false;
  const [saltHex, hashHex] = parts;
  try {
    const salt = Buffer.from(saltHex, 'hex');
    const expected = Buffer.from(hashHex, 'hex');
    const actual = scryptSync(plaintext, salt, SCRYPT_KEY_LEN, {
      N: SCRYPT_N,
      r: SCRYPT_R,
      p: SCRYPT_P,
    });
    return timingSafeEqual(actual, expected);
  } catch {
    return false;
  }
}

function rowToAccount(row: AccountRow): Account {
  return {
    id: row.id,
    email: row.email,
    displayName: row.display_name,
    passwordHash: row.password_hash,
    linkedIdentityToken: row.linked_identity_token,
    createdAt: row.created_at,
    lastLoginAt: row.last_login_at,
    isVerified: row.is_verified === 1,
  };
}

function rowToSession(row: SessionRow): AccountSession {
  return {
    token: row.token,
    accountId: row.account_id,
    createdAt: row.created_at,
    expiresAt: row.expires_at,
  };
}

function toProfile(account: Account): AccountProfile {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  const { passwordHash: _ignored, ...rest } = account;
  return rest;
}

// ---------------------------------------------------------------------------
// AccountStore
// ---------------------------------------------------------------------------

export class AccountStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
  }

  // -------------------------------------------------------------------------
  // Registration & login
  // -------------------------------------------------------------------------

  /**
   * Register a new account.
   *
   * @returns The new Account, or null if the email is already taken.
   */
  register(
    email: string,
    displayName: string,
    plainPassword: string
  ): Account | null {
    const normalised = email.trim().toLowerCase();
    if (!normalised) return null;

    // Check for duplicate email
    const existing = this.db
      .prepare<string, AccountRow>('SELECT id FROM accounts WHERE email = ?')
      .get(normalised);
    if (existing) return null;

    const id = randomUUID();
    const now = new Date().toISOString();
    const passwordHash = hashPassword(plainPassword);

    this.db
      .prepare(
        `INSERT INTO accounts
           (id, email, display_name, password_hash, linked_identity_token,
            created_at, last_login_at, is_verified)
         VALUES (?, ?, ?, ?, NULL, ?, NULL, 0)`
      )
      .run(id, normalised, displayName, passwordHash, now);

    return rowToAccount(
      this.db
        .prepare<string, AccountRow>('SELECT * FROM accounts WHERE id = ?')
        .get(id)!
    );
  }

  /**
   * Attempt to log in with email and password.
   *
   * @returns A new AccountSession on success, or null on bad credentials.
   */
  login(email: string, plainPassword: string): AccountSession | null {
    const normalised = email.trim().toLowerCase();
    const row = this.db
      .prepare<string, AccountRow>('SELECT * FROM accounts WHERE email = ?')
      .get(normalised);
    if (!row) return null;
    if (!verifyPassword(plainPassword, row.password_hash)) return null;

    // Update last_login_at
    const now = new Date().toISOString();
    this.db
      .prepare('UPDATE accounts SET last_login_at = ? WHERE id = ?')
      .run(now, row.id);

    return this.createSession(row.id);
  }

  // -------------------------------------------------------------------------
  // Session management
  // -------------------------------------------------------------------------

  /**
   * Create a new session token for an account.
   */
  createSession(accountId: string): AccountSession {
    const token = randomBytes(32).toString('hex');
    const now = new Date();
    const expiresAt = new Date(now.getTime() + SESSION_TTL_MS).toISOString();

    this.db
      .prepare(
        `INSERT INTO account_sessions (token, account_id, created_at, expires_at)
         VALUES (?, ?, ?, ?)`
      )
      .run(token, accountId, now.toISOString(), expiresAt);

    return rowToSession(
      this.db
        .prepare<string, SessionRow>(
          'SELECT * FROM account_sessions WHERE token = ?'
        )
        .get(token)!
    );
  }

  /**
   * Validate a session token and return the associated account profile.
   *
   * @returns The AccountProfile if valid and not expired, otherwise null.
   */
  validateSession(token: string): AccountProfile | null {
    const sessionRow = this.db
      .prepare<string, SessionRow>(
        'SELECT * FROM account_sessions WHERE token = ?'
      )
      .get(token);
    if (!sessionRow) return null;

    // Check expiry
    if (new Date(sessionRow.expires_at) < new Date()) {
      this.revokeSession(token);
      return null;
    }

    const accountRow = this.db
      .prepare<string, AccountRow>(
        'SELECT * FROM accounts WHERE id = ?'
      )
      .get(sessionRow.account_id);
    if (!accountRow) return null;

    return toProfile(rowToAccount(accountRow));
  }

  /**
   * Revoke (delete) a session token.
   */
  revokeSession(token: string): void {
    this.db
      .prepare('DELETE FROM account_sessions WHERE token = ?')
      .run(token);
  }

  /**
   * Revoke all sessions for an account (e.g. on password change).
   */
  revokeAllSessions(accountId: string): void {
    this.db
      .prepare('DELETE FROM account_sessions WHERE account_id = ?')
      .run(accountId);
  }

  // -------------------------------------------------------------------------
  // Profile management
  // -------------------------------------------------------------------------

  /**
   * Look up an account by its stable ID.
   */
  getById(id: string): AccountProfile | null {
    const row = this.db
      .prepare<string, AccountRow>('SELECT * FROM accounts WHERE id = ?')
      .get(id);
    return row ? toProfile(rowToAccount(row)) : null;
  }

  /**
   * Look up an account by email.
   */
  getByEmail(email: string): AccountProfile | null {
    const normalised = email.trim().toLowerCase();
    const row = this.db
      .prepare<string, AccountRow>('SELECT * FROM accounts WHERE email = ?')
      .get(normalised);
    return row ? toProfile(rowToAccount(row)) : null;
  }

  /**
   * Update mutable profile fields (displayName, isVerified).
   */
  updateProfile(
    id: string,
    changes: Partial<Pick<Account, 'displayName' | 'isVerified'>>
  ): AccountProfile | null {
    const row = this.db
      .prepare<string, AccountRow>('SELECT * FROM accounts WHERE id = ?')
      .get(id);
    if (!row) return null;

    if (changes.displayName !== undefined) {
      this.db
        .prepare('UPDATE accounts SET display_name = ? WHERE id = ?')
        .run(changes.displayName, id);
    }
    if (changes.isVerified !== undefined) {
      this.db
        .prepare('UPDATE accounts SET is_verified = ? WHERE id = ?')
        .run(changes.isVerified ? 1 : 0, id);
    }

    return this.getById(id);
  }

  /**
   * Change the password for an account.
   * All existing sessions are revoked.
   *
   * @returns true on success, false if oldPassword is wrong.
   */
  changePassword(
    id: string,
    oldPlainPassword: string,
    newPlainPassword: string
  ): boolean {
    const row = this.db
      .prepare<string, AccountRow>('SELECT * FROM accounts WHERE id = ?')
      .get(id);
    if (!row) return false;
    if (!verifyPassword(oldPlainPassword, row.password_hash)) return false;

    const newHash = hashPassword(newPlainPassword);
    this.db
      .prepare('UPDATE accounts SET password_hash = ? WHERE id = ?')
      .run(newHash, id);
    this.revokeAllSessions(id);
    return true;
  }

  // -------------------------------------------------------------------------
  // Identity linking
  // -------------------------------------------------------------------------

  /**
   * Link an existing anonymous identity token to this account.
   * This allows pre-registration history (friends, sessions, etc.) to be
   * associated with the account.
   *
   * @returns true if linked, false if the account does not exist.
   */
  linkIdentity(accountId: string, identityToken: string): boolean {
    const row = this.db
      .prepare<string, AccountRow>('SELECT id FROM accounts WHERE id = ?')
      .get(accountId);
    if (!row) return false;

    this.db
      .prepare(
        'UPDATE accounts SET linked_identity_token = ? WHERE id = ?'
      )
      .run(identityToken, accountId);
    return true;
  }
}
