import { useState, useEffect, useCallback, useMemo } from 'react';
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

const SYSTEM_COLOR = '#003087';
const SYSTEM_COLOR_DARK = '#001a4d';
const SYSTEM_COLOR_MID = 'rgba(0,48,135,0.18)';

type ActiveTab = 'lobby' | 'leaderboard' | 'guide';
type GenreFilter = 'all' | 'Fighting' | 'Action' | 'Racing' | 'Sports' | 'Platformer' | 'Shooter' | 'Beat-em-up';

interface PS3Game {
  id: string;
  title: string;
  genre: string;
  genreEmoji: string;
  maxPlayers: number;
  singleOnly?: boolean;
  competitive?: boolean;
  coopOnly?: boolean;
  rpcn?: boolean;
  year?: number;
}

interface PS3Ranking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------

const PS3_GAMES: PS3Game[] = [
  // Fighting
  { id: 'ps3-street-fighter-iv',                  title: 'Street Fighter IV',                 genre: 'Fighting',    genreEmoji: '👊', maxPlayers: 2, competitive: true, rpcn: true,  year: 2009 },
  { id: 'ps3-tekken-6',                            title: 'Tekken 6',                          genre: 'Fighting',    genreEmoji: '🥋', maxPlayers: 2, competitive: true, rpcn: true,  year: 2009 },
  { id: 'ps3-mortal-kombat-2011',                  title: 'Mortal Kombat (2011)',               genre: 'Fighting',    genreEmoji: '🩸', maxPlayers: 2, competitive: true, rpcn: true,  year: 2011 },
  { id: 'ps3-blazblue-calamity-trigger',           title: 'BlazBlue: Calamity Trigger',        genre: 'Fighting',    genreEmoji: '⚡', maxPlayers: 2, competitive: true, rpcn: true,  year: 2009 },
  // Action
  { id: 'ps3-god-of-war-iii',                      title: 'God of War III',                    genre: 'Action',      genreEmoji: '⚔️', maxPlayers: 1, singleOnly: true,              year: 2010 },
  { id: 'ps3-uncharted-2',                         title: 'Uncharted 2: Among Thieves',        genre: 'Action',      genreEmoji: '🗺️', maxPlayers: 10, competitive: true, rpcn: true, year: 2009 },
  { id: 'ps3-metal-gear-solid-4',                  title: 'Metal Gear Solid 4',                genre: 'Action',      genreEmoji: '🎖️', maxPlayers: 1, singleOnly: true,              year: 2008 },
  // Beat-em-up
  { id: 'ps3-castle-crashers',                     title: 'Castle Crashers',                   genre: 'Beat-em-up',  genreEmoji: '🏰', maxPlayers: 4, coopOnly: true,   rpcn: true,  year: 2010 },
  { id: 'ps3-scott-pilgrim-vs-world',              title: 'Scott Pilgrim vs. the World',       genre: 'Beat-em-up',  genreEmoji: '🎮', maxPlayers: 4, coopOnly: true,   rpcn: true,  year: 2010 },
  { id: 'ps3-the-warriors-rock',                   title: 'Warriors: Rock',                    genre: 'Beat-em-up',  genreEmoji: '🥊', maxPlayers: 2, coopOnly: true,   rpcn: true,  year: 2011 },
  // Shooter
  { id: 'ps3-killzone-2',                          title: 'Killzone 2',                        genre: 'Shooter',     genreEmoji: '🔫', maxPlayers: 32, competitive: true, rpcn: true, year: 2009 },
  { id: 'ps3-resistance-fall-of-man',              title: 'Resistance: Fall of Man',           genre: 'Shooter',     genreEmoji: '💥', maxPlayers: 40, competitive: true, rpcn: true, year: 2006 },
  { id: 'ps3-borderlands',                         title: 'Borderlands',                       genre: 'Shooter',     genreEmoji: '🌵', maxPlayers: 4, coopOnly: true,   rpcn: true,  year: 2009 },
  // Platformer
  { id: 'ps3-little-big-planet',                   title: 'LittleBigPlanet',                   genre: 'Platformer',  genreEmoji: '🧸', maxPlayers: 4, coopOnly: true,   rpcn: true,  year: 2008 },
  { id: 'ps3-ratchet-clank-full-frontal-assault',  title: 'Ratchet & Clank: Full Frontal Assault', genre: 'Platformer', genreEmoji: '🔧', maxPlayers: 4, competitive: true, rpcn: true, year: 2012 },
  // Racing
  { id: 'ps3-gran-turismo-5',                      title: 'Gran Turismo 5',                    genre: 'Racing',      genreEmoji: '🏎️', maxPlayers: 16, competitive: true, rpcn: true, year: 2010 },
  { id: 'ps3-wipeout-hd',                          title: 'WipEout HD',                        genre: 'Racing',      genreEmoji: '🚀', maxPlayers: 8, competitive: true, rpcn: true,  year: 2008 },
  // Sports
  { id: 'ps3-nba-2k11',                            title: 'NBA 2K11',                          genre: 'Sports',      genreEmoji: '🏀', maxPlayers: 10, competitive: true, rpcn: true, year: 2010 },
  { id: 'ps3-nfl-blitz',                           title: 'NFL Blitz',                         genre: 'Sports',      genreEmoji: '🏈', maxPlayers: 4, competitive: true, rpcn: true,  year: 2012 },
  { id: 'ps3-tekken-tag-tournament-2',             title: 'Tekken Tag Tournament 2',           genre: 'Fighting',    genreEmoji: '🥋', maxPlayers: 4, competitive: true, rpcn: true,  year: 2012 },
];

