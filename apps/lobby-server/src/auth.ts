/**
 * Auth/identity utilities for the lobby server.
 *
 * Responsibilities:
 *  1. Display name validation — ensures names are safe, printable, and within length limits.
 *  2. Room ownership tokens — a server-generated secret given only to the room creator.
 *     The creator must echo this token back when performing host-only actions (e.g.
 *     future reconnect-and-reclaim-host flows). For in-session enforcement the server
 *     already tracks hostId by WebSocket identity, so the token is primarily useful for
 *     out-of-band host reclaim after a reconnect.
 */

import { createHmac, randomBytes } from 'crypto';

// ---------------------------------------------------------------------------
// Display name validation
// ---------------------------------------------------------------------------

/** Maximum display name length (characters). */
export const MAX_DISPLAY_NAME_LENGTH = 24;
/** Minimum display name length (characters). */
export const MIN_DISPLAY_NAME_LENGTH = 1;

/**
 * Strip leading/trailing whitespace and collapse internal runs to a single space.
 */
export function normalizeDisplayName(raw: string): string {
  return raw.trim().replace(/\s+/g, ' ');
}

/**
 * Validate a display name.
 * Returns `null` when the name is valid, or an error string otherwise.
 */
export function validateDisplayName(name: string): string | null {
  // Check for control characters in the raw input before normalization
  // eslint-disable-next-line no-control-regex
  if (/[\x00-\x1F\x7F]/.test(name)) {
    return 'Display name must not contain control characters.';
  }
  const normalized = normalizeDisplayName(name);
  if (normalized.length < MIN_DISPLAY_NAME_LENGTH) {
    return `Display name must be at least ${MIN_DISPLAY_NAME_LENGTH} character(s).`;
  }
  if (normalized.length > MAX_DISPLAY_NAME_LENGTH) {
    return `Display name must be at most ${MAX_DISPLAY_NAME_LENGTH} characters.`;
  }
  return null;
}

// ---------------------------------------------------------------------------
// Room ownership tokens
// ---------------------------------------------------------------------------

/** Server-side secret used to sign ownership tokens. Regenerated on each process start. */
const SIGNING_SECRET = randomBytes(32).toString('hex');

/**
 * Generate a room ownership token for the given room ID.
 * The token is an HMAC-SHA256 of the roomId (hex-encoded, first 32 chars for compactness).
 * It is deterministic per-process: the same roomId always yields the same token within
 * a single server session, which avoids the need to store the token separately.
 */
export function generateOwnerToken(roomId: string): string {
  return createHmac('sha256', SIGNING_SECRET).update(roomId).digest('hex').slice(0, 32);
}

/**
 * Verify that a token matches the expected ownership token for the given room ID.
 */
export function verifyOwnerToken(roomId: string, token: string): boolean {
  const expected = generateOwnerToken(roomId);
  // Constant-time comparison to prevent timing attacks
  if (expected.length !== token.length) return false;
  let diff = 0;
  for (let i = 0; i < expected.length; i++) {
    diff |= expected.charCodeAt(i) ^ token.charCodeAt(i);
  }
  return diff === 0;
}
