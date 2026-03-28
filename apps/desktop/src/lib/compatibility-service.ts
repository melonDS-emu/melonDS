/**
 * Compatibility service — Phase 28 (desktop layer)
 *
 * Thin fetch wrapper around the lobby server's /api/compatibility and
 * /api/known-issues endpoints.
 */

const BACKEND_URL: string =
  (typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_BACKEND_URL?: string } }).env?.VITE_BACKEND_URL
    : undefined) ?? '';

export type CompatibilityStatus = 'verified' | 'playable' | 'partial' | 'broken' | 'untested';
export type IssueSeverity = 'low' | 'medium' | 'high' | 'critical';
export type IssueStatus = 'open' | 'investigating' | 'resolved';

export interface CompatibilityRecord {
  gameId: string;
  system: string;
  backend: string;
  status: CompatibilityStatus;
  notes?: string;
  testedAt: string;
  testedBy: string;
}

export interface CompatibilitySummary {
  gameId: string;
  bestStatus: CompatibilityStatus;
  records: CompatibilityRecord[];
}

export interface KnownIssue {
  id: string;
  gameId: string;
  system: string;
  backend: string | null;
  title: string;
  description: string;
  severity: IssueSeverity;
  status: IssueStatus;
  reportedAt: string;
  reportedBy: string;
  resolvedAt?: string;
}

export interface SmokeTestEntry {
  system: string;
  backend: string;
  gameId: string;
  status: 'pass' | 'fail' | 'skip' | 'pending';
  notes?: string;
}

// ---------------------------------------------------------------------------
// Badge display helpers
// ---------------------------------------------------------------------------

export const COMPAT_BADGE: Record<CompatibilityStatus, { label: string; color: string; icon: string }> = {
  verified:  { label: 'Verified',  color: '#22c55e', icon: '✅' },
  playable:  { label: 'Playable',  color: '#84cc16', icon: '🟢' },
  partial:   { label: 'Partial',   color: '#f59e0b', icon: '🟡' },
  broken:    { label: 'Broken',    color: '#ef4444', icon: '🔴' },
  untested:  { label: 'Untested',  color: '#6b7280', icon: '⚪' },
};

export const SEVERITY_COLOR: Record<IssueSeverity, string> = {
  low:      '#84cc16',
  medium:   '#f59e0b',
  high:     '#f97316',
  critical: '#ef4444',
};

// ---------------------------------------------------------------------------
// API calls
// ---------------------------------------------------------------------------

/** Get the compatibility summary for a specific game. */
export async function fetchCompatibility(gameId: string): Promise<CompatibilitySummary> {
  const res = await fetch(`${BACKEND_URL}/api/compatibility/${encodeURIComponent(gameId)}`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json() as Promise<CompatibilitySummary>;
}

/** Get all compatibility summaries. */
export async function fetchAllCompatibility(): Promise<{ summaries: CompatibilitySummary[] }> {
  const res = await fetch(`${BACKEND_URL}/api/compatibility`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json() as Promise<{ summaries: CompatibilitySummary[] }>;
}

/** Get known issues for a specific game. */
export async function fetchKnownIssues(gameId: string): Promise<{ issues: KnownIssue[] }> {
  const res = await fetch(`${BACKEND_URL}/api/known-issues/${encodeURIComponent(gameId)}`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json() as Promise<{ issues: KnownIssue[] }>;
}

/** Get the smoke-test matrix. */
export async function fetchSmokeTests(): Promise<{ matrix: SmokeTestEntry[] }> {
  const res = await fetch(`${BACKEND_URL}/api/smoke-tests`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json() as Promise<{ matrix: SmokeTestEntry[] }>;
}

/** Report a new known issue. */
export async function reportKnownIssue(
  issue: Pick<KnownIssue, 'gameId' | 'system' | 'backend' | 'title' | 'description' | 'severity' | 'reportedBy'>,
): Promise<{ issue: KnownIssue }> {
  const res = await fetch(`${BACKEND_URL}/api/known-issues`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(issue),
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json() as Promise<{ issue: KnownIssue }>;
}