const ALL_GENRES: GenreFilter[] = ['all', 'Fighting', 'Action', 'Shooter', 'Beat-em-up', 'Platformer', 'Racing', 'Sports'];

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

function PS3Banner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ backgroundColor: 'rgba(0,48,135,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
    >
      <span className="text-xl mt-0.5">🔵</span>
      <div className="space-y-1">
        <p className="text-sm font-bold" style={{ color: '#93c5fd' }}>
          Sony PlayStation 3 · RPCS3 Emulator + RPCN Online
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Sessions use RPCS3 with RPCN (fan-run PSN replacement) for online multiplayer. ROM folders (decrypted disc dumps) or{' '}
          <code className="mx-0.5 px-1 rounded" style={{ background: SYSTEM_COLOR_MID, color: '#93c5fd' }}>
            .pkg
          </code>{' '}
          files are supported. Requires PS3 firmware installed via{' '}
          <code className="mx-0.5 px-1 rounded" style={{ background: SYSTEM_COLOR_MID, color: '#93c5fd' }}>
            File → Install Firmware
          </code>{' '}
          in RPCS3.
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          For RPCN games, create a free account at rpcs3.net/rpcn and enable Network settings in RPCS3.
        </p>
        <p className="text-xs font-semibold" style={{ color: '#fbbf24' }}>
          ℹ️ RetroOasis relay sessions available as fallback — RPCN preferred for marked titles.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Genre filter bar
// ---------------------------------------------------------------------------

