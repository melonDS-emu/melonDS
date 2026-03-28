import { useState, useEffect, useCallback } from 'react';
import { Link } from 'react-router-dom';
import { useNavigate } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';
import type { MockGame } from '../data/mock-games';
import { MOCK_GAMES } from '../data/mock-games';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const NDS_GAMES = MOCK_GAMES.filter((g) => g.system === 'NDS' && !g.isDsiWare);
const NDS_COLOR = '#C45200';
const NDS_COLOR_DARK = '#8b3a00';

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

type ActiveTab = 'lobby' | 'leaderboard' | 'guide';

interface NdsRanking { displayName: string; sessions: number; }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

async function apiFetch<T>(path: string): Promise<T> {
  const res = await fetch(`${API}${path}`, { headers: { 'Content-Type': 'application/json' } });
  if (!res.ok) throw new Error('Request failed');
  return res.json() as Promise<T>;
}

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

function NDSBanner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ background: 'rgba(196,82,0,0.08)', border: '1px solid rgba(196,82,0,0.25)' }}
    >
      <span className="text-xl mt-0.5">🎮</span>
      <div>
        <p className="text-sm font-bold" style={{ color: NDS_COLOR }}>
          melonDS Emulator · Wi-Fi Relay + WFC Support
        </p>
        <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          melonDS emulates DS Wi-Fi and WFC over TCP relay. Supports{' '}
          <span className="font-semibold text-white">Wiimmfi DNS</span> for Pokémon and Nintendo online titles.
          Touch controls via mouse. DeSmuME available as fallback.
          Automatic <span className="font-semibold text-white">.nds / .dsi</span> ROM detection.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function NDSGameCard({
  game,
  openRooms,
  onHost,
  onQuickMatch,
}: {
  game: MockGame;
  openRooms: number;
  onHost: (game: MockGame) => void;
  onQuickMatch: (game: MockGame) => void;
}) {
  const multiPlayer = game.maxPlayers > 1;

  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{ background: 'linear-gradient(135deg, rgba(196,82,0,0.07), rgba(196,82,0,0.02))', border: '1px solid rgba(196,82,0,0.15)' }}
    >
      {/* Header row */}
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-3">
          <span className="text-3xl leading-none">{game.coverEmoji}</span>
          <div>
            <p className="text-sm font-bold text-white leading-tight">{game.title}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.maxPlayers === 1 ? 'Single Player' : `Up to ${game.maxPlayers}P`}
            </p>
          </div>
        </div>
        <div className="flex flex-col items-end gap-1 shrink-0">
          {openRooms > 0 && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(0,209,112,0.12)', color: '#00d170', border: '1px solid rgba(0,209,112,0.3)' }}
            >
              {openRooms} open
            </span>
          )}
        </div>
      </div>

      {/* Description */}
      <p className="text-xs leading-relaxed" style={{ color: 'var(--color-oasis-text-muted)' }}>
        {game.description}
      </p>

      {/* Tags */}
      <div className="flex flex-wrap gap-1.5">
        {game.tags.map((t) => (
          <span
            key={t}
            className="text-xs px-2 py-0.5 rounded-full"
            style={{ background: 'rgba(255,255,255,0.06)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.08)' }}
          >
            {t}
          </span>
        ))}
        {game.badges.map((b) => (
          <span
            key={b}
            className="text-xs px-2 py-0.5 rounded-full font-semibold"
            style={{ background: 'rgba(196,82,0,0.12)', color: '#fb923c', border: '1px solid rgba(196,82,0,0.25)' }}
          >
            {b}
          </span>
        ))}
      </div>

      {/* Actions */}
      {multiPlayer && (
        <div className="flex gap-2 mt-auto">
          <button
            onClick={() => onQuickMatch(game)}
            className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
            style={{ background: `linear-gradient(90deg, ${NDS_COLOR}, ${NDS_COLOR_DARK})`, color: '#fff' }}
          >
            ⚡ Quick Match
          </button>
          <button
            onClick={() => onHost(game)}
            className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
            style={{ background: 'rgba(196,82,0,0.08)', border: `1px solid rgba(196,82,0,0.35)`, color: NDS_COLOR }}
          >
            🎮 Host Room
          </button>
        </div>
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Lobby panel
// ---------------------------------------------------------------------------

function LobbyPanel({ onJoin }: { onJoin: (roomCode: string) => void }) {
  const { publicRooms, createRoom } = useLobby();
  const [hostGame, setHostGame] = useState<MockGame | null>(null);

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const handleQuickMatch = useCallback(async (game: MockGame) => {
    const open = publicRooms.find((r) => r.gameId === game.id && r.status === 'waiting');
    if (open) { onJoin(open.roomCode); return; }
    try {
      const data = await apiFetch<{ rooms?: Array<{ roomCode: string }> }>(`/api/rooms?gameId=${encodeURIComponent(game.id)}`);
      const rooms = data.rooms ?? [];
      if (rooms.length > 0) { onJoin(rooms[0].roomCode); } else { setHostGame(game); }
    } catch {
      setHostGame(game);
    }
  }, [publicRooms, onJoin]);

  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';

  return (
    <div className="space-y-6">
      <NDSBanner />

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {NDS_GAMES.map((game) => (
          <NDSGameCard
            key={game.id}
            game={game}
            openRooms={openRoomsFor(game.id)}
            onHost={setHostGame}
            onQuickMatch={handleQuickMatch}
          />
        ))}
      </div>

      {!displayName && (
        <p className="text-center text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Set a display name in{' '}
          <Link to="/settings" className="underline" style={{ color: NDS_COLOR }}>
            Settings
          </Link>{' '}
          to host or join rooms.
        </p>
      )}

      {hostGame && (
        <HostRoomModal
          preselectedGameId={hostGame.id}
          onConfirm={(payload, dn) => { createRoom(payload, dn); setHostGame(null); }}
          onClose={() => setHostGame(null)}
        />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Leaderboard panel
// ---------------------------------------------------------------------------

function LeaderboardPanel() {
  const [rankings, setRankings] = useState<NdsRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: NdsRanking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">🎮</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">Play some DS sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2 max-w-lg">
      <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
        Top players ranked by session count
      </p>
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(196,82,0,0.05)', border: '1px solid rgba(196,82,0,0.12)' }}
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
// Guide panel
// ---------------------------------------------------------------------------

function GuideSection({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <div
      className="rounded-2xl p-5 space-y-3"
      style={{ background: 'rgba(196,82,0,0.05)', border: '1px solid rgba(196,82,0,0.15)' }}
    >
      <h3 className="text-sm font-bold" style={{ color: NDS_COLOR }}>{title}</h3>
      {children}
    </div>
  );
}

function GuideRow({ label, value, note }: { label: string; value: string; note?: string }) {
  return (
    <div className="flex gap-3 text-xs py-1.5" style={{ borderBottom: '1px solid rgba(196,82,0,0.08)' }}>
      <span className="min-w-[140px]" style={{ color: 'var(--color-oasis-text-muted)' }}>{label}</span>
      <div>
        <span className="font-semibold text-white">{value}</span>
        {note && <span className="ml-2" style={{ color: 'var(--color-oasis-text-muted)' }}>{note}</span>}
      </div>
    </div>
  );
}

function GuidePanel() {
  return (
    <div className="space-y-4 max-w-2xl">
      <GuideSection title="🖥️ Emulator Backends">
        <GuideRow label="melonDS"       value="Default — best accuracy"  note="Native Wi-Fi emulation; ad-hoc & WFC relay support" />
        <GuideRow label="DeSmuME"       value="Compatibility fallback"    note="Older but broadly compatible; no Wi-Fi by default" />
        <GuideRow label="RetroArch"     value="libretro melonDS core"     note="Same accuracy as standalone via RetroArch netplay" />
        <p className="text-xs pt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Select your backend in <strong className="text-white">Settings → Emulator Paths</strong>. melonDS is strongly recommended for online play.
        </p>
      </GuideSection>

      <GuideSection title="📡 Wi-Fi & WFC Setup">
        <GuideRow label="melonDS Wi-Fi"  value="Direct mode"    note="Emulates DS wireless adapter; works for local sessions" />
        <GuideRow label="Wiimmfi DNS"    value="Custom DNS"     note="Set DNS to 95.217.77.151 in game Wi-Fi settings" />
        <GuideRow label="WFC relay"      value="RetroOasis TCP" note="All internet sessions routed via relay server" />
        <p className="text-xs pt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          For Pokémon Diamond/Pearl/HeartGold/SoulSilver trade and battle, use Wiimmfi DNS. RetroOasis handles connectivity — no router changes needed.
        </p>
      </GuideSection>

      <GuideSection title="💾 ROM Formats">
        <GuideRow label=".nds" value="Standard cartridge dump"  note="Most common format — use this" />
        <GuideRow label=".dsi" value="DSi-enhanced cartridge"   note="Requires DSi firmware in melonDS" />
        <GuideRow label=".srl" value="SDK ROM format"           note="Developer / SDK format; same as .nds internally" />
      </GuideSection>

      <GuideSection title="🎮 Touch Controls">
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          The DS lower screen is emulated via mouse cursor. In melonDS, left-click = touch. You can configure a separate touch overlay window or use a drawing tablet for the most authentic experience.
          <br /><br />
          Some games (Nintendogs, Cooking Mama) rely heavily on touch — these are best played with a drawing tablet or touchscreen monitor.
        </p>
      </GuideSection>

      <GuideSection title="🌐 Netplay Tips">
        <GuideRow label="Turn-based"     value="up to 200 ms"  note="Pokémon battles, chess games" />
        <GuideRow label="Action"         value="< 100 ms"      note="Mario Kart DS, Metroid Prime Hunters" />
        <GuideRow label="Party games"    value="60–120 ms"     note="Mario Party DS, WarioWare" />
        <p className="text-xs pt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          DS netplay is relay-based — all players connect to the RetroOasis server. Wired Ethernet recommended for the host.
        </p>
      </GuideSection>
    </div>
  );
}

// ---------------------------------------------------------------------------
// NDSPage
// ---------------------------------------------------------------------------

export default function NDSPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [joinCode, setJoinCode] = useState<string | null>(null);
  const { joinByCode, currentRoom } = useLobby();
  const navigate = useNavigate();

  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby',       label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'guide',       label: '📖 Guide' },
  ];

  const multiplayerCount = NDS_GAMES.filter((g) => g.maxPlayers > 1).length;

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Hero header */}
      <div
        className="rounded-3xl p-6 flex items-center gap-5"
        style={{ background: 'linear-gradient(135deg, rgba(196,82,0,0.18) 0%, rgba(196,82,0,0.05) 60%, transparent 100%)', border: '1px solid rgba(196,82,0,0.2)' }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: `linear-gradient(135deg, ${NDS_COLOR}, ${NDS_COLOR_DARK})` }}
        >
          🎮
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">Nintendo DS</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            melonDS · Wi-Fi relay + WFC support · Up to 8-player online
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <div className="flex items-center gap-2">
            <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {NDS_GAMES.length} games
            </span>
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(196,82,0,0.15)', color: NDS_COLOR }}
            >
              {multiplayerCount} multiplayer
            </span>
          </div>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            7th Generation · 2004
          </span>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(196,82,0,0.15)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(196,82,0,0.1)', color: NDS_COLOR, borderBottom: `2px solid ${NDS_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby'       && <LobbyPanel onJoin={(code) => setJoinCode(code)} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
      {activeTab === 'guide'       && <GuidePanel />}

      {joinCode && (
        <JoinRoomModal
          initialCode={joinCode}
          onConfirm={(code, name) => { joinByCode(code, name); setJoinCode(null); }}
          onClose={() => setJoinCode(null)}
        />
      )}
    </div>
  );
}
