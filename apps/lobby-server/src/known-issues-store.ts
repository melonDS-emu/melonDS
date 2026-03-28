/**
 * Known issues per game — Phase 28
 *
 * Tracks known compatibility/stability issues for specific games.
 * Issues can be associated with a particular backend and have severity
 * levels and lifecycle statuses.
 */

import { randomUUID } from 'crypto';

export type IssueSeverity = 'low' | 'medium' | 'high' | 'critical';
export type IssueStatus = 'open' | 'investigating' | 'resolved';

export interface KnownIssue {
  id: string;
  gameId: string;
  system: string;
  /** null means "affects all backends" */
  backend: string | null;
  title: string;
  description: string;
  severity: IssueSeverity;
  status: IssueStatus;
  reportedAt: string;  // ISO timestamp
  reportedBy: string;  // display name or 'system'
  resolvedAt?: string; // ISO timestamp, present when status === 'resolved'
}

export type NewIssueInput = Omit<KnownIssue, 'id' | 'reportedAt' | 'status' | 'resolvedAt'>;

// ---------------------------------------------------------------------------
// Pre-seeded known issues (realistic examples)
// ---------------------------------------------------------------------------

const SEED_ISSUES: Omit<KnownIssue, 'id' | 'reportedAt'>[] = [
  {
    gameId: 'n64-goldeneye-007',
    system: 'n64',
    backend: 'mupen64plus',
    title: 'Occasional desyncs at high player counts',
    description: 'With 4 players, desyncs can occur after ~20 minutes. Workaround: lower emulation speed to 90%.',
    severity: 'medium',
    status: 'investigating',
    reportedBy: 'system',
  },
  {
    gameId: 'gba-four-swords',
    system: 'gba',
    backend: 'mgba',
    title: 'Link cable emulation requires same mGBA version',
    description: 'Both players must run the same mGBA version for link cable emulation to work reliably.',
    severity: 'low',
    status: 'open',
    reportedBy: 'system',
  },
  {
    gameId: 'nds-pokemon-black',
    system: 'nds',
    backend: 'melonds',
    title: 'WFC connection intermittently drops',
    description: 'Intermittent disconnect when entering the Global Trade System. Save before trading.',
    severity: 'medium',
    status: 'open',
    reportedBy: 'system',
  },
  {
    gameId: 'gc-super-smash-bros-melee',
    system: 'gc',
    backend: 'dolphin',
    title: 'Rollback netcode not available',
    description: 'Dolphin uses delay-based netcode. Expect ~2 frame input lag at 50ms RTT.',
    severity: 'low',
    status: 'open',
    reportedBy: 'system',
  },
  {
    gameId: 'wii-super-smash-bros-brawl',
    system: 'wii',
    backend: 'dolphin',
    title: 'Online mode requires Wiimmfi patch',
    description: 'Nintendo WFC is offline. Apply the Wiimmfi DNS patch via WFC Config (see /wfc).',
    severity: 'high',
    status: 'open',
    reportedBy: 'system',
  },
];

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

export class KnownIssuesStore {
  private readonly issues = new Map<string, KnownIssue>();

  constructor() {
    this._seed();
  }

  private _seed(): void {
    const now = new Date().toISOString();
    for (const s of SEED_ISSUES) {
      const issue: KnownIssue = { ...s, id: randomUUID(), reportedAt: now };
      this.issues.set(issue.id, issue);
    }
  }

  /** Report a new issue. Returns the created record. */
  report(input: NewIssueInput): KnownIssue {
    const issue: KnownIssue = {
      ...input,
      id: randomUUID(),
      status: 'open',
      reportedAt: new Date().toISOString(),
    };
    this.issues.set(issue.id, issue);
    return issue;
  }

  /** Get all issues for a specific game. */
  getByGameId(gameId: string): KnownIssue[] {
    return [...this.issues.values()].filter((i) => i.gameId === gameId);
  }

  /** Get all open (non-resolved) issues. */
  getOpen(): KnownIssue[] {
    return [...this.issues.values()].filter((i) => i.status !== 'resolved');
  }

  /** Get all issues. */
  getAll(): KnownIssue[] {
    return [...this.issues.values()];
  }

  /** Get a specific issue by id. */
  getById(id: string): KnownIssue | undefined {
    return this.issues.get(id);
  }

  /** Update issue status. */
  updateStatus(id: string, status: IssueStatus): KnownIssue | null {
    const issue = this.issues.get(id);
    if (!issue) return null;
    const updated: KnownIssue = {
      ...issue,
      status,
      resolvedAt: status === 'resolved' ? new Date().toISOString() : issue.resolvedAt,
    };
    this.issues.set(id, updated);
    return updated;
  }
}
