import { useState, useEffect, useCallback, useMemo } from 'react';
import { Link } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

const SYSTEM_COLOR = '#4B5EFC';
const SYSTEM_COLOR_DARK = '#1A1A2E';
const SYSTEM_COLOR_MID = 'rgba(75,94,252,0.18)';

type ActiveTab = 'lobby' | 'leaderboard' | 'setup';

type ModeFilter = 'all' | 'multiplayer' | 'single';

interface SegaCDGame {
  id: string;
  title: string;
  genre: string;
  genreEmoji: string;
  maxPlayers: number;
  competitive?: boolean;
  coopOnly?: boolean;
  singleOnly?: boolean;
  year?: number;
  cdAudio?: boolean;
  description?: string;
}

interface SegaCDRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------

const SEGACD_GAMES: SegaCDGame[] = [
  // Multiplayer
  {
    id: 'segacd-final-fight-cd',
    title: 'Final Fight CD',
    genre: 'Beat-em-up',
    genreEmoji: '🥊',
    maxPlayers: 2,
    coopOnly: true,
    year: 1993,
    cdAudio: true,
    description: 'The definitive console port — CD version restores Guy and all censored content.',
  },
  {
    id: 'segacd-batman-returns',
    title: 'Batman Returns',
    genre: 'Beat-em-up',
    genreEmoji: '🦇',
    maxPlayers: 2,
    coopOnly: true,
    year: 1993,
    cdAudio: true,
    description: 'Co-op brawler with Danny Elfman\'s original film score streaming from CD.',
  },
  {
    id: 'segacd-robo-aleste',
    title: 'Robo Aleste',
    genre: 'Shoot-em-up',
    genreEmoji: '🤖',
    maxPlayers: 2,
    coopOnly: true,
    year: 1992,
    cdAudio: true,
    description: 'Vertical shooter set in feudal Japan with two-player simultaneous mode.',
  },
  // Single-player highlights
  {
    id: 'segacd-sonic-cd',
    title: 'Sonic CD',
    genre: 'Platformer',
    genreEmoji: '💿',
    maxPlayers: 1,
    singleOnly: true,
    year: 1993,
    cdAudio: true,
    description: 'Sonic\'s time-travel adventure — widely considered the best Sonic game of the era.',
  },
  {
    id: 'segacd-lunar-silver-star',
    title: 'Lunar: The Silver Star',
    genre: 'RPG',
    genreEmoji: '🌙',
    maxPlayers: 1,
    singleOnly: true,
    year: 1993,
    cdAudio: true,
    description: 'Landmark RPG with voiced cutscenes and full CD audio soundtrack.',
  },
  {
    id: 'segacd-snatcher',
    title: 'Snatcher',
    genre: 'Adventure',
    genreEmoji: '🔍',
    maxPlayers: 1,
    singleOnly: true,
    year: 1994,
    cdAudio: true,
    description: 'Hideo Kojima\'s cyberpunk noir adventure — English localisation exclusive to Sega CD.',
  },
  {
    id: 'segacd-silpheed',
    title: 'Silpheed',
    genre: 'Shoot-em-up',
    genreEmoji: '🚀',
    maxPlayers: 1,
    singleOnly: true,
    year: 1993,
    cdAudio: true,
    description: 'Pseudo-3D space shooter that pushed the CD-ROM\'s FMV streaming capabilities.',
  },
  {
    id: 'segacd-popful-mail',
    title: 'Popful Mail',
    genre: 'Action RPG',
    genreEmoji: '✉️',
    maxPlayers: 1,
    singleOnly: true,
    year: 1994,
    cdAudio: true,
    description: 'Falcom action RPG localised by Working Designs with full voice acting.',
  },
  {
    id: 'segacd-keio-flying-squadron',
    title: 'Keio Flying Squadron',
    genre: 'Shoot-em-up',
    genreEmoji: '🐉',
    maxPlayers: 1,
    singleOnly: true,
    year: 1993,
    cdAudio: true,
    description: 'Colourful horizontal shooter featuring a tanuki riding a dragon.',
  },
];

