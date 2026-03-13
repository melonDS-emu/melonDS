import { useState, useEffect, useCallback } from 'react';
import { useLobby } from '../context/LobbyContext';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';
import type { CreateRoomPayload } from '../services/lobby-types';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

const MK_GAME_ID = 'nds-mario-kart-ds';
const MK_GAME_TITLE = 'Mario Kart DS';

type KartMode = 'quick' | 'public' | 'ranked';

interface KartRanking {
  displayName: string;
  wins: number;
  totalRaces: number;
  winRate: number;
}

// ---------------------------------------------------------------------------
// API helpers
// ---------------------------------------------------------------------------

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
// Rank badge helper
// ---------------------------------------------------------------------------

function RankBadge({ rank }: { rank: number }) {
  const medals: Record<number, string> = { 1: '🥇', 2: '🥈', 3: '🥉' };
  return (
    <span className="text-lg w-8 text-center" title={`Rank #${rank}`}>
      {medals[rank] ?? `#${rank}`}
    </span>
  );
}

// ---------------------------------------------------------------------------
// Quick Race panel
// ---------------------------------------------------------------------------

function QuickRacePanel({ displayName }: { displayName: string }) {
  const { publicRooms, createRoom, joinByCode, listRooms } = useLobby();
  const [queued, setQueued] = useState(false);
  const [queuePos, setQueuePos] = useState<number | null>(null);
  const [status, setStatus] = useState('');

  // Auto-refresh room list periodically while in queue
  useEffect(() => {
    if (!queued) return;
    const iv = setInterval(() => { listRooms(); }, 5000);
    return () => clearInterval(iv);
  }, [queued, listRooms]);

  // Watch for an available quick race room while queued
  useEffect(() => {
    if (!queued) return;
    const openRoom = publicRooms.find(
      (r) => r.gameId === MK_GAME_ID && r.maxPlayers === 2 && r.players.length < 2
    );
    if (openRoom) {
      joinByCode(openRoom.roomCode, displayName);
      setQueued(false);
      setStatus('');
    }
  }, [queued, publicRooms, joinByCode, displayName]);

  async function handleQuickRace() {
    if (!displayName) return;
    setStatus('Looking for an open room…');

    // First try to find an existing open 1v1 room
    const openRoom = publicRooms.find(
      (r) => r.gameId === MK_GAME_ID && r.maxPlayers === 2 && r.players.length < 2
    );
    if (openRoom) {
      joinByCode(openRoom.roomCode, displayName);
      setStatus('Joining existing room…');
      return;
    }

    // No room available — join the matchmaking queue
    try {
      const playerId = `${displayName}-${Date.now()}`;
      const result = await apiFetch<{ position: number }>('/api/matchmaking/join', {
        method: 'POST',
        body: JSON.stringify({
          playerId,
          displayName,
          gameId: MK_GAME_ID,
          gameTitle: MK_GAME_TITLE,
          system: 'nds',
          maxPlayers: 2,
        }),
      });
      setQueued(true);
      setQueuePos(result.position);
      setStatus(`In queue — position ${result.position}. Creating a room to host while you wait…`);

      // Host a room so the opponent can join us
      createRoom(
        {
          gameId: MK_GAME_ID,
          gameTitle: MK_GAME_TITLE,
          system: 'nds',
          maxPlayers: 2,
          isPublic: true,
          name: `${displayName}'s Quick Race`,
        },
        displayName
      );
    } catch (err) {
      setStatus(`❌ ${(err as Error).message}`);
    }
  }

  function handleCancel() {
    setQueued(false);
    setQueuePos(null);
    setStatus('');
  }

  return (
    <div className="space-y-5">
      {/* Explainer */}
      <div
        className="rounded-2xl p-4"
        style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
      >
        <p className="text-sm font-bold mb-1" style={{ color: 'var(--color-oasis-accent-light)' }}>
          ⚡ Quick Race
        </p>
        <p className="text-xs leading-relaxed" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Click once and RetroOasis finds you an opponent. If nobody is waiting, you join the
          matchmaking queue and host a room — the next player in queue is sent your room code
          automatically.
        </p>
      </div>

      {/* Quick Race button */}
      <div className="flex flex-col items-start gap-3">
        {!queued ? (
          <button
            onClick={() => void handleQuickRace()}
            disabled={!displayName}
            className="text-sm font-bold px-6 py-3 rounded-2xl transition-all flex items-center gap-2"
            style={{
              backgroundColor: 'var(--color-oasis-accent)',
              color: '#fff',
              opacity: displayName ? 1 : 0.5,
              fontSize: '1rem',
            }}
          >
            🏎️ Quick Race
          </button>
        ) : (
          <div className="flex items-center gap-3">
            <div
              className="w-4 h-4 rounded-full animate-pulse"
              style={{ backgroundColor: 'var(--color-oasis-accent)' }}
            />
            <span className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
              {queuePos !== null ? `Queue position: ${queuePos}` : 'Searching…'}
            </span>
            <button
              onClick={handleCancel}
              className="text-xs font-bold px-3 py-1 rounded-xl"
              style={{ backgroundColor: 'rgba(255,255,255,0.08)', color: 'var(--color-oasis-text-muted)' }}
            >
              Cancel
            </button>
          </div>
        )}
        {status && (
          <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {status}
          </p>
        )}
        {!displayName && (
          <p className="text-xs" style={{ color: 'var(--color-oasis-accent)' }}>
            Set a display name in Settings to race.
          </p>
        )}
      </div>

      {/* Open MK rooms */}
      {publicRooms.filter((r) => r.gameId === MK_GAME_ID).length > 0 && (
        <div>
          <p className="text-xs font-bold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Available right now
          </p>
          <div className="space-y-2">
            {publicRooms
              .filter((r) => r.gameId === MK_GAME_ID && r.players.length < r.maxPlayers)
              .slice(0, 3)
              .map((r) => {
                const host = r.players.find((p) => p.isHost);
                return (
                  <div
                    key={r.id}
                    className="rounded-xl px-4 py-2.5 flex items-center justify-between"
                    style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
                  >
                    <div>
                      <p className="text-xs font-bold" style={{ color: 'var(--color-oasis-text)' }}>
                        {host?.displayName ?? r.name}
                      </p>
                      <p className="text-[10px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
                        {r.players.length}/{r.maxPlayers} players
                      </p>
                    </div>
                    <button
                      onClick={() => { joinByCode(r.roomCode, displayName); }}
                      disabled={!displayName}
                      className="text-xs font-bold px-3 py-1 rounded-lg"
                      style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff', opacity: displayName ? 1 : 0.5 }}
                    >
                      Join
                    </button>
                  </div>
                );
              })}
          </div>
        </div>
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Public Rooms panel
// ---------------------------------------------------------------------------

function PublicRoomsPanel({ displayName }: { displayName: string }) {
  const { publicRooms, createRoom, joinByCode, listRooms } = useLobby();
  const [showHost, setShowHost] = useState(false);
  const [joinTarget, setJoinTarget] = useState<string | null>(null);

  useEffect(() => {
    listRooms();
  }, [listRooms]);

  const mkRooms = publicRooms.filter((r) => r.gameId === MK_GAME_ID);

  function handleHostConfirm(payload: Omit<CreateRoomPayload, 'displayName'>, dn: string) {
    createRoom(payload, dn);
    setShowHost(false);
  }

  return (
    <div className="space-y-5">
      {/* Header actions */}
      <div className="flex items-center gap-3">
        <button
          onClick={() => setShowHost(true)}
          disabled={!displayName}
          className="text-sm font-bold px-4 py-2 rounded-xl"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff', opacity: displayName ? 1 : 0.5 }}
        >
          🏁 Host Public Race
        </button>
        <button
          onClick={() => { listRooms(); }}
          className="text-xs font-bold px-3 py-2 rounded-xl"
          style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)' }}
        >
          ↻ Refresh
        </button>
      </div>

      {/* Room list */}
      {mkRooms.length === 0 ? (
        <div
          className="rounded-2xl p-6 text-center"
          style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
        >
          <p className="text-2xl mb-2">🏎️</p>
          <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
            No public races open
          </p>
          <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Host one and your room code will appear here for others to join.
          </p>
        </div>
      ) : (
        <div className="space-y-2">
          {mkRooms.map((r) => {
            const host = r.players.find((p) => p.isHost);
            const isFull = r.players.length >= r.maxPlayers;
            return (
              <div
                key={r.id}
                className="rounded-xl px-4 py-3 flex items-center justify-between"
                style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
              >
                <div>
                  <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
                    {r.name}
                  </p>
                  <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    {host ? `Hosted by ${host.displayName}` : ''} · {r.players.length}/{r.maxPlayers} players
                  </p>
                </div>
                <div className="flex items-center gap-2">
                  <span
                    className="text-[10px] font-bold px-2 py-0.5 rounded-full"
                    style={{
                      backgroundColor: isFull ? 'rgba(248,113,113,0.15)' : 'rgba(74,222,128,0.15)',
                      color: isFull ? '#f87171' : '#4ade80',
                    }}
                  >
                    {isFull ? 'Full' : 'Open'}
                  </span>
                  {!isFull && (
                    <button
                      onClick={() => setJoinTarget(r.roomCode)}
                      disabled={!displayName}
                      className="text-xs font-bold px-3 py-1 rounded-lg"
                      style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff', opacity: displayName ? 1 : 0.5 }}
                    >
                      Join
                    </button>
                  )}
                </div>
              </div>
            );
          })}
        </div>
      )}

      {showHost && (
        <HostRoomModal
          preselectedGameId={MK_GAME_ID}
          onConfirm={handleHostConfirm}
          onClose={() => setShowHost(false)}
        />
      )}
      {joinTarget && (
        <JoinRoomModal
          initialCode={joinTarget}
          onConfirm={(code, dn) => { joinByCode(code, dn); setJoinTarget(null); }}
          onSpectate={(code, dn) => { joinByCode(code, dn); setJoinTarget(null); }}
          onClose={() => setJoinTarget(null)}
        />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Ranked Races panel
// ---------------------------------------------------------------------------

function RankedRacesPanel({ displayName }: { displayName: string }) {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const [rankings, setRankings] = useState<KartRanking[]>([]);
  const [queued, setQueued] = useState(false);
  const [status, setStatus] = useState('');

  const fetchRankings = useCallback(async () => {
    try {
      // Rankings are based on total session count (proxy for race experience).
      // MK-specific win/loss tracking is a future enhancement.
      const board = await apiFetch<{ displayName: string; sessions: number }[]>(
        '/api/leaderboard?metric=sessions&limit=20'
      );
      const ranked: KartRanking[] = board.map((entry) => ({
        displayName: entry.displayName,
        wins: 0, // Not tracked yet — future: MK-specific win tracking
        totalRaces: entry.sessions,
        winRate: 0,
      }));
      setRankings(ranked.filter((r) => r.totalRaces > 0));
    } catch {
      // Leaderboard unavailable — show empty state
      setRankings([]);
    }
  }, []);

  useEffect(() => {
    void fetchRankings();
  }, [fetchRankings]);

  // Watch for a ranked room to appear while queued
  useEffect(() => {
    if (!queued) return;
    const openRoom = publicRooms.find(
      (r) => r.gameId === MK_GAME_ID && r.maxPlayers === 4 && r.players.length < 4
    );
    if (openRoom) {
      joinByCode(openRoom.roomCode, displayName);
      setQueued(false);
      setStatus('');
    }
  }, [queued, publicRooms, joinByCode, displayName]);

  async function handleJoinRanked() {
    if (!displayName) return;
    setStatus('Joining ranked queue…');
    try {
      const playerId = `${displayName}-ranked-${Date.now()}`;
      const result = await apiFetch<{ position: number }>('/api/matchmaking/join', {
        method: 'POST',
        body: JSON.stringify({
          playerId,
          displayName,
          gameId: MK_GAME_ID,
          gameTitle: MK_GAME_TITLE,
          system: 'nds',
          maxPlayers: 4,
        }),
      });
      setQueued(true);
      setStatus(`Ranked queue — position ${result.position}. Hosting a 4-player room…`);

      createRoom(
        {
          gameId: MK_GAME_ID,
          gameTitle: MK_GAME_TITLE,
          system: 'nds',
          maxPlayers: 4,
          isPublic: true,
          name: `${displayName}'s Ranked Race`,
        },
        displayName
      );
    } catch (err) {
      setStatus(`❌ ${(err as Error).message}`);
      setQueued(false);
    }
  }

  return (
    <div className="space-y-6">
      {/* Join ranked */}
      <div
        className="rounded-2xl p-5"
        style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
      >
        <h3 className="text-base font-bold mb-1" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🏆 Ranked Race
        </h3>
        <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Join a 4-player ranked race. RetroOasis matches you with players of similar activity
          level via the matchmaking queue. Results count toward the global leaderboard.
        </p>

        {!queued ? (
          <button
            onClick={() => void handleJoinRanked()}
            disabled={!displayName}
            className="text-sm font-bold px-5 py-2.5 rounded-xl"
            style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff', opacity: displayName ? 1 : 0.5 }}
          >
            🏆 Join Ranked Queue
          </button>
        ) : (
          <div className="flex items-center gap-3">
            <div
              className="w-3 h-3 rounded-full animate-pulse"
              style={{ backgroundColor: 'var(--color-oasis-accent)' }}
            />
            <span className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {status}
            </span>
            <button
              onClick={() => { setQueued(false); setStatus(''); }}
              className="text-xs font-bold px-3 py-1 rounded-xl"
              style={{ backgroundColor: 'rgba(255,255,255,0.08)', color: 'var(--color-oasis-text-muted)' }}
            >
              Cancel
            </button>
          </div>
        )}
        {!queued && status && (
          <p className="text-xs mt-2" style={{ color: 'var(--color-oasis-text-muted)' }}>{status}</p>
        )}
        {!displayName && (
          <p className="text-xs mt-2" style={{ color: 'var(--color-oasis-accent)' }}>
            Set a display name in Settings to race.
          </p>
        )}
      </div>

      {/* Leaderboard */}
      <div>
        <div className="flex items-center justify-between mb-3">
          <h3 className="text-base font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
            🏅 Leaderboard
          </h3>
          <button
            onClick={() => void fetchRankings()}
            className="text-xs font-bold px-2 py-1 rounded-lg"
            style={{ color: 'var(--color-oasis-text-muted)', backgroundColor: 'var(--color-oasis-card)' }}
          >
            ↻
          </button>
        </div>

        {rankings.length === 0 ? (
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No ranked data yet — race to get on the board!
          </p>
        ) : (
          <div className="space-y-2">
            {rankings.slice(0, 10).map((entry, i) => (
              <div
                key={entry.displayName}
                className="rounded-xl px-4 py-3 flex items-center gap-3"
                style={{
                  backgroundColor:
                    entry.displayName === displayName
                      ? 'rgba(230,0,18,0.1)'
                      : 'var(--color-oasis-card)',
                  border: entry.displayName === displayName
                    ? '1px solid rgba(230,0,18,0.3)'
                    : '1px solid var(--n-border)',
                }}
              >
                <RankBadge rank={i + 1} />
                <div className="flex-1 min-w-0">
                  <p className="text-sm font-bold truncate" style={{ color: 'var(--color-oasis-text)' }}>
                    {entry.displayName}
                    {entry.displayName === displayName && (
                      <span className="ml-2 text-[10px] font-black" style={{ color: 'var(--color-oasis-accent)' }}>
                        YOU
                      </span>
                    )}
                  </p>
                  <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    {entry.totalRaces} sessions
                  </p>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------

export function MarioKartPage() {
  const [activeMode, setActiveMode] = useState<KartMode>('quick');
  const [displayName, setDisplayName] = useState('');

  useEffect(() => {
    const stored = localStorage.getItem('retro-oasis-display-name');
    if (stored) setDisplayName(stored);
  }, []);

  const MODES: { id: KartMode; label: string; icon: string; description: string }[] = [
    {
      id: 'quick',
      label: 'Quick Race',
      icon: '⚡',
      description: 'Auto-match and race immediately',
    },
    {
      id: 'public',
      label: 'Public Rooms',
      icon: '🌐',
      description: 'Browse and join open rooms',
    },
    {
      id: 'ranked',
      label: 'Ranked',
      icon: '🏆',
      description: 'Skill-matched 4-player races',
    },
  ];

  return (
    <div className="space-y-6 max-w-3xl">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-bold flex items-center gap-2" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🏎️ Mario Kart DS
        </h1>
        <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Online racing via Wiimmfi WFC — no port forwarding or DNS config required.
        </p>
      </div>

      {/* Mode cards */}
      <div className="grid grid-cols-3 gap-3">
        {MODES.map((mode) => (
          <button
            key={mode.id}
            onClick={() => setActiveMode(mode.id)}
            className="rounded-2xl p-4 text-left transition-all"
            style={{
              backgroundColor:
                activeMode === mode.id ? 'rgba(230,0,18,0.12)' : 'var(--color-oasis-card)',
              border:
                activeMode === mode.id
                  ? '1px solid rgba(230,0,18,0.4)'
                  : '1px solid var(--n-border)',
            }}
          >
            <div className="text-2xl mb-1">{mode.icon}</div>
            <p
              className="text-sm font-bold"
              style={{ color: activeMode === mode.id ? 'var(--color-oasis-accent)' : 'var(--color-oasis-text)' }}
            >
              {mode.label}
            </p>
            <p className="text-[10px] mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {mode.description}
            </p>
          </button>
        ))}
      </div>

      {/* Active panel */}
      <div
        className="rounded-2xl p-5"
        style={{ backgroundColor: 'var(--color-oasis-surface)', border: '1px solid var(--n-border)' }}
      >
        {activeMode === 'quick'  && <QuickRacePanel   displayName={displayName} />}
        {activeMode === 'public' && <PublicRoomsPanel displayName={displayName} />}
        {activeMode === 'ranked' && <RankedRacesPanel displayName={displayName} />}
      </div>

      {/* WFC badge */}
      <div
        className="rounded-2xl p-4 flex items-start gap-3"
        style={{ backgroundColor: 'rgba(74,222,128,0.07)', border: '1px solid rgba(74,222,128,0.18)' }}
      >
        <span className="text-xl">🌐</span>
        <div>
          <p className="text-sm font-bold" style={{ color: '#4ade80' }}>
            Powered by Wiimmfi WFC
          </p>
          <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            RetroOasis auto-configures melonDS to use DNS 178.62.43.212 for WFC. No manual setup needed —
            just add your ROM and race.
          </p>
        </div>
      </div>
    </div>
  );
}
