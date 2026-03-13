import { useState, useEffect, useCallback } from 'react';

// ---------------------------------------------------------------------------
// Types (mirror tournament-store.ts shape)
// ---------------------------------------------------------------------------

type MatchStatus = 'pending' | 'in-progress' | 'completed';
type TournamentStatus = 'pending' | 'active' | 'completed';

interface TournamentMatch {
  id: string;
  round: number;
  slot: number;
  playerA: string | null;
  playerB: string | null;
  winner: string | null;
  status: MatchStatus;
}

interface Tournament {
  id: string;
  name: string;
  gameId: string;
  gameTitle: string;
  system: string;
  players: string[];
  matches: TournamentMatch[];
  status: TournamentStatus;
  winner: string | null;
  createdAt: string;
  updatedAt: string;
}

// ---------------------------------------------------------------------------
// API helpers
// ---------------------------------------------------------------------------

const API = typeof import.meta !== 'undefined'
  ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ?? 'http://localhost:8080'
  : 'http://localhost:8080';

async function apiFetch<T>(path: string, opts?: RequestInit): Promise<T> {
  const res = await fetch(`${API}${path}`, {
    headers: { 'Content-Type': 'application/json' },
    ...opts,
  });
  if (!res.ok) {
    const err = await res.json().catch(() => ({ error: 'Unknown error' }));
    throw new Error((err as { error?: string }).error ?? 'Request failed');
  }
  return res.json() as Promise<T>;
}

// ---------------------------------------------------------------------------
// Sub-components
// ---------------------------------------------------------------------------

function SystemBadge({ system }: { system: string }) {
  return (
    <span
      className="text-[10px] font-black uppercase px-2 py-0.5 rounded-full"
      style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
    >
      {system}
    </span>
  );
}

function StatusBadge({ status }: { status: TournamentStatus }) {
  const map: Record<TournamentStatus, { label: string; color: string }> = {
    pending: { label: 'Pending', color: 'var(--color-oasis-text-muted)' },
    active: { label: 'Active', color: 'var(--color-oasis-green)' },
    completed: { label: 'Complete', color: 'var(--color-oasis-accent-light)' },
  };
  const { label, color } = map[status];
  return (
    <span className="text-[10px] font-bold" style={{ color }}>
      ● {label}
    </span>
  );
}

// ── Single-elimination bracket visualiser ────────────────────────────────────

function BracketView({
  tournament,
  onRecordResult,
}: {
  tournament: Tournament;
  onRecordResult: (match: TournamentMatch) => void;
}) {
  const maxRound = tournament.matches.length > 0
    ? Math.max(...tournament.matches.map((m) => m.round))
    : 0;
  const rounds = Array.from({ length: maxRound }, (_, i) => i + 1);

  const roundLabel = (r: number) => {
    if (r === maxRound) return 'Final';
    if (r === maxRound - 1) return 'Semi-finals';
    if (r === maxRound - 2) return 'Quarter-finals';
    return `Round ${r}`;
  };

  return (
    <div className="overflow-x-auto">
      <div className="flex gap-6 min-w-max pb-2">
        {rounds.map((r) => {
          const roundMatches = tournament.matches
            .filter((m) => m.round === r)
            .sort((a, b) => a.slot - b.slot);
          return (
            <div key={r} className="flex flex-col gap-3">
              <p
                className="text-[10px] font-black uppercase tracking-widest text-center mb-1"
                style={{ color: 'var(--color-oasis-text-muted)' }}
              >
                {roundLabel(r)}
              </p>
              {roundMatches.map((m) => (
                <MatchCard
                  key={m.id}
                  match={m}
                  canRecord={
                    tournament.status !== 'completed' &&
                    m.status !== 'completed' &&
                    m.playerA !== null &&
                    m.playerB !== null &&
                    m.playerA !== 'BYE' &&
                    m.playerB !== 'BYE'
                  }
                  onRecord={() => onRecordResult(m)}
                />
              ))}
            </div>
          );
        })}
      </div>
    </div>
  );
}

