/**
 * Compatibility Page — Phase 28
 *
 * Shows per-game compatibility badges, known issues, and the smoke-test matrix.
 *
 * Tabs:
 *   1. Games         — compatibility badges by system
 *   2. Known Issues  — open/investigating issues with severity
 *   3. Smoke Tests   — pass/fail matrix per system×backend×game
 */

import { useState, useEffect } from 'react';
import {
  fetchAllCompatibility,
  fetchSmokeTests,
  COMPAT_BADGE,
  SEVERITY_COLOR,
  type CompatibilitySummary,
  type SmokeTestEntry,
  type KnownIssue,
  type IssueSeverity,
} from '../lib/compatibility-service';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const SMOKE_ICONS: Record<string, string> = {
  pass: '✅',
  fail: '❌',
  skip: '⏭',
  pending: '⏳',
};

function SystemFilter({
  systems,
  active,
  onChange,
}: {
  systems: string[];
  active: string;
  onChange: (s: string) => void;
}) {
  return (
    <div className="flex flex-wrap gap-2 mb-4">
      {['all', ...systems].map((s) => (
        <button
          key={s}
          onClick={() => onChange(s)}
          className="px-3 py-1 rounded-full text-sm font-medium transition-colors"
          style={{
            background: active === s ? 'var(--color-oasis-accent)' : 'var(--color-oasis-card)',
            color: active === s ? '#fff' : 'var(--color-oasis-text)',
            border: '1px solid var(--color-oasis-border)',
          }}
        >
          {s === 'all' ? 'All Systems' : s.toUpperCase()}
        </button>
      ))}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------

function GamesTab() {
  const [summaries, setSummaries] = useState<CompatibilitySummary[]>([]);
  const [loading, setLoading] = useState(true);
  const [filter, setFilter] = useState('all');

  useEffect(() => {
    fetchAllCompatibility()
      .then((d) => setSummaries(d.summaries))
      .catch(() => setSummaries([]))
      .finally(() => setLoading(false));
  }, []);

  const systems = [...new Set(summaries.flatMap((s) => s.records.map((r) => r.system)))].sort();
  const visible = filter === 'all'
    ? summaries
    : summaries.filter((s) => s.records.some((r) => r.system === filter));

  if (loading) {
    return <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>Loading…</p>;
  }

  return (
    <div>
      <SystemFilter systems={systems} active={filter} onChange={setFilter} />
      <div className="grid gap-3">
        {visible.map((summary) => {
          const badge = COMPAT_BADGE[summary.bestStatus];
          return (
            <div
              key={summary.gameId}
              className="flex items-center justify-between p-3 rounded-lg"
              style={{ background: 'var(--color-oasis-card)', border: '1px solid var(--color-oasis-border)' }}
            >
              <div>
                <p className="font-medium" style={{ color: 'var(--color-oasis-text)' }}>
                  {summary.gameId}
                </p>
                <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {summary.records.map((r) => r.backend).join(', ')}
                </p>
              </div>
              <span
                className="flex items-center gap-1.5 px-3 py-1 rounded-full text-sm font-bold"
                style={{ background: `${badge.color}22`, color: badge.color, border: `1px solid ${badge.color}55` }}
              >
                {badge.icon} {badge.label}
              </span>
            </div>
          );
        })}
        {visible.length === 0 && (
          <p className="text-center py-8" style={{ color: 'var(--color-oasis-text-muted)' }}>No data for this system yet.</p>
        )}
      </div>
    </div>
  );
}

function KnownIssuesTab() {
  const [issues, setIssues] = useState<KnownIssue[]>([]);
  const [loading, setLoading] = useState(true);
  const [severityFilter, setSeverityFilter] = useState<IssueSeverity | 'all'>('all');

  useEffect(() => {
    // Load open issues from all games via the /api/known-issues endpoint
    const BACKEND_URL: string =
      (import.meta.env?.VITE_BACKEND_URL as string | undefined) ?? '';
    fetch(`${BACKEND_URL}/api/known-issues`)
      .then((r) => r.json())
      .then((d: { issues: KnownIssue[] }) => setIssues(d.issues.filter((i) => i.status !== 'resolved')))
      .catch(() => setIssues([]))
      .finally(() => setLoading(false));
  }, []);

  const SEVERITIES: Array<IssueSeverity | 'all'> = ['all', 'critical', 'high', 'medium', 'low'];
  const visible = severityFilter === 'all' ? issues : issues.filter((i) => i.severity === severityFilter);

  if (loading) {
    return <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>Loading…</p>;
  }

  return (
    <div>
      <div className="flex flex-wrap gap-2 mb-4">
        {SEVERITIES.map((s) => (
          <button
            key={s}
            onClick={() => setSeverityFilter(s)}
            className="px-3 py-1 rounded-full text-sm font-medium"
            style={{
              background: severityFilter === s ? 'var(--color-oasis-accent)' : 'var(--color-oasis-card)',
              color: severityFilter === s ? '#fff' : 'var(--color-oasis-text)',
              border: '1px solid var(--color-oasis-border)',
            }}
          >
            {s === 'all' ? 'All' : s.charAt(0).toUpperCase() + s.slice(1)}
          </button>
        ))}
      </div>
      <div className="flex flex-col gap-3">
        {visible.map((issue) => (
          <div
            key={issue.id}
            className="p-4 rounded-lg"
            style={{ background: 'var(--color-oasis-card)', border: '1px solid var(--color-oasis-border)' }}
          >
            <div className="flex items-start justify-between gap-3">
              <div className="flex-1 min-w-0">
                <p className="font-semibold" style={{ color: 'var(--color-oasis-text)' }}>{issue.title}</p>
                <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {issue.gameId} · {issue.system.toUpperCase()}{issue.backend ? ` · ${issue.backend}` : ''}
                </p>
                <p className="text-sm mt-2" style={{ color: 'var(--color-oasis-text-muted)' }}>{issue.description}</p>
              </div>
              <div className="flex flex-col items-end gap-1 shrink-0">
                <span
                  className="px-2 py-0.5 rounded-full text-xs font-bold uppercase"
                  style={{ background: `${SEVERITY_COLOR[issue.severity]}33`, color: SEVERITY_COLOR[issue.severity] }}
                >
                  {issue.severity}
                </span>
                <span
                  className="px-2 py-0.5 rounded-full text-xs"
                  style={{ background: 'var(--color-oasis-border)', color: 'var(--color-oasis-text-muted)' }}
                >
                  {issue.status}
                </span>
              </div>
            </div>
          </div>
        ))}
        {visible.length === 0 && (
          <div className="text-center py-12">
            <p className="text-4xl mb-3">✅</p>
            <p className="font-bold" style={{ color: 'var(--color-oasis-text)' }}>No open issues!</p>
            <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              All known issues are resolved or none have been reported yet.
            </p>
          </div>
        )}
      </div>
    </div>
  );
}

function SmokeTestsTab() {
  const [matrix, setMatrix] = useState<SmokeTestEntry[]>([]);
  const [loading, setLoading] = useState(true);
  const [sysFilter, setSysFilter] = useState('all');

  useEffect(() => {
    fetchSmokeTests()
      .then((d) => setMatrix(d.matrix))
      .catch(() => setMatrix([]))
      .finally(() => setLoading(false));
  }, []);

  const systems = [...new Set(matrix.map((e) => e.system))].sort();
  const visible = sysFilter === 'all' ? matrix : matrix.filter((e) => e.system === sysFilter);

  if (loading) {
    return <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>Loading…</p>;
  }

  const passPct = matrix.length > 0
    ? Math.round((matrix.filter((e) => e.status === 'pass').length / matrix.length) * 100)
    : 0;

  return (
    <div>
      <div
        className="flex items-center gap-4 p-4 rounded-lg mb-4"
        style={{ background: 'var(--color-oasis-card)', border: '1px solid var(--color-oasis-border)' }}
      >
        <div className="text-4xl font-black" style={{ color: '#22c55e' }}>{passPct}%</div>
        <div>
          <p className="font-semibold" style={{ color: 'var(--color-oasis-text)' }}>Smoke Tests Passing</p>
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {matrix.filter((e) => e.status === 'pass').length} / {matrix.length} games across {systems.length} systems
          </p>
        </div>
      </div>

      <SystemFilter systems={systems} active={sysFilter} onChange={setSysFilter} />

      <div className="flex flex-col gap-2">
        {visible.map((entry) => (
          <div
            key={entry.gameId}
            className="flex items-center justify-between px-4 py-2 rounded-lg"
            style={{ background: 'var(--color-oasis-card)', border: '1px solid var(--color-oasis-border)' }}
          >
            <div>
              <p className="text-sm font-medium" style={{ color: 'var(--color-oasis-text)' }}>
                {entry.gameId}
              </p>
              <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                {entry.system.toUpperCase()} · {entry.backend}
              </p>
              {entry.notes && (
                <p className="text-xs mt-0.5 italic" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {entry.notes}
                </p>
              )}
            </div>
            <span className="text-xl">{SMOKE_ICONS[entry.status]}</span>
          </div>
        ))}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Main page
// ---------------------------------------------------------------------------

type Tab = 'games' | 'issues' | 'smoke';

export default function CompatibilityPage() {
  const [tab, setTab] = useState<Tab>('games');

  const TABS: { id: Tab; label: string; icon: string }[] = [
    { id: 'games',  label: 'Compatibility',  icon: '🎮' },
    { id: 'issues', label: 'Known Issues',   icon: '⚠️' },
    { id: 'smoke',  label: 'Smoke Tests',    icon: '🧪' },
  ];

  return (
    <div className="flex flex-col gap-6 p-6 max-w-4xl mx-auto">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-bold" style={{ color: 'var(--color-oasis-text)' }}>
          🛡️ Compatibility & Stability
        </h1>
        <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Per-game compatibility badges, known issues, and automated smoke-test results.
        </p>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'var(--color-oasis-border)' }}>
        {TABS.map((t) => (
          <button
            key={t.id}
            onClick={() => setTab(t.id)}
            className="flex items-center gap-1.5 px-4 py-2 text-sm font-medium border-b-2 transition-colors"
            style={{
              borderColor: tab === t.id ? 'var(--color-oasis-accent)' : 'transparent',
              color: tab === t.id ? 'var(--color-oasis-accent)' : 'var(--color-oasis-text-muted)',
            }}
          >
            {t.icon} {t.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {tab === 'games'  && <GamesTab />}
      {tab === 'issues' && <KnownIssuesTab />}
      {tab === 'smoke'  && <SmokeTestsTab />}
    </div>
  );
}