function GenreFilterBar({
  active,
  onChange,
  counts,
}: {
  active: GenreFilter;
  onChange: (g: GenreFilter) => void;
  counts: Record<string, number>;
}) {
  return (
    <div className="flex flex-wrap gap-2">
      {ALL_GENRES.map((g) => {
        const count = g === 'all' ? PS3_GAMES.length : (counts[g] ?? 0);
        const isActive = active === g;
        return (
          <button
            key={g}
            onClick={() => onChange(g)}
            className="px-3 py-1 rounded-full text-xs font-semibold transition-all"
            style={
              isActive
                ? { background: SYSTEM_COLOR, color: '#fff' }
                : { background: SYSTEM_COLOR_MID, color: '#93c5fd', border: `1px solid ${SYSTEM_COLOR_MID}` }
            }
          >
            {g === 'all' ? 'All' : g} {count > 0 && <span className="opacity-70">({count})</span>}
          </button>
        );
      })}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function PS3GameCard({
  game,
  onHost,
  onQuickMatch,
  openRooms,
}: {
  game: PS3Game;
  onHost: (game: PS3Game) => void;
  onQuickMatch: (game: PS3Game) => void;
  openRooms: number;
}) {
  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{
        background: `linear-gradient(135deg, rgba(0,48,135,0.12), rgba(0,26,77,0.06))`,
        border: `1px solid ${SYSTEM_COLOR_MID}`,
      }}
    >
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-2 min-w-0">
          <span className="text-2xl shrink-0">{game.genreEmoji}</span>
          <div className="min-w-0">
            <p className="text-sm font-bold text-white leading-tight truncate">{game.title}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.genre} · {game.singleOnly ? '1P' : `Up to ${game.maxPlayers}P`}
              {game.year && ` · ${game.year}`}
            </p>
          </div>
        </div>
        <div className="flex flex-col items-end gap-1 shrink-0">
          {game.singleOnly && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(148,163,184,0.1)', color: '#94a3b8', border: '1px solid rgba(148,163,184,0.25)' }}
            >
              Single Player
            </span>
          )}
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
          {game.rpcn && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(0,48,135,0.2)', color: '#93c5fd', border: `1px solid ${SYSTEM_COLOR_MID}` }}
            >
              RPCN
            </span>
          )}
          {openRooms > 0 && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(96,165,250,0.1)', color: '#60a5fa', border: '1px solid rgba(96,165,250,0.25)' }}
            >
              {openRooms} open
            </span>
          )}
        </div>
      </div>

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
          style={{ background: SYSTEM_COLOR_MID, border: `1px solid rgba(0,48,135,0.35)`, color: '#93c5fd' }}
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
  const [rankings, setRankings] = useState<PS3Ranking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: PS3Ranking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">🔵</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">Play some PlayStation 3 sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(0,48,135,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
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

function LobbyPanel({ games }: { games: PS3Game[] }) {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';
  const [hostGame, setHostGame] = useState<PS3Game | null>(null);
  const [showJoin, setShowJoin] = useState(false);
  const [joinCode, setJoinCode] = useState('');
  const [notification, setNotification] = useState('');
  const [genreFilter, setGenreFilter] = useState<GenreFilter>('all');

  const genreCounts = useMemo(() => {
    const counts: Record<string, number> = {};
    for (const g of games) {
      counts[g.genre] = (counts[g.genre] ?? 0) + 1;
    }
    return counts;
  }, [games]);

  const filteredGames = useMemo(
    () => (genreFilter === 'all' ? games : games.filter((g) => g.genre === genreFilter)),
    [games, genreFilter],
  );

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const totalOpenRooms = useMemo(
    () => publicRooms.filter((r) => r.status === 'waiting' && games.some((g) => g.id === r.gameId)).length,
    [publicRooms, games],
  );

  const handleQuickMatch = useCallback(
    async (game: PS3Game) => {
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
      <PS3Banner />

      {/* Stats row */}
      <div className="flex gap-3 flex-wrap">
        {[
          { label: 'Games', value: games.length },
          { label: 'Open Rooms', value: totalOpenRooms },
          { label: 'Players Online', value: publicRooms.filter((r) => r.status === 'waiting').reduce((n, r) => n + (r.players?.length ?? 0), 0) },
        ].map(({ label, value }) => (
          <div
            key={label}
            className="flex-1 min-w-[80px] rounded-xl px-3 py-2 text-center"
            style={{ background: 'rgba(0,48,135,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
          >
            <p className="text-lg font-black" style={{ color: '#93c5fd' }}>{value}</p>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{label}</p>
          </div>
        ))}
      </div>

      {/* Genre filter */}
      <GenreFilterBar active={genreFilter} onChange={setGenreFilter} counts={genreCounts} />

      {notification && (
        <div
          className="rounded-xl px-4 py-3 text-sm font-semibold"
          style={{ background: 'rgba(0,48,135,0.1)', color: '#93c5fd', border: `1px solid rgba(0,48,135,0.3)` }}
        >
          ℹ️ {notification}
        </div>
      )}

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {filteredGames.map((game) => (
          <PS3GameCard
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
          No games found for this genre filter.
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
// Guide panel
// ---------------------------------------------------------------------------

function PS3GuidePanel() {
  return (
    <div className="space-y-6 max-w-2xl">
      <section>
        <h3 className="text-base font-bold text-white mb-2">🖥️ Emulator Backends</h3>
        <div className="space-y-2 text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <p><span className="font-semibold text-white">RPCS3</span> — Primary backend. High compatibility with most PS3 titles. Uses Vulkan for best performance.</p>
          <p>Download RPCS3: <span className="font-mono text-blue-400">rpcs3.net</span></p>
          <p>Install (Linux): <span className="font-mono text-blue-400">flatpak install flathub net.rpcs3.RPCS3</span></p>
        </div>
      </section>
      <section>
        <h3 className="text-base font-bold text-white mb-2">🔑 RPCN Setup</h3>
        <div className="space-y-1 text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <p>RPCN is a free fan-run PSN replacement for RPCS3.</p>
          <p>Create a free account at <span className="font-mono text-blue-400">rpcs3.net/rpcn</span></p>
          <p>Enable RPCN in RPCS3: <span className="font-mono">Network → Network Status: Connected → RPCN</span></p>
          <p>All RetroOasis PS3 sessions route through RPCN relay — no port forwarding needed.</p>
        </div>
      </section>
      <section>
        <h3 className="text-base font-bold text-white mb-2">📀 ROM Formats</h3>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Supported: <span className="font-mono text-white">PS3 ISO folder</span>, <span className="font-mono text-white">.pkg</span> files, decrypted game folders
        </p>
      </section>
      <section>
        <h3 className="text-base font-bold text-white mb-2">🎮 Controller Setup</h3>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          DualShock 3 via USB or DS4 via DS4Windows. Configure in RPCS3 Pad Settings.
        </p>
      </section>
      <section>
        <h3 className="text-base font-bold text-white mb-2">🌐 Netplay Tips</h3>
        <ul className="space-y-1 text-xs list-disc list-inside" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <li>RPCS3 RPCN handles relay — no port forwarding needed.</li>
          <li>Fighting games: target ≤ 60 ms for competitive sessions.</li>
          <li>Co-op games: 120 ms is acceptable for most titles.</li>
          <li>Check rpcs3.net/compatibility before starting — some titles have known issues.</li>
        </ul>
      </section>
    </div>
  );
}

// ---------------------------------------------------------------------------
// PS3Page
// ---------------------------------------------------------------------------

export default function PS3Page() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby', label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'guide', label: '📖 Guide' },
  ];

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Hero header */}
      <div
        className="rounded-3xl p-6 flex items-center gap-5"
        style={{ background: `linear-gradient(135deg, rgba(0,48,135,0.18) 0%, rgba(0,48,135,0.05) 60%, transparent 100%)`, border: `1px solid ${SYSTEM_COLOR_MID}` }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: `linear-gradient(135deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})` }}
        >
          🔵
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">PlayStation 3</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            RPCS3 emulator · RPCN online · Up to 40 players · No port forwarding needed
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {PS3_GAMES.length} games
          </span>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            7th Generation · 2006
          </span>
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
                ? { background: 'rgba(0,48,135,0.15)', color: '#93c5fd', borderBottom: `2px solid ${SYSTEM_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby' && <LobbyPanel games={PS3_GAMES} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
      {activeTab === 'guide' && <PS3GuidePanel />}
    </div>
  );
}