function MatchCard({
  match,
  canRecord,
  onRecord,
}: {
  match: TournamentMatch;
  canRecord: boolean;
  onRecord: () => void;
}) {
  const playerA = match.playerA ?? '—';
  const playerB = match.playerB ?? '—';

  function rowStyle(player: string | null) {
    const isWinner = match.winner !== null && match.winner === player;
    const isLoser = match.winner !== null && match.winner !== player && player !== null && player !== '—';
    return {
      color: isWinner
        ? 'var(--color-oasis-accent-light)'
        : isLoser
          ? 'var(--color-oasis-text-muted)'
          : 'var(--color-oasis-text)',
      fontWeight: isWinner ? 700 : 400,
      textDecoration: isLoser ? 'line-through' : 'none',
      opacity: player === 'BYE' ? 0.4 : 1,
    } as React.CSSProperties;
  }

  return (
    <div
      className="rounded-xl p-3 min-w-[160px]"
      style={{
        backgroundColor: 'var(--color-oasis-card)',
        border: `1px solid ${match.status === 'completed' ? 'transparent' : 'var(--n-border)'}`,
      }}
    >
      <div className="space-y-1.5">
        <p className="text-xs" style={rowStyle(match.playerA)}>
          {match.winner === playerA && '🏆 '}
          {playerA}
        </p>
        <div style={{ height: 1, backgroundColor: 'var(--n-border)' }} />
        <p className="text-xs" style={rowStyle(match.playerB)}>
          {match.winner === playerB && '🏆 '}
          {playerB}
        </p>
      </div>
      {canRecord && (
        <button
          onClick={onRecord}
          className="mt-2 w-full text-[10px] font-black py-1 rounded-lg transition-all hover:brightness-110"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
        >
          Report Result
        </button>
      )}
    </div>
  );
}

// ── Record result modal ───────────────────────────────────────────────────────

function RecordResultModal({
  match,
  onConfirm,
  onClose,
}: {
  match: TournamentMatch;
  onConfirm: (winner: string) => void;
  onClose: () => void;
}) {
  const players = [match.playerA, match.playerB].filter(
    (p): p is string => p !== null && p !== 'BYE'
  );
  return (
    <div
      className="fixed inset-0 flex items-center justify-center z-50"
      style={{ backgroundColor: 'rgba(0,0,0,0.6)' }}
      onClick={onClose}
    >
      <div
        className="rounded-2xl p-6 w-80"
        style={{ backgroundColor: 'var(--color-oasis-surface)' }}
        onClick={(e) => e.stopPropagation()}
      >
        <h3 className="text-base font-black mb-4" style={{ color: 'var(--color-oasis-accent-light)' }}>
          ⚔️ Record Match Result
        </h3>
        <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Select the winner
        </p>
        <div className="space-y-2">
          {players.map((p) => (
            <button
              key={p}
              onClick={() => onConfirm(p)}
              className="w-full py-2.5 rounded-xl text-sm font-bold transition-all hover:brightness-110"
              style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)' }}
            >
              🏆 {p}
            </button>
          ))}
        </div>
        <button
          onClick={onClose}
          className="mt-4 w-full py-2 text-xs font-bold rounded-xl"
          style={{ color: 'var(--color-oasis-text-muted)' }}
        >
          Cancel
        </button>
      </div>
    </div>
  );
}

// ── Create tournament form ────────────────────────────────────────────────────

