import { useState, useEffect, useCallback } from 'react';
import { useLobby } from '../context/LobbyContext';
import { HostRoomModal } from '../components/HostRoomModal';
import type { CreateRoomPayload } from '../services/lobby-types';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

type ZeldaGame = {
  id: string;
  title: string;
  system: 'gba' | 'nds' | 'gbc';
  mode: string;
  modeEmoji: string;
  maxPlayers: number;
  netplayMode: string;
  wfc?: boolean;
  templateId: string;
  description: string;
};

type ActiveTab = 'coop' | 'battle' | 'leaderboard';

interface ZeldaRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------

const GBA_ZELDA_GAMES: ZeldaGame[] = [
  {
    id: 'gba-zelda-four-swords',
    title: 'The Legend of Zelda: Four Swords',
    system: 'gba',
    mode: '⚔️ 4P Co-op',
    modeEmoji: '⚔️',
    maxPlayers: 4,
    netplayMode: 'online-relay',
    templateId: 'gba-zelda-four-swords-4p',
    description: 'Four Links must work together — and compete — to rescue Zelda. Iconic 4-player link cable co-op via online relay.',
  },
  {
    id: 'gba-zelda-four-swords',
    title: 'The Legend of Zelda: Four Swords (2P)',
    system: 'gba',
    mode: '⚔️ 2P Co-op',
    modeEmoji: '⚔️',
    maxPlayers: 2,
    netplayMode: 'online-relay',
    templateId: 'gba-zelda-four-swords-2p',
    description: 'Play Four Swords with just two players — great for coordinated puzzle-solving and competitive scoring.',
  },
];

const GBC_ZELDA_GAMES: ZeldaGame[] = [
  {
    id: 'gbc-zelda-oracle-ages',
    title: 'The Legend of Zelda: Oracle of Ages',
    system: 'gbc',
    mode: '🔗 2P Link',
    modeEmoji: '🔗',
    maxPlayers: 2,
    netplayMode: 'online-p2p',
    templateId: '',
    description: 'Trade rings and secrets between Oracle of Ages and Oracle of Seasons via link cable emulation.',
  },
  {
    id: 'gbc-zelda-oracle-seasons',
    title: 'The Legend of Zelda: Oracle of Seasons',
    system: 'gbc',
    mode: '🔗 2P Link',
    modeEmoji: '🔗',
    maxPlayers: 2,
    netplayMode: 'online-p2p',
    templateId: '',
    description: 'The companion game to Oracle of Ages — link the two to unlock the true final boss and ending.',
  },
];

const NDS_ZELDA_GAMES: ZeldaGame[] = [
  {
    id: 'nds-zelda-phantom-hourglass',
    title: 'The Legend of Zelda: Phantom Hourglass',
    system: 'nds',
    mode: '🗡️ 2P Battle',
    modeEmoji: '🗡️',
    maxPlayers: 2,
    netplayMode: 'online-relay',
    wfc: true,
    templateId: 'nds-zelda-phantom-hourglass-2p',
    description: 'Asymmetric 2-player battle: one player guides Link while the other commands the Phantoms. Requires Wiimmfi.',
  },
];

const ALL_ZELDA_GAMES = [...GBA_ZELDA_GAMES, ...GBC_ZELDA_GAMES, ...NDS_ZELDA_GAMES];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function systemLabel(system: string) {
  const map: Record<string, string> = {
    gba: 'GBA',
    gbc: 'GBC',
    nds: 'NDS',
  };
  return map[system] ?? system.toUpperCase();
}

function systemGradient(system: string) {
  const map: Record<string, string> = {
    gba: 'var(--gradient-gba)',
    gbc: 'var(--gradient-gbc)',
    nds: 'var(--gradient-nds)',
  };
  return map[system] ?? 'linear-gradient(135deg,#1a1a2e,#16213e)';
}

async function apiFetch<T>(path: string): Promise<T> {
  const res = await fetch(`${API}${path}`, {
    headers: { 'Content-Type': 'application/json' },
  });
  if (!res.ok) throw new Error('Request failed');
  return res.json() as Promise<T>;
}

// ---------------------------------------------------------------------------
// Co-op Rooms panel
// ---------------------------------------------------------------------------

