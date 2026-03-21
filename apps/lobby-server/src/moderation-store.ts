/**
 * Lightweight moderation and reporting system for public rooms.
 *
 * Players can report other players or public rooms for:
 *   - spam / flooding chat
 *   - harassment or abusive language
 *   - cheating / exploiting
 *   - offensive display names
 *   - other issues
 *
 * Reports are stored persistently and can be reviewed by moderators via
 * REST API.  A simple auto-flag threshold is applied: when a target
 * accumulates `AUTO_FLAG_THRESHOLD` pending reports it is flagged
 * automatically in the listing so human review can be prioritised.
 */

import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

export type ReportTargetType = 'player' | 'room';
export type ReportReason =
  | 'spam'
  | 'harassment'
  | 'cheating'
  | 'offensive-name'
  | 'other';
export type ReportStatus = 'pending' | 'reviewed' | 'dismissed';

export interface Report {
  /** Stable UUID for this report. */
  id: string;
  /** Player ID of the person who filed the report. */
  reporterId: string;
  /** Display name of the reporter at the time of reporting. */
  reporterName: string;
  /** ID of the player or room being reported. */
  targetId: string;
  /** Whether the target is a player or a room. */
  targetType: ReportTargetType;
  /** Category of the report. */
  reason: ReportReason;
  /** Optional free-text description (max 1000 chars). */
  description: string;
  /** Current moderation status. */
  status: ReportStatus;
  /** ISO timestamp when the report was filed. */
  createdAt: string;
  /** ISO timestamp when a moderator reviewed the report, or null. */
  reviewedAt: string | null;
  /** Optional note left by the moderator when resolving. */
  resolvedNote: string | null;
}

// ---------------------------------------------------------------------------
// Internal DB row shape
// ---------------------------------------------------------------------------