const ALL_MODE_FILTERS: { id: ModeFilter; label: string }[] = [
  { id: 'all', label: 'All Games' },
  { id: 'multiplayer', label: 'Multiplayer' },
  { id: 'single', label: 'Single Player' },
];

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
// Rank badge
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
// Info banner
// ---------------------------------------------------------------------------

function SegaCDBanner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ backgroundColor: 'rgba(75,94,252,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
    >
      <span className="text-xl mt-0.5">💿</span>
      <div className="space-y-1.5">
        <p className="text-sm font-bold" style={{ color: '#818cf8' }}>
          SEGA CD / Mega-CD · Relay Netplay via RetroArch + Genesis Plus GX
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          The same Genesis Plus GX core handles Sega CD disc images natively. Supported
          formats:
          {['.cue', '.chd', '.img'].map((ext) => (
            <code
              key={ext}
              className="mx-0.5 px-1 rounded"
              style={{ background: 'rgba(75,94,252,0.18)', color: '#a5b4fc' }}
            >
              {ext}
            </code>
          ))}
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          A region-appropriate BIOS image is required:{' '}
          {['bios_CD_U.bin', 'bios_CD_E.bin', 'bios_CD_J.bin'].map((b) => (
            <code
              key={b}
              className="mx-0.5 px-1 rounded"
              style={{ background: 'rgba(75,94,252,0.18)', color: '#a5b4fc' }}
            >
              {b}
            </code>
          ))}{' '}
          — place it in your RetroArch system directory.
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          CD audio streams locally on each client; relay netplay syncs game state only.
          Visit the{' '}
          <Link to="/genesis" className="underline" style={{ color: '#818cf8' }}>
            Genesis page
          </Link>{' '}
          for cartridge-based games.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Setup guide tab
// ---------------------------------------------------------------------------

