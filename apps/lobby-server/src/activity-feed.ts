/**
 * Community activity feed.
 *
 * Records notable events across the platform — sessions started, achievements
 * unlocked, tournaments created/won, reviews submitted, friends added — and
 * exposes a chronological feed for the Community Hub page.
 *
 * Pure in-memory (ring buffer capped at MAX_EVENTS). Not persisted across
 * restarts; the feed is intentionally ephemeral (live colour).
 */

import { randomUUID } from 'crypto';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type ActivityEventType =
  | 'session-started'
  | 'session-ended'
  | 'achievement-unlocked'
  | 'tournament-created'
  | 'tournament-won'
  | 'review-submitted'
  | 'friend-added';

export interface ActivityEvent {
  id: string;
  type: ActivityEventType;
  playerName: string;
  /** Human-readable description surfaced in the feed card. */
  description: string;
  /** Optional secondary entity (game title, achievement name, tournament name, …). */
  subject?: string;
  /** ISO timestamp. */
  createdAt: string;
  /** Additional metadata for rendering (icon, href, …). */
  meta?: Record<string, string | number>;
}

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

const MAX_EVENTS = 200;

export class ActivityFeedStore {
  private events: ActivityEvent[] = [];

  record(
    type: ActivityEventType,
    playerName: string,
    description: string,
    subject?: string,
    meta?: Record<string, string | number>,
  ): ActivityEvent {
    const event: ActivityEvent = {
      id: randomUUID(),
      type,
      playerName,
      description,
      subject,
      meta,
      createdAt: new Date().toISOString(),
    };
    this.events.unshift(event);
    // Cap ring buffer
    if (this.events.length > MAX_EVENTS) {
      this.events = this.events.slice(0, MAX_EVENTS);
    }
    return event;
  }

  /** Return the N most recent events, optionally filtered by type or player. */
  getRecent(
    limit = 50,
    opts: { type?: ActivityEventType; playerName?: string } = {},
  ): ActivityEvent[] {
    let results = this.events;
    if (opts.type) results = results.filter(e => e.type === opts.type);
    if (opts.playerName)
      results = results.filter(
        e => e.playerName.toLowerCase() === opts.playerName!.toLowerCase(),
      );
    return results.slice(0, limit);
  }

  count(): number {
    return this.events.length;
  }
}

// ---------------------------------------------------------------------------
// Singleton — module-level so index.ts can import it directly
// ---------------------------------------------------------------------------

export const activityFeed = new ActivityFeedStore();

// ---------------------------------------------------------------------------
// Helper functions used from index.ts / handler.ts
// ---------------------------------------------------------------------------

export function recordSessionStarted(playerName: string, gameTitle: string): void {
  activityFeed.record(
    'session-started',
    playerName,
    `${playerName} started a ${gameTitle} session`,
    gameTitle,
    { icon: '🎮' },
  );
}

export function recordSessionEnded(playerName: string, gameTitle: string, durationSecs: number): void {
  const mins = Math.round(durationSecs / 60);
  activityFeed.record(
    'session-ended',
    playerName,
    `${playerName} finished a ${gameTitle} session (${mins}m)`,
    gameTitle,
    { icon: '✅', durationSecs },
  );
}

export function recordAchievementUnlocked(playerName: string, achievementName: string): void {
  activityFeed.record(
    'achievement-unlocked',
    playerName,
    `${playerName} unlocked "${achievementName}"`,
    achievementName,
    { icon: '🏆' },
  );
}

export function recordTournamentCreated(playerName: string, tournamentName: string): void {
  activityFeed.record(
    'tournament-created',
    playerName,
    `${playerName} created tournament "${tournamentName}"`,
    tournamentName,
    { icon: '🎪' },
  );
}

export function recordTournamentWon(playerName: string, tournamentName: string): void {
  activityFeed.record(
    'tournament-won',
    playerName,
    `${playerName} won tournament "${tournamentName}"!`,
    tournamentName,
    { icon: '👑' },
  );
}

export function recordReviewSubmitted(playerName: string, gameTitle: string, rating: number): void {
  const stars = '⭐'.repeat(rating);
  activityFeed.record(
    'review-submitted',
    playerName,
    `${playerName} rated ${gameTitle} ${stars}`,
    gameTitle,
    { icon: '⭐', rating },
  );
}

export function recordFriendAdded(playerName: string, friendName: string): void {
  activityFeed.record(
    'friend-added',
    playerName,
    `${playerName} and ${friendName} are now friends`,
    friendName,
    { icon: '🤝' },
  );
}