function CoopRoomsPanel({ displayName }: { displayName: string }) {
  const { publicRooms, createRoom, joinByCode, listRooms } = useLobby();
  const [hostingGame, setHostingGame] = useState<ZeldaGame | null>(null);
  const [status, setStatus] = useState('');

  useEffect(() => {
    listRooms();
    const iv = setInterval(listRooms, 8000);
    return () => clearInterval(iv);
  }, [listRooms]);

  const zeldaCoopGames = [...GBA_ZELDA_GAMES, ...GBC_ZELDA_GAMES];
  const hostableGames = zeldaCoopGames.filter((g) => g.templateId !== '');
  const openRooms = publicRooms.filter((r) =>
    zeldaCoopGames.some((g) => g.id === r.gameId)
  );

  async function handleJoinRoom(roomCode: string) {
    if (!displayName) { setStatus('Enter your display name in Settings first.'); return; }
    joinByCode(roomCode, displayName);
  }

  function handleHostGame(game: ZeldaGame) {
    if (!displayName) { setStatus('Enter your display name in Settings first.'); return; }
    setHostingGame(game);
  }

  const handleHostConfirm = useCallback(
    (payload: Omit<CreateRoomPayload, 'displayName'>, dn: string) => {
      createRoom(payload, dn);
      setHostingGame(null);
      setStatus('Room created! Waiting for players…');
    },
    [createRoom]
  );

  return (
    <div className="space-y-6">
      {status && (
        <div
          className="p-3 rounded-xl text-sm font-semibold"
          style={{ backgroundColor: 'rgba(230,0,18,0.12)', color: 'var(--color-oasis-accent)' }}
        >
          {status}
        </div>
      )}

      {/* Co-op game selector */}
      <div>
        <h3 className="text-xs font-black uppercase tracking-widest mb-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Host a Co-op Room
        </h3>
        <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
          {hostableGames.map((game, i) => (
            <div
              key={`${game.id}-${i}`}
              className="rounded-2xl p-4 flex flex-col gap-2"
              style={{ background: systemGradient(game.system), border: '1px solid var(--n-border)' }}
            >
              <div className="flex items-start justify-between gap-2">
                <div>
                  <p className="text-sm font-black leading-tight" style={{ color: '#fff' }}>
                    {game.title}
                  </p>
                  <p className="text-[10px] font-semibold mt-0.5" style={{ color: 'rgba(255,255,255,0.65)' }}>
                    {systemLabel(game.system)} · {game.mode} · {game.maxPlayers}P
                  </p>
                </div>
                <span className="text-lg flex-shrink-0">{game.modeEmoji}</span>
              </div>
              <p className="text-[11px] leading-relaxed" style={{ color: 'rgba(255,255,255,0.55)' }}>
                {game.description}
              </p>
              <button
                onClick={() => handleHostGame(game)}
                className="mt-1 text-xs font-black px-3 py-1.5 rounded-xl self-start"
                style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
              >
                Host Room
              </button>
            </div>
          ))}
        </div>
      </div>

      {/* GBC Oracle info (link cable only — no host flow) */}
      <div>
        <h3 className="text-xs font-black uppercase tracking-widest mb-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
          GBC Oracle Games (Link Cable)
        </h3>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          {GBC_ZELDA_GAMES.map((game) => (
            <div
              key={game.id}
              className="rounded-2xl p-3 flex items-start gap-3"
              style={{ background: systemGradient(game.system), border: '1px solid var(--n-border)', opacity: 0.8 }}
            >
              <span className="text-xl flex-shrink-0">{game.modeEmoji}</span>
              <div>
                <p className="text-xs font-black leading-tight" style={{ color: '#fff' }}>{game.title.replace('The Legend of Zelda: ', '')}</p>
                <p className="text-[10px] mt-0.5" style={{ color: 'rgba(255,255,255,0.55)' }}>
                  GBC · Link Cable (p2p) · 2P trade/secrets
                </p>
              </div>
            </div>
          ))}
        </div>
        <p className="text-[11px] mt-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Oracle games use GBC link cable emulation (p2p). Use the standard game library to start a session.
        </p>
      </div>

      {/* Open rooms */}
      <div>
        <h3 className="text-xs font-black uppercase tracking-widest mb-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Open Co-op Rooms ({openRooms.length})
        </h3>
        {openRooms.length === 0 ? (
          <div
            className="rounded-2xl p-5 text-center"
            style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
          >
            <p className="text-2xl mb-1">🗡️</p>
            <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              No open co-op rooms right now. Be the hero — host one!
            </p>
          </div>
        ) : (
          <div className="space-y-2">
            {openRooms.map((room) => (
              <div
                key={room.id}
                className="rounded-2xl p-3 flex items-center justify-between gap-3"
                style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
              >
                <div>
                  <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
                    {room.name}
                  </p>
                  <p className="text-[11px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    {room.gameTitle} · {room.players.length}/{room.maxPlayers} players
                  </p>
                </div>
                <button
                  onClick={() => handleJoinRoom(room.roomCode)}
                  className="text-xs font-black px-3 py-1.5 rounded-xl flex-shrink-0"
                  style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
                >
                  Join
                </button>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Host modal */}
      {hostingGame && (
        <HostRoomModal
          preselectedGameId={hostingGame.id}
          onConfirm={handleHostConfirm}
          onClose={() => setHostingGame(null)}
        />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Battle Mode panel (NDS Phantom Hourglass)
// ---------------------------------------------------------------------------

function BattleModePanel({ displayName }: { displayName: string }) {
  const { publicRooms, createRoom, joinByCode, listRooms } = useLobby();
  const [hostModalOpen, setHostModalOpen] = useState(false);
  const [status, setStatus] = useState('');

  const BATTLE_GAME = NDS_ZELDA_GAMES[0];

  useEffect(() => {
    listRooms();
    const iv = setInterval(listRooms, 8000);
    return () => clearInterval(iv);
  }, [listRooms]);

  const battleRooms = publicRooms.filter((r) => r.gameId === BATTLE_GAME.id);

  const handleHostConfirm = useCallback(
    (payload: Omit<CreateRoomPayload, 'displayName'>, dn: string) => {
      createRoom(payload, dn);
      setHostModalOpen(false);
      setStatus('Battle room created! Waiting for challenger…');
    },
    [createRoom]
  );

  return (
    <div className="space-y-6">
      {/* WFC banner */}
      <div
        className="rounded-2xl p-4 flex items-start gap-3"
        style={{ backgroundColor: 'rgba(26,160,198,0.12)', border: '1px solid rgba(26,160,198,0.3)' }}
      >
        <span className="text-2xl flex-shrink-0">🌐</span>
        <div>
          <p className="text-sm font-black" style={{ color: 'var(--color-oasis-blue)' }}>
            Wiimmfi Required
          </p>
          <p className="text-xs leading-relaxed mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Phantom Hourglass Battle Mode uses Nintendo WFC via{' '}
            <strong style={{ color: 'var(--color-oasis-text)' }}>Wiimmfi</strong> (178.62.43.212).
            RetroOasis configures this automatically when you host or join a battle room.
          </p>
        </div>
      </div>

      {/* Game info card */}
      <div
        className="rounded-2xl p-4"
        style={{ background: systemGradient('nds'), border: '1px solid var(--n-border)' }}
      >
        <div className="flex items-start justify-between gap-3">
          <div>
            <p className="text-base font-black" style={{ color: '#fff' }}>
              {BATTLE_GAME.title}
            </p>
            <p className="text-xs mt-0.5" style={{ color: 'rgba(255,255,255,0.65)' }}>
              NDS · 2P Battle Mode · Wiimmfi Online
            </p>
          </div>
          <span className="text-3xl">🗡️</span>
        </div>
        <p className="text-xs mt-2 leading-relaxed" style={{ color: 'rgba(255,255,255,0.6)' }}>
          {BATTLE_GAME.description}
        </p>
        <button
          onClick={() => { if (!displayName) { setStatus('Enter your display name in Settings.'); return; } setHostModalOpen(true); }}
          className="mt-3 text-sm font-black px-4 py-2 rounded-xl"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
        >
          Host Battle Room
        </button>
      </div>

      {status && (
        <div className="p-3 rounded-xl text-sm font-semibold" style={{ backgroundColor: 'rgba(230,0,18,0.12)', color: 'var(--color-oasis-accent)' }}>
          {status}
        </div>
      )}

      {/* Open battle rooms */}
      <div>
        <h3 className="text-xs font-black uppercase tracking-widest mb-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Open Battle Rooms ({battleRooms.length})
        </h3>
        {battleRooms.length === 0 ? (
          <div
            className="rounded-2xl p-5 text-center"
            style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
          >
            <p className="text-2xl mb-1">🗡️</p>
            <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              No battle rooms open. Challenge someone!
            </p>
          </div>
        ) : (
          <div className="space-y-2">
            {battleRooms.map((room) => (
              <div
                key={room.id}
                className="rounded-2xl p-3 flex items-center justify-between gap-3"
                style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
              >
                <div>
                  <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>{room.name}</p>
                  <p className="text-[11px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    {room.players.length}/{room.maxPlayers} players
                  </p>
                </div>
                <button
                  onClick={() => joinByCode(room.roomCode, displayName)}
                  className="text-xs font-black px-3 py-1.5 rounded-xl"
                  style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
                >
                  Join
                </button>
              </div>
            ))}
          </div>
        )}
      </div>

      {hostModalOpen && (
        <HostRoomModal
          preselectedGameId={BATTLE_GAME.id}
          onConfirm={handleHostConfirm}
          onClose={() => setHostModalOpen(false)}
        />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Leaderboard panel
// ---------------------------------------------------------------------------

function LeaderboardPanel() {
  const [rankings, setRankings] = useState<ZeldaRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: ZeldaRanking[] }>('/api/leaderboard?metric=sessions&limit=10')
      .then((data) => setRankings(data.leaderboard))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  return (
    <div className="space-y-4">
      <div
        className="rounded-2xl p-4"
        style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
      >
        <h3 className="text-xs font-black uppercase tracking-widest mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          🏆 Top Players (by Sessions)
        </h3>
        {loading ? (
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>Loading…</p>
        ) : rankings.length === 0 ? (
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No ranked players yet. Be the first to host a Zelda session!
          </p>
        ) : (
          <div className="space-y-2">
            {rankings.map((r, i) => (
              <div key={r.displayName} className="flex items-center gap-3">
                <span className="w-6 text-center text-base">
                  {i === 0 ? '🥇' : i === 1 ? '🥈' : i === 2 ? '🥉' : `#${i + 1}`}
                </span>
                <span className="flex-1 text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
                  {r.displayName}
                </span>
                <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {r.sessions} sessions
                </span>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Main ZeldaPage
// ---------------------------------------------------------------------------

export function ZeldaPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('coop');
  const [displayName, setDisplayName] = useState('');

  useEffect(() => {
    const stored = localStorage.getItem('retro-oasis-display-name');
    if (stored) setDisplayName(stored);
  }, []);

  const tabs: { id: ActiveTab; label: string; icon: string }[] = [
    { id: 'coop', label: 'Co-op Rooms', icon: '⚔️' },
    { id: 'battle', label: 'Battle Mode', icon: '🗡️' },
    { id: 'leaderboard', label: 'Leaderboard', icon: '🏆' },
  ];

  return (
    <div className="space-y-6">
      {/* Header */}
      <div>
        <div className="flex items-center gap-3 mb-1">
          <span className="text-4xl">🗡️</span>
          <div>
            <h1 className="text-2xl font-black" style={{ color: '#fff' }}>
              Zelda Multiplayer
            </h1>
            <p className="text-sm mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Four Swords co-op, Phantom Hourglass battle mode &amp; Oracle link cable
            </p>
          </div>
        </div>

        {/* Game chips */}
        <div className="flex flex-wrap gap-2 mt-3">
          {ALL_ZELDA_GAMES.filter((g, i, arr) => arr.findIndex((x) => x.id === g.id) === i).map((g) => (
            <span
              key={g.id}
              className="text-[11px] font-bold px-2.5 py-1 rounded-full"
              style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)', border: '1px solid var(--n-border)' }}
            >
              {systemLabel(g.system)} {g.title.replace('The Legend of Zelda: ', '')}
            </span>
          ))}
        </div>
      </div>

      {/* Tabs */}
      <div className="flex gap-1 p-1 rounded-2xl" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="flex-1 flex items-center justify-center gap-2 py-2.5 rounded-xl text-sm font-bold transition-all"
            style={{
              backgroundColor: activeTab === tab.id ? 'var(--color-oasis-accent)' : 'transparent',
              color: activeTab === tab.id ? '#fff' : 'var(--color-oasis-text-muted)',
            }}
          >
            <span>{tab.icon}</span>
            <span>{tab.label}</span>
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'coop' && <CoopRoomsPanel displayName={displayName} />}
      {activeTab === 'battle' && <BattleModePanel displayName={displayName} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
    </div>
  );
}