function SetupGuide() {
  const steps = [
    {
      step: 1,
      title: 'Install RetroArch',
      body: 'Download RetroArch from retroarch.com or install via your package manager (e.g. sudo apt install retroarch). On macOS: brew install retroarch.',
    },
    {
      step: 2,
      title: 'Download the Genesis Plus GX core',
      body: 'In RetroArch go to Main Menu → Load Core → Download a Core → SEGA - Mega Drive / Genesis (Genesis Plus GX). The core handles both Genesis cartridges and Sega CD disc images.',
    },
    {
      step: 3,
      title: 'Obtain a Sega CD BIOS',
      body: 'You need a BIOS image matching your region — bios_CD_U.bin (USA), bios_CD_E.bin (Europe), or bios_CD_J.bin (Japan). Place it in your RetroArch system directory (typically ~/.config/retroarch/system/).',
    },
    {
      step: 4,
      title: 'Prepare your disc images',
      body: 'CHD files (.chd) are recommended — they are single-file compressed images. CUE/BIN pairs also work; keep all .bin track files in the same directory as the .cue sheet. Load the .chd or .cue file in RetroArch.',
    },
    {
      step: 5,
      title: 'Set your RetroArch path in RetroOasis',
      body: 'Open Settings → Emulator Paths and point the RetroArch executable field to your retroarch binary. RetroOasis will pass the core and ROM path automatically when you host or join a session.',
    },
    {
      step: 6,
      title: 'Host or join a room',
      body: 'On the Lobby tab, click Host Room to create a new relay session, or Quick Match to find an open room. RetroOasis handles all netplay flags — no manual port forwarding required.',
    },
  ];

  return (
    <div className="space-y-4">
      <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
        Follow these steps to get Sega CD games running on RetroOasis.
      </p>
      <div className="space-y-3">
        {steps.map(({ step, title, body }) => (
          <div
            key={step}
            className="rounded-xl p-4 flex gap-3"
            style={{ background: 'rgba(75,94,252,0.07)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
          >
            <div
              className="w-7 h-7 rounded-full flex items-center justify-center text-xs font-black shrink-0 mt-0.5"
              style={{ background: SYSTEM_COLOR, color: '#fff' }}
            >
              {step}
            </div>
            <div>
              <p className="text-sm font-bold text-white">{title}</p>
              <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>{body}</p>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function SegaCDGameCard({
  game,
  onHost,
  onQuickMatch,
  openRooms,
}: {
  game: SegaCDGame;
  onHost: (game: SegaCDGame) => void;
  onQuickMatch: (game: SegaCDGame) => void;
  openRooms: number;
}) {
  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{
        background: 'linear-gradient(135deg, rgba(75,94,252,0.1), rgba(26,26,46,0.08))',
        border: `1px solid ${SYSTEM_COLOR_MID}`,
      }}
    >
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-2 min-w-0">
          <span className="text-2xl shrink-0">{game.genreEmoji}</span>
          <div className="min-w-0">
            <p className="text-sm font-bold text-white leading-tight truncate">{game.title}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.genre} · {game.maxPlayers === 1 ? '1P' : '2P'}
              {game.year && ` · ${game.year}`}
              {game.cdAudio && (
                <span className="ml-1" title="CD audio soundtrack">💿</span>
              )}
            </p>
          </div>
        </div>
        <div className="flex flex-col items-end gap-1 shrink-0">
          {game.competitive && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(251,146,60,0.15)', color: '#fb923c', border: '1px solid rgba(251,146,60,0.3)' }}
            >
              Competitive
            </span>
          )}
          {game.coopOnly && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(74,222,128,0.1)', color: '#4ade80', border: '1px solid rgba(74,222,128,0.25)' }}
            >
              Co-op
            </span>
          )}
          {game.singleOnly && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(148,163,184,0.1)', color: '#94a3b8', border: '1px solid rgba(148,163,184,0.2)' }}
            >
              Solo
            </span>
          )}
          {openRooms > 0 && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(129,140,248,0.12)', color: '#818cf8', border: '1px solid rgba(129,140,248,0.25)' }}
            >
              {openRooms} open
            </span>
          )}
        </div>
      </div>

      {game.description && (
        <p className="text-xs leading-relaxed" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {game.description}
        </p>
      )}

      <div className="flex gap-2 mt-auto">
        <button
          onClick={() => onQuickMatch(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: `linear-gradient(90deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})`, color: '#fff' }}
        >
          ⚡ Quick Match
        </button>
        <button
          onClick={() => onHost(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: SYSTEM_COLOR_MID, border: `1px solid rgba(75,94,252,0.35)`, color: '#818cf8' }}
        >
          🎮 Host Room
        </button>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Leaderboard panel
// ---------------------------------------------------------------------------

function LeaderboardPanel() {
  const [rankings, setRankings] = useState<SegaCDRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: SegaCDRanking[] }>('/api/leaderboard?system=segacd&orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">💿</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">Play some Sega CD sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(75,94,252,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
        >
          <RankBadge rank={i + 1} />
          <span className="flex-1 text-sm font-semibold text-white">{r.displayName}</span>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {r.sessions} session{r.sessions !== 1 ? 's' : ''}
          </span>
        </div>
      ))}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Lobby panel
// ---------------------------------------------------------------------------