interface ReportRow {
  id: string;
  reporter_id: string;
  reporter_name: string;
  target_id: string;
  target_type: string;
  reason: string;
  description: string;
  status: string;
  created_at: string;
  reviewed_at: string | null;
  resolved_note: string | null;
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/** Maximum length for the free-text description field. */
const MAX_DESCRIPTION_LENGTH = 1000;

/** Number of pending reports required to auto-flag a target. */
export const AUTO_FLAG_THRESHOLD = 3;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function rowToReport(row: ReportRow): Report {
  return {
    id: row.id,
    reporterId: row.reporter_id,
    reporterName: row.reporter_name,
    targetId: row.target_id,
    targetType: row.target_type as ReportTargetType,
    reason: row.reason as ReportReason,
    description: row.description,
    status: row.status as ReportStatus,
    createdAt: row.created_at,
    reviewedAt: row.reviewed_at,
    resolvedNote: row.resolved_note,
  };
}

// ---------------------------------------------------------------------------
// ModerationStore
// ---------------------------------------------------------------------------

export class ModerationStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
  }

  // -------------------------------------------------------------------------
  // Filing reports
  // -------------------------------------------------------------------------

  /**
   * File a new report.
   *
   * Duplicate reports from the same reporter against the same target are
   * allowed (they may describe different incidents).
   *
   * @returns The created Report.
   */
  report(
    reporterId: string,
    reporterName: string,
    targetId: string,
    targetType: ReportTargetType,
    reason: ReportReason,
    description = ''
  ): Report {
    const id = randomUUID();
    const now = new Date().toISOString();
    const truncated = description.slice(0, MAX_DESCRIPTION_LENGTH);

    this.db
      .prepare(
        `INSERT INTO moderation_reports
           (id, reporter_id, reporter_name, target_id, target_type,
            reason, description, status, created_at, reviewed_at, resolved_note)
         VALUES (?, ?, ?, ?, ?, ?, ?, 'pending', ?, NULL, NULL)`
      )
      .run(
        id,
        reporterId,
        reporterName,
        targetId,
        targetType,
        reason,
        truncated,
        now
      );

    return rowToReport(
      this.db
        .prepare<string, ReportRow>(
          'SELECT * FROM moderation_reports WHERE id = ?'
        )
        .get(id)!
    );
  }

  // -------------------------------------------------------------------------
  // Querying reports
  // -------------------------------------------------------------------------

  /**
   * List reports, optionally filtered by status.
   * Results are returned newest-first.
   */
  getReports(status?: ReportStatus): Report[] {
    if (status) {
      return this.db
        .prepare<string, ReportRow>(
          `SELECT * FROM moderation_reports
           WHERE status = ?
           ORDER BY created_at DESC`
        )
        .all(status)
        .map(rowToReport);
    }
    return this.db
      .prepare<[], ReportRow>(
        'SELECT * FROM moderation_reports ORDER BY created_at DESC'
      )
      .all()
      .map(rowToReport);
  }

  /**
   * Get all reports for a specific target (player or room).
   */
  getReportsByTarget(targetId: string): Report[] {
    return this.db
      .prepare<string, ReportRow>(
        `SELECT * FROM moderation_reports
         WHERE target_id = ?
         ORDER BY created_at DESC`
      )
      .all(targetId)
      .map(rowToReport);
  }

  /**
   * Get a single report by ID.
   */
  getReport(id: string): Report | null {
    const row = this.db
      .prepare<string, ReportRow>(
        'SELECT * FROM moderation_reports WHERE id = ?'
      )
      .get(id);
    return row ? rowToReport(row) : null;
  }

  /**
   * Count pending reports for a target.
   * Values ≥ AUTO_FLAG_THRESHOLD indicate the target should be prioritised
   * for human review.
   */
  getPendingCount(targetId: string): number {
    const row = this.db
      .prepare<string, { cnt: number }>(
        `SELECT COUNT(*) as cnt FROM moderation_reports
         WHERE target_id = ? AND status = 'pending'`
      )
      .get(targetId);
    return row?.cnt ?? 0;
  }

  /**
   * Returns true when the target has been reported enough times to warrant
   * automatic flagging.
   */
  isFlagged(targetId: string): boolean {
    return this.getPendingCount(targetId) >= AUTO_FLAG_THRESHOLD;
  }

  // -------------------------------------------------------------------------
  // Moderator actions
  // -------------------------------------------------------------------------

  /**
   * Mark a report as reviewed with an optional resolution note.
   *
   * @returns The updated Report, or null if not found.
   */
  reviewReport(id: string, note = ''): Report | null {
    const existing = this.db
      .prepare<string, ReportRow>(
        'SELECT * FROM moderation_reports WHERE id = ?'
      )
      .get(id);
    if (!existing) return null;

    const now = new Date().toISOString();
    this.db
      .prepare(
        `UPDATE moderation_reports
         SET status = 'reviewed', reviewed_at = ?, resolved_note = ?
         WHERE id = ?`
      )
      .run(now, note || null, id);

    return rowToReport(
      this.db
        .prepare<string, ReportRow>(
          'SELECT * FROM moderation_reports WHERE id = ?'
        )
        .get(id)!
    );
  }

  /**
   * Dismiss a report (mark as not actionable).
   *
   * @returns The updated Report, or null if not found.
   */
  dismissReport(id: string, note = ''): Report | null {
    const existing = this.db
      .prepare<string, ReportRow>(
        'SELECT * FROM moderation_reports WHERE id = ?'
      )
      .get(id);
    if (!existing) return null;

    const now = new Date().toISOString();
    this.db
      .prepare(
        `UPDATE moderation_reports
         SET status = 'dismissed', reviewed_at = ?, resolved_note = ?
         WHERE id = ?`
      )
      .run(now, note || null, id);

    return rowToReport(
      this.db
        .prepare<string, ReportRow>(
          'SELECT * FROM moderation_reports WHERE id = ?'
        )
        .get(id)!
    );
  }

  /**
   * Bulk-dismiss all pending reports against a target.
   * Useful when a reported name/room has been manually verified as harmless.
   */
  clearReports(targetId: string): number {
    const now = new Date().toISOString();
    const result = this.db
      .prepare(
        `UPDATE moderation_reports
         SET status = 'dismissed', reviewed_at = ?
         WHERE target_id = ? AND status = 'pending'`
      )
      .run(now, targetId);
    return result.changes;
  }
}
