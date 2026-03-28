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

type SystemFilter = 'n64' | 'nds' | 'gba';
type ActiveTab = 'lobby' | 'leaderboard';

interface SportsGame {
  id: string;
  title: string;
  sport: string;
  sportEmoji: string;
  system: SystemFilter;
  maxPlayers: number;
  netplayMode: string;
  wfc?: boolean;
}

interface SportsRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------

const N64_SPORTS_GAMES: SportsGame[] = [
  { id: 'n64-mario-tennis',  title: 'Mario Tennis',  sport: '🎾 Tennis',     sportEmoji: '🎾', system: 'n64', maxPlayers: 4, netplayMode: 'online-relay' },
  { id: 'n64-mario-golf',    title: 'Mario Golf',    sport: '⛳ Golf',        sportEmoji: '⛳', system: 'n64', maxPlayers: 4, netplayMode: 'online-relay' },
];

const NDS_SPORTS_GAMES: SportsGame[] = [
  { id: 'nds-mario-hoops-3on3',              title: 'Mario Hoops 3-on-3',                sport: '🏀 Basketball', sportEmoji: '🏀', system: 'nds', maxPlayers: 4, netplayMode: 'online-relay' },
  { id: 'nds-mario-and-sonic-olympic-games', title: 'Mario & Sonic at the Olympic Games', sport: '🏅 Olympics',   sportEmoji: '🏅', system: 'nds', maxPlayers: 4, netplayMode: 'online-relay', wfc: true },
];

const GBA_SPORTS_GAMES: SportsGame[] = [
  { id: 'gba-mario-tennis-power-tour', title: 'Mario Tennis: Power Tour', sport: '🎾 Tennis', sportEmoji: '🎾', system: 'gba', maxPlayers: 4, netplayMode: 'online-p2p' },
  { id: 'gba-mario-golf-advance-tour', title: 'Mario Golf: Advance Tour', sport: '⛳ Golf',   sportEmoji: '⛳', system: 'gba', maxPlayers: 4, netplayMode: 'online-p2p' },
];

const ALL_SPORTS_GAMES = [...N64_SPORTS_GAMES, ...NDS_SPORTS_GAMES, ...GBA_SPORTS_GAMES];

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
// System info banners
// ---------------------------------------------------------------------------

function SystemBanner({ system }: { system: SystemFilter }) {
  if (system === 'n64') {
    return (
      <div
        className="rounded-2xl p-4 flex items-start gap-3"
        style={{ backgroundColor: 'rgba(251,146,60,0.08)', border: '1px solid rgba(251,146,60,0.25)' }}
      >
        <span className="text-xl">🟠</span>
        <div>
          <p className="text-sm font-bold" style={{ color: '#fb923c' }}>
            N64 Relay Netplay
          </p>
          <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            N64 sports use RetroOasis relay netplay via Mupen64Plus. Up to 4 players with
            automatic controller assignment — no port forwarding required.
          </p>
        </div>
      </div>
    );
  }
  if (system === 'nds') {
    return (
      <div
        className="rounded-2xl p-4 flex items-start gap-3"
        style={{ backgroundColor: 'rgba(74,222,128,0.07)', border: '1px solid rgba(74,222,128,0.18)' }}
      >
        <span className="text-xl">🌐</span>
        <div>
          <p className="text-sm font-bold" style={{ color: '#4ade80' }}>
            DS Relay / Wiimmfi WFC
          </p>
          <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            DS sports use relay netplay via melonDS. WFC-enabled titles (marked 🌐) are
            auto-configured for Wiimmfi (DNS 178.62.43.212) — no manual setup required.
          </p>
        </div>
      </div>
    );
  }
  // gba
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ backgroundColor: 'rgba(96,165,250,0.08)', border: '1px solid rgba(96,165,250,0.22)' }}
    >
      <span className="text-xl">🔗</span>
      <div>
        <p className="text-sm font-bold" style={{ color: '#60a5fa' }}>
          GBA Link Cable (P2P Netplay)
        </p>
        <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          GBA sports emulate the link cable over mGBA peer-to-peer netplay. Up to 4 players
          with low-latency P2P connections — no relay server needed.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Lobby panel
// ---------------------------------------------------------------------------