function LobbyPanel({ games }: { games: SegaCDGame[] }) {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';
  const [hostGame, setHostGame] = useState<SegaCDGame | null>(null);
  const [showJoin, setShowJoin] = useState(false);
  const [joinCode, setJoinCode] = useState('');
  const [notification, setNotification] = useState('');
  const [modeFilter, setModeFilter] = useState<ModeFilter>('all');

  const filteredGames = useMemo(() => {
    if (modeFilter === 'multiplayer') return games.filter((g) => !g.singleOnly);
    if (modeFilter === 'single') return games.filter((g) => g.singleOnly);
    return games;
  }, [games, modeFilter]);

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const totalOpenRooms = useMemo(
    () => publicRooms.filter((r) => r.status === 'waiting' && games.some((g) => g.id === r.gameId)).length,
    [publicRooms, games],
  );

  const handleQuickMatch = useCallback(
    async (game: SegaCDGame) => {
      const open = publicRooms.find((r) => r.gameId === game.id && r.status === 'waiting');
      if (open) {
        setJoinCode(open.roomCode);
        setShowJoin(true);
      } else {
        setHostGame(game);
        setNotification(`No open ${game.title} rooms — hosting one for you!`);
        setTimeout(() => setNotification(''), 3000);
      }
    },
    [publicRooms],
  );

  return (
    <div className="space-y-5">
      <SegaCDBanner />

      {/* Stats row */}
      <div className="flex gap-3 flex-wrap">
        {[
          { label: 'Games', value: games.length },
          { label: 'Open Rooms', value: totalOpenRooms },
          { label: 'CD Audio Titles', value: games.filter((g) => g.cdAudio).length },
        ].map(({ label, value }) => (
          <div
            key={label}
            className="flex-1 min-w-[80px] rounded-xl px-3 py-2 text-center"
            style={{ background: 'rgba(75,94,252,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
          >
            <p className="text-lg font-black" style={{ color: '#818cf8' }}>{value}</p>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{label}</p>
          </div>
        ))}
      </div>

      {/* Mode filter */}
      <div className="flex gap-2 flex-wrap">
        {ALL_MODE_FILTERS.map(({ id, label }) => (
          <button
            key={id}
            onClick={() => setModeFilter(id)}
            className="px-3 py-1 rounded-full text-xs font-semibold transition-all"
            style={
              modeFilter === id
                ? { background: SYSTEM_COLOR, color: '#fff' }
                : { background: SYSTEM_COLOR_MID, color: '#a5b4fc', border: `1px solid ${SYSTEM_COLOR_MID}` }
            }
          >
            {label}
          </button>
        ))}
      </div>

      {notification && (
        <div
          className="rounded-xl px-4 py-3 text-sm font-semibold"
          style={{ background: 'rgba(75,94,252,0.1)', color: '#818cf8', border: `1px solid rgba(75,94,252,0.3)` }}
        >
          ℹ️ {notification}
        </div>
      )}

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {filteredGames.map((game) => (
          <SegaCDGameCard
            key={game.id}
            game={game}
            onHost={setHostGame}
            onQuickMatch={handleQuickMatch}
            openRooms={openRoomsFor(game.id)}
          />
        ))}
      </div>

      {filteredGames.length === 0 && (
        <div className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
          No games found for this filter.
        </div>
      )}

      {!displayName && (
        <p className="text-center text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Set a display name in Settings to host or join rooms.
        </p>
      )}

      {hostGame && (
        <HostRoomModal
          preselectedGameId={hostGame.id}
          onConfirm={(payload, dn) => { createRoom(payload, dn); setHostGame(null); }}
          onClose={() => setHostGame(null)}
        />
      )}
      {showJoin && (
        <JoinRoomModal
          onConfirm={(code, dn) => { joinByCode(code, dn); setShowJoin(false); }}
          onClose={() => setShowJoin(false)}
          initialCode={joinCode}
        />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// SegaCDPage
// ---------------------------------------------------------------------------

export default function SegaCDPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby', label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'setup', label: '⚙️ Setup' },
  ];

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Header */}
      <div className="flex items-center gap-4">
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: `linear-gradient(135deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})` }}
        >
          💿
        </div>
        <div>
          <h1 className="text-3xl font-black text-white">SEGA CD</h1>
          <p className="text-sm mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Also known as the Mega-CD · CD-ROM add-on for the Genesis / Mega Drive · 1992–1996
          </p>
          <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {SEGACD_GAMES.length} games · Relay netplay via RetroArch + Genesis Plus GX · BIOS required
          </p>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: SYSTEM_COLOR_MID }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(75,94,252,0.15)', color: '#818cf8', borderBottom: `2px solid ${SYSTEM_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby' && <LobbyPanel games={SEGACD_GAMES} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
      {activeTab === 'setup' && <SetupGuide />}
    </div>
  );
}