function CreateTournamentForm({ onCreated }: { onCreated: (t: Tournament) => void }) {
  const [name, setName] = useState('');
  const [gameTitle, setGameTitle] = useState('');
  const [gameId, setGameId] = useState('');
  const [system, setSystem] = useState('NES');
  const [playersText, setPlayersText] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    setError(null);
    const players = playersText
      .split('\n')
      .map((p) => p.trim())
      .filter(Boolean);
    if (players.length < 2) {
      setError('Enter at least 2 player names (one per line).');
      return;
    }
    setLoading(true);
    try {
      const t = await apiFetch<Tournament>('/api/tournaments', {
        method: 'POST',
        body: JSON.stringify({ name, gameId: gameId || gameTitle, gameTitle, system, players }),
      });
      onCreated(t);
      setName('');
      setGameTitle('');
      setGameId('');
      setPlayersText('');
    } catch (err: unknown) {
      setError((err as Error).message);
    } finally {
      setLoading(false);
    }
  }

  const SYSTEMS = ['NES', 'SNES', 'GB', 'GBC', 'GBA', 'N64', 'NDS'];

  return (
    <form onSubmit={handleSubmit} className="space-y-3">
      <div>
        <label className="block text-[10px] font-black uppercase tracking-widest mb-1"
          style={{ color: 'var(--color-oasis-text-muted)' }}>Tournament Name</label>
        <input
          value={name}
          onChange={(e) => setName(e.target.value)}
          required
          placeholder="Summer Cup 2026"
          className="w-full rounded-xl px-3 py-2 text-sm"
          style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)', border: '1px solid var(--n-border)' }}
        />
      </div>
      <div className="flex gap-2">
        <div className="flex-1">
          <label className="block text-[10px] font-black uppercase tracking-widest mb-1"
            style={{ color: 'var(--color-oasis-text-muted)' }}>Game</label>
          <input
            value={gameTitle}
            onChange={(e) => setGameTitle(e.target.value)}
            required
            placeholder="Mario Kart 64"
            className="w-full rounded-xl px-3 py-2 text-sm"
            style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)', border: '1px solid var(--n-border)' }}
          />
        </div>
        <div>
          <label className="block text-[10px] font-black uppercase tracking-widest mb-1"
            style={{ color: 'var(--color-oasis-text-muted)' }}>System</label>
          <select
            value={system}
            onChange={(e) => setSystem(e.target.value)}
            className="rounded-xl px-3 py-2 text-sm"
            style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)', border: '1px solid var(--n-border)' }}
          >
            {SYSTEMS.map((s) => <option key={s} value={s}>{s}</option>)}
          </select>
        </div>
      </div>
      <div>
        <label className="block text-[10px] font-black uppercase tracking-widest mb-1"
          style={{ color: 'var(--color-oasis-text-muted)' }}>Players (one per line, min 2)</label>
        <textarea
          value={playersText}
          onChange={(e) => setPlayersText(e.target.value)}
          required
          rows={5}
          placeholder={"Alice\nBob\nCarlos\nDiana"}
          className="w-full rounded-xl px-3 py-2 text-sm font-mono resize-y"
          style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)', border: '1px solid var(--n-border)' }}
        />
      </div>
      {error && (
        <p className="text-xs" style={{ color: 'var(--color-oasis-accent)' }}>{error}</p>
      )}
      <button
        type="submit"
        disabled={loading}
        className="w-full py-2.5 rounded-xl text-sm font-black transition-all hover:brightness-110 disabled:opacity-50"
        style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
      >
        {loading ? 'Creating…' : '🏆 Create Bracket'}
      </button>
    </form>
  );
}

// ── Tournament card (list view) ───────────────────────────────────────────────

function TournamentCard({
  tournament,
  onSelect,
}: {
  tournament: Tournament;
  onSelect: () => void;
}) {
  return (
    <button
      onClick={onSelect}
      className="w-full text-left rounded-2xl p-4 transition-all hover:brightness-110"
      style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
    >
      <div className="flex items-start justify-between gap-2">
        <div className="flex-1 min-w-0">
          <p className="text-sm font-black truncate" style={{ color: 'var(--color-oasis-text)' }}>
            {tournament.name}
          </p>
          <p className="text-xs mt-0.5 truncate" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {tournament.gameTitle} · {tournament.players.length} players
          </p>
          {tournament.winner && (
            <p className="text-xs mt-1 font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
              🏆 Winner: {tournament.winner}
            </p>
          )}
        </div>
        <div className="flex flex-col items-end gap-1">
          <SystemBadge system={tournament.system} />
          <StatusBadge status={tournament.status} />
        </div>
      </div>
    </button>
  );
}