function LobbyPanel({
  displayName,
  system,
}: {
  displayName: string;
  system: SystemFilter;
}) {
  const { publicRooms, createRoom, joinByCode, listRooms } = useLobby();
  const games = ALL_SPORTS_GAMES.filter((g) => g.system === system);
  const [selectedGame, setSelectedGame] = useState<SportsGame>(games[0]);
  const [showHost, setShowHost] = useState(false);
  const [joinTarget, setJoinTarget] = useState<string | null>(null);
  const [queued, setQueued] = useState(false);
  const [status, setStatus] = useState('');

  // Reset selection when system changes
  useEffect(() => {
    const first = ALL_SPORTS_GAMES.find((g) => g.system === system);
    if (first) setSelectedGame(first);
    setQueued(false);
    setStatus('');
  }, [system]);

  useEffect(() => {
    listRooms();
  }, [listRooms]);

  // Auto-join when a quick-match room becomes available
  useEffect(() => {
    if (!queued) return;
    const openRoom = publicRooms.find(
      (r) =>
        r.gameId === selectedGame.id &&
        r.players.length < r.maxPlayers
    );
    if (openRoom) {
      joinByCode(openRoom.roomCode, displayName);
      setQueued(false);
      setStatus('✅ Joined a room!');
    }
  }, [queued, publicRooms, selectedGame, joinByCode, displayName]);

  async function handleQuickMatch() {
    if (!displayName) return;
    setStatus('Looking for an open game…');

    const openRoom = publicRooms.find(
      (r) => r.gameId === selectedGame.id && r.players.length < r.maxPlayers
    );
    if (openRoom) {
      joinByCode(openRoom.roomCode, displayName);
      setStatus('✅ Joining existing room…');
      return;
    }

    try {
      const playerId = `${displayName}-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
      const result = await apiFetch<{ position: number }>('/api/matchmaking/join', {
        method: 'POST',
        body: JSON.stringify({
          playerId,
          displayName,
          gameId: selectedGame.id,
          gameTitle: selectedGame.title,
          system,
          maxPlayers: selectedGame.maxPlayers,
        }),
      });
      setQueued(true);
      setStatus(`⚡ Queue position ${result.position} — hosting a room while you wait…`);

      createRoom(
        {
          gameId: selectedGame.id,
          gameTitle: selectedGame.title,
          system,
          maxPlayers: selectedGame.maxPlayers,
          isPublic: true,
          name: `${displayName}'s ${selectedGame.title}`,
        },
        displayName
      );
    } catch (err) {
      setStatus(`❌ ${(err as Error).message}`);
    }
  }

  function handleHostConfirm(payload: Omit<CreateRoomPayload, 'displayName'>, dn: string) {
    createRoom(payload, dn);
    setShowHost(false);
  }

  const gameRooms = publicRooms.filter((r) => r.gameId === selectedGame.id);

  return (
    <div className="space-y-5">
      {/* Game selector */}
      <div>
        <p className="text-xs font-bold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Select game
        </p>
        <div className="flex flex-wrap gap-2">
          {games.map((g) => (
            <button
              key={g.id}
              onClick={() => { setSelectedGame(g); setQueued(false); setStatus(''); }}
              className="text-xs font-bold px-3 py-1.5 rounded-xl transition-all"
              style={{
                backgroundColor:
                  selectedGame.id === g.id ? 'rgba(230,0,18,0.15)' : 'var(--color-oasis-card)',
                border:
                  selectedGame.id === g.id
                    ? '1px solid rgba(230,0,18,0.4)'
                    : '1px solid var(--n-border)',
                color:
                  selectedGame.id === g.id
                    ? 'var(--color-oasis-accent)'
                    : 'var(--color-oasis-text)',
              }}
            >
              {g.sport}&nbsp;&nbsp;{g.title}
              {g.wfc && (
                <span className="ml-1.5 text-[9px] font-black px-1 py-0.5 rounded" style={{ backgroundColor: 'rgba(74,222,128,0.18)', color: '#4ade80' }}>
                  WFC
                </span>
              )}
            </button>
          ))}
        </div>
      </div>

      {/* Quick match */}
      <div
        className="rounded-2xl p-4"
        style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
      >
        <p className="text-sm font-bold mb-1" style={{ color: 'var(--color-oasis-accent-light)' }}>
          ⚡ Quick Match — {selectedGame.title}
        </p>
        <p className="text-xs mb-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Find an open game or host one for others to join. Up to {selectedGame.maxPlayers} players.
        </p>

        {!queued ? (
          <div className="flex items-center gap-3 flex-wrap">
            <button
              onClick={() => void handleQuickMatch()}
              disabled={!displayName}
              className="text-sm font-bold px-5 py-2 rounded-xl"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff', opacity: displayName ? 1 : 0.5 }}
            >
              ⚡ Quick Match
            </button>
            <button
              onClick={() => setShowHost(true)}
              disabled={!displayName}
              className="text-sm font-bold px-4 py-2 rounded-xl"
              style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)', border: '1px solid var(--n-border)', opacity: displayName ? 1 : 0.5 }}
            >
              🏟️ Host Room
            </button>
            <button
              onClick={() => { listRooms(); }}
              className="text-xs font-bold px-3 py-2 rounded-xl"
              style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)' }}
            >
              ↻ Refresh
            </button>
          </div>
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
            Set a display name in Settings to play.
          </p>
        )}
      </div>

      {/* Open rooms */}
      {gameRooms.length === 0 ? (
        <div
          className="rounded-2xl p-6 text-center"
          style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
        >
          <p className="text-2xl mb-2">{selectedGame.sportEmoji}</p>
          <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
            No open rooms for {selectedGame.title}
          </p>
          <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Host a room and invite friends, or use Quick Match to join the queue.
          </p>
        </div>
      ) : (
        <div className="space-y-2">
          <p className="text-xs font-bold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Open rooms ({gameRooms.length})
          </p>
          {gameRooms.map((r) => {
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
          preselectedGameId={selectedGame.id}
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
// Leaderboard panel
// ---------------------------------------------------------------------------

function LeaderboardPanel({ displayName }: { displayName: string }) {
  const [rankings, setRankings] = useState<SportsRanking[]>([]);

  const fetchRankings = useCallback(async () => {
    try {
      const board = await apiFetch<{ displayName: string; sessions: number }[]>(
        '/api/leaderboard?metric=sessions&limit=20'
      );
      setRankings(board.filter((e) => e.sessions > 0));
    } catch {
      setRankings([]);
    }
  }, []);

  useEffect(() => {
    void fetchRankings();
  }, [fetchRankings]);

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <h3 className="text-base font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🏅 Global Leaderboard
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
          No data yet — play some games to appear on the board!
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
                border:
                  entry.displayName === displayName
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
                  {entry.sessions} sessions
                </p>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------

export function MarioSportsPage() {
  const [system, setSystem] = useState<SystemFilter>('n64');
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [displayName, setDisplayName] = useState('');

  useEffect(() => {
    const stored = localStorage.getItem('retro-oasis-display-name');
    if (stored) setDisplayName(stored);
  }, []);

  const SYSTEMS: { id: SystemFilter; label: string; icon: string; color: string }[] = [
    { id: 'n64',  label: 'N64',        icon: '🟠', color: '#fb923c' },
    { id: 'nds',  label: 'Nintendo DS', icon: '📺', color: '#4ade80' },
    { id: 'gba',  label: 'GBA',        icon: '🔵', color: '#60a5fa' },
  ];

  const TABS: { id: ActiveTab; label: string; icon: string }[] = [
    { id: 'lobby',       label: 'Lobby',       icon: '🎮' },
    { id: 'leaderboard', label: 'Leaderboard', icon: '🏅' },
  ];

  return (
    <div className="space-y-6 max-w-3xl">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-bold flex items-center gap-2" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🎾 Mario Sports
        </h1>
        <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Online multiplayer for Mario Tennis, Golf, Basketball &amp; more across N64, DS, and GBA.
        </p>
      </div>

      {/* System selector */}
      <div className="grid grid-cols-3 gap-3">
        {SYSTEMS.map((s) => (
          <button
            key={s.id}
            onClick={() => setSystem(s.id)}
            className="rounded-2xl p-4 text-left transition-all"
            style={{
              backgroundColor:
                system === s.id ? 'rgba(230,0,18,0.12)' : 'var(--color-oasis-card)',
              border:
                system === s.id ? '1px solid rgba(230,0,18,0.4)' : '1px solid var(--n-border)',
            }}
          >
            <div className="text-2xl mb-1">{s.icon}</div>
            <p
              className="text-sm font-bold"
              style={{ color: system === s.id ? 'var(--color-oasis-accent)' : 'var(--color-oasis-text)' }}
            >
              {s.label}
            </p>
            <p className="text-[10px] mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {ALL_SPORTS_GAMES.filter((g) => g.system === s.id).length} games
            </p>
          </button>
        ))}
      </div>

      {/* Tabs */}
      <div className="flex gap-2">
        {TABS.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="text-sm font-bold px-4 py-2 rounded-xl transition-all"
            style={{
              backgroundColor:
                activeTab === tab.id ? 'rgba(230,0,18,0.12)' : 'var(--color-oasis-card)',
              border:
                activeTab === tab.id ? '1px solid rgba(230,0,18,0.4)' : '1px solid var(--n-border)',
              color:
                activeTab === tab.id ? 'var(--color-oasis-accent)' : 'var(--color-oasis-text)',
            }}
          >
            {tab.icon} {tab.label}
          </button>
        ))}
      </div>

      {/* Active tab content */}
      <div
        className="rounded-2xl p-5"
        style={{ backgroundColor: 'var(--color-oasis-surface)', border: '1px solid var(--n-border)' }}
      >
        {activeTab === 'lobby' && (
          <LobbyPanel displayName={displayName} system={system} />
        )}
        {activeTab === 'leaderboard' && (
          <LeaderboardPanel displayName={displayName} />
        )}
      </div>

      {/* System info banner */}
      <SystemBanner system={system} />
    </div>
  );
}