// ── Standings panel ───────────────────────────────────────────────────────────

function StandingsPanel({ tournament }: { tournament: Tournament }) {
  // Count wins per player
  const wins = new Map<string, number>();
  for (const m of tournament.matches) {
    if (m.winner && m.winner !== 'BYE') {
      wins.set(m.winner, (wins.get(m.winner) ?? 0) + 1);
    }
  }
  const standings = [...wins.entries()]
    .sort((a, b) => b[1] - a[1])
    .map(([name, w], i) => ({ rank: i + 1, name, wins: w }));

  if (standings.length === 0) return null;

  return (
    <div className="mt-4 rounded-2xl p-4" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
      <p className="text-[10px] font-black uppercase tracking-widest mb-3"
        style={{ color: 'var(--color-oasis-text-muted)' }}>Standings</p>
      <div className="space-y-1.5">
        {standings.map((s) => (
          <div key={s.name} className="flex items-center gap-2">
            <span className="text-xs font-black w-5 text-right"
              style={{ color: 'var(--color-oasis-text-muted)' }}>
              {s.rank}.
            </span>
            <span className="flex-1 text-xs font-semibold" style={{ color: 'var(--color-oasis-text)' }}>
              {s.rank === 1 && tournament.status === 'completed' ? '🏆 ' : ''}
              {s.name}
            </span>
            <span className="text-xs" style={{ color: 'var(--color-oasis-accent-light)' }}>
              {s.wins}W
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------

export function TournamentsPage() {
  const [tournaments, setTournaments] = useState<Tournament[]>([]);
  const [selected, setSelected] = useState<Tournament | null>(null);
  const [showCreate, setShowCreate] = useState(false);
  const [pendingMatch, setPendingMatch] = useState<TournamentMatch | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const loadTournaments = useCallback(async () => {
    setLoading(true);
    try {
      const list = await apiFetch<Tournament[]>('/api/tournaments');
      setTournaments(list);
      // Refresh selected tournament data
      if (selected) {
        const refreshed = list.find((t) => t.id === selected.id);
        if (refreshed) setSelected(refreshed);
      }
    } catch (err: unknown) {
      setError((err as Error).message);
    } finally {
      setLoading(false);
    }
  }, [selected]);

  useEffect(() => {
    void loadTournaments();
  }, []);

  // Listen for tournament-updated WS messages
  useEffect(() => {
    const wsUrl = (typeof import.meta !== 'undefined'
      ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ?? 'ws://localhost:8080'
      : 'ws://localhost:8080'
    ).replace(/^http/, 'ws');
    let ws: WebSocket | null = null;
    try {
      ws = new WebSocket(wsUrl);
      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data as string) as { type: string; tournamentId?: string };
          if (msg.type === 'tournament-updated') {
            void loadTournaments();
          }
        } catch { /* ignore */ }
      };
    } catch { /* non-critical */ }
    return () => ws?.close();
  }, [loadTournaments]);

  async function handleRecordResult(winner: string) {
    if (!selected || !pendingMatch) return;
    try {
      const updated = await apiFetch<Tournament>(
        `/api/tournaments/${selected.id}/matches/${pendingMatch.id}/result`,
        { method: 'POST', body: JSON.stringify({ winner }) }
      );
      setSelected(updated);
      setTournaments((prev) => prev.map((t) => (t.id === updated.id ? updated : t)));
    } catch (err: unknown) {
      setError((err as Error).message);
    } finally {
      setPendingMatch(null);
    }
  }

  function handleCreated(t: Tournament) {
    setTournaments((prev) => [t, ...prev]);
    setSelected(t);
    setShowCreate(false);
  }

  return (
    <div className="max-w-5xl mx-auto space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-black" style={{ color: 'var(--color-oasis-accent-light)' }}>
            🏆 Tournaments
          </h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Single-elimination brackets for any game
          </p>
        </div>
        <button
          onClick={() => { setShowCreate((v) => !v); setSelected(null); }}
          className="px-4 py-2 rounded-xl text-sm font-black transition-all hover:brightness-110"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
        >
          {showCreate ? '✕ Cancel' : '+ New Bracket'}
        </button>
      </div>

      {error && (
        <div className="rounded-xl p-3 text-sm" style={{ backgroundColor: 'rgba(230,0,18,0.15)', color: 'var(--color-oasis-accent)' }}>
          {error}
          <button onClick={() => setError(null)} className="ml-2 font-bold">✕</button>
        </div>
      )}

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Left: Create form or tournament list */}
        <div className="lg:col-span-1 space-y-3">
          {showCreate ? (
            <div className="rounded-2xl p-5" style={{ backgroundColor: 'var(--color-oasis-surface)', border: '1px solid var(--n-border)' }}>
              <p className="text-sm font-black mb-4" style={{ color: 'var(--color-oasis-accent-light)' }}>
                ✏️ New Tournament
              </p>
              <CreateTournamentForm onCreated={handleCreated} />
            </div>
          ) : (
            <>
              <p className="text-[10px] font-black uppercase tracking-widest"
                style={{ color: 'var(--color-oasis-text-muted)' }}>
                {loading ? 'Loading…' : `${tournaments.length} tournament${tournaments.length !== 1 ? 's' : ''}`}
              </p>
              {tournaments.length === 0 && !loading && (
                <p className="text-sm py-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  No tournaments yet. Create the first one!
                </p>
              )}
              {tournaments.map((t) => (
                <TournamentCard
                  key={t.id}
                  tournament={t}
                  onSelect={() => { setSelected(t); setShowCreate(false); }}
                />
              ))}
            </>
          )}
        </div>

        {/* Right: Bracket detail */}
        <div className="lg:col-span-2">
          {selected ? (
            <div className="rounded-2xl p-5" style={{ backgroundColor: 'var(--color-oasis-surface)', border: '1px solid var(--n-border)' }}>
              <div className="flex items-start justify-between mb-4">
                <div>
                  <h2 className="text-lg font-black" style={{ color: 'var(--color-oasis-accent-light)' }}>
                    {selected.name}
                  </h2>
                  <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    {selected.gameTitle} · {selected.system} · {selected.players.length} players
                  </p>
                </div>
                <div className="flex items-center gap-2">
                  <SystemBadge system={selected.system} />
                  <StatusBadge status={selected.status} />
                </div>
              </div>

              {selected.winner && (
                <div
                  className="rounded-xl p-3 mb-4 text-center"
                  style={{ backgroundColor: 'rgba(230,0,18,0.1)', border: '1px solid var(--color-oasis-accent)' }}
                >
                  <p className="text-lg font-black" style={{ color: 'var(--color-oasis-accent-light)' }}>
                    🏆 {selected.winner} wins the tournament!
                  </p>
                </div>
              )}

              <BracketView
                tournament={selected}
                onRecordResult={(m) => setPendingMatch(m)}
              />

              <StandingsPanel tournament={selected} />
            </div>
          ) : (
            <div className="h-full flex items-center justify-center py-20">
              <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Select a tournament to view its bracket
              </p>
            </div>
          )}
        </div>
      </div>

      {pendingMatch && (
        <RecordResultModal
          match={pendingMatch}
          onConfirm={handleRecordResult}
          onClose={() => setPendingMatch(null)}
        />
      )}
    </div>
  );
}
