import { useState, useEffect, useCallback } from 'react';
import { useLobby } from '../context/LobbyContext';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';
import { MOCK_GAMES } from '../data/mock-games';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

const SYSTEM_COLOR = '#CC0000';
const SYSTEM_COLOR_DARK = '#880000';

type ActiveTab = 'lobby' | 'leaderboard' | 'guide';

interface ThreeDSRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Derived game list from shared mock catalog
// ---------------------------------------------------------------------------

const THREEDS_GAMES = MOCK_GAMES.filter((g) => g.system === '3DS');

// Games that work well in StreetPass/SpotPass mode
const STREETPASS_IDS = new Set([
  '3ds-tomodachi-life',
  '3ds-fire-emblem-awakening',
  '3ds-animal-crossing-new-leaf',
]);

// Games with strong 4-player multiplayer
const FOURPLAYER_IDS = new Set([
  '3ds-mario-kart-7',
  '3ds-super-smash-bros-for-3ds',
  '3ds-monster-hunter-4-ultimate',
  '3ds-luigis-mansion-dark-moon',
  '3ds-kirby-triple-deluxe',
  '3ds-animal-crossing-new-leaf',
]);

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
// Sub-components
// ---------------------------------------------------------------------------

function RankBadge({ rank }: { rank: number }) {
  const medals: Record<number, string> = { 1: '🥇', 2: '🥈', 3: '🥉' };
  return (
    <span className="text-lg w-8 text-center" title={`Rank #${rank}`}>
      {medals[rank] ?? `#${rank}`}
    </span>
  );
}

function GuideSection({
  title,
  emoji,
  children,
}: {
  title: string;
  emoji: string;
  children: React.ReactNode;
}) {
  return (
    <div
      className="rounded-2xl p-5 mb-4"
      style={{ background: 'rgba(204,0,0,0.05)', border: '1px solid rgba(204,0,0,0.15)' }}
    >
      <h3 className="text-sm font-bold mb-3" style={{ color: SYSTEM_COLOR }}>
        {emoji} {title}
      </h3>
      {children}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Guide Tab
// ---------------------------------------------------------------------------

function ThreeDSGuide() {
  return (
    <div className="space-y-2">
      {/* Banner */}
      <div
        className="rounded-lg p-4 mb-4 text-sm text-white"
        style={{
          background: `linear-gradient(135deg, ${SYSTEM_COLOR}22, ${SYSTEM_COLOR}44)`,
          border: `1px solid ${SYSTEM_COLOR}55`,
        }}
      >
        <p className="font-semibold mb-1">Powered by Lime3DS — the active Citra fork</p>
        <p className="text-gray-300 text-xs">
          Citra was discontinued in March 2024. Lime3DS is the community-maintained fork and receives
          ongoing fixes. Most of the 3DS library runs at full speed with hardware shaders enabled.
        </p>
      </div>

      <GuideSection title="Emulator Setup" emoji="⚙️">
        <ol className="list-decimal list-inside space-y-1 text-sm text-gray-300">
          <li>Install Lime3DS: <code className="text-xs bg-gray-700 px-1 rounded">brew install --cask lime3ds</code> (macOS), Flatpak on Linux, or download from GitHub Releases</li>
          <li>Dump your 3DS games to <code className="text-xs bg-gray-700 px-1 rounded">.3ds</code> (trimmed ROM) format using GodMode9 on real hardware</li>
          <li>For encrypted <code className="text-xs bg-gray-700 px-1 rounded">.cia</code> titles, place <code className="text-xs bg-gray-700 px-1 rounded">aes_keys.txt</code> in Lime3DS's system directory</li>
          <li>Some games require system archives — dump via GodMode9's <span className="text-yellow-300">Dump System Files</span> option</li>
          <li>Enable <strong>hardware shaders</strong> in Lime3DS Graphics settings for full performance</li>
        </ol>
      </GuideSection>

      <GuideSection title="Screen Layout Modes" emoji="📺">
        <div className="grid grid-cols-2 gap-3 text-sm">
          {[
            { mode: 'Default', desc: 'Top (240p) above bottom (240p) — matches hardware layout. Best for Pokémon and RPGs.' },
            { mode: 'Large Screen', desc: 'Top screen enlarged, bottom screen in corner. Best for action games and fighters.' },
            { mode: 'Side-by-Side', desc: 'Both screens displayed horizontally. Good for games that use both screens equally.' },
            { mode: 'Single Screen', desc: 'Top screen only. Use for games where the bottom screen is decorative (e.g. most racing games).' },
          ].map(({ mode, desc }) => (
            <div key={mode} className="bg-gray-700 rounded p-2">
              <p className="text-red-300 text-xs font-medium">{mode}</p>
              <p className="text-gray-400 text-xs mt-1">{desc}</p>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="Multiplayer Setup" emoji="🌐">
        <ul className="space-y-1 text-sm text-gray-300">
          <li><span className="text-red-300 font-medium">RetroOasis relay:</span> TCP relay bridges at network level — host a room and launch Lime3DS normally</li>
          <li><span className="text-red-300 font-medium">Direct connect:</span> Lime3DS GUI → Multiplayer → Direct Connect to host IP (for non-relay sessions)</li>
          <li><span className="text-red-300 font-medium">citra-room server:</span> Run <code className="text-xs bg-gray-700 px-1 rounded">citra-room --port 24872 --name "MyRoom" --max-members 8</code> to host a dedicated server</li>
          <li><span className="text-red-300 font-medium">Latency targets:</span> Under 80 ms for kart/fighting; under 150 ms for Pokémon and turn-based games</li>
          <li><span className="text-red-300 font-medium">Game version:</span> All players must use matching game versions for online compatibility</li>
        </ul>
      </GuideSection>

      <GuideSection title="ROM Formats" emoji="💾">
        <div className="grid grid-cols-2 gap-3 text-sm">
          {[
            { ext: '.3ds', desc: 'Trimmed ROM — most common format; works with all games' },
            { ext: '.cci', desc: 'Raw card image — full size, highest compatibility' },
            { ext: '.cia', desc: 'Installable content archive — requires AES keys file' },
            { ext: '.3dsx', desc: 'Homebrew executable — for unofficial apps' },
          ].map(({ ext, desc }) => (
            <div key={ext} className="bg-gray-700 rounded p-2">
              <code className="text-red-300 text-xs font-mono">{ext}</code>
              <p className="text-gray-400 text-xs mt-1">{desc}</p>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="StreetPass & SpotPass" emoji="📡">
        <p className="text-sm text-gray-300 mb-2">
          StreetPass was the 3DS's signature passive social feature — exchanging data with nearby consoles.
          Lime3DS can simulate StreetPass through its direct-connect multiplayer mode.
        </p>
        <ul className="space-y-1 text-sm text-gray-300">
          <li><span className="text-yellow-300">Tomodachi Life</span> — Trade your Miis and build an island community with friends</li>
          <li><span className="text-yellow-300">Fire Emblem: Awakening</span> — SpotPass delivers free DLC units and opponent teams for the SpotPass Battle Log</li>
          <li><span className="text-yellow-300">Animal Crossing: New Leaf</span> — Visit friends' towns via the Dream Suite or through online island visits</li>
        </ul>
      </GuideSection>

      <GuideSection title="Best Multiplayer Games" emoji="🏆">
        <div className="space-y-2 text-sm text-gray-300">
          {[
            { title: 'Mario Kart 7', badge: '8P Racing', why: 'Hang-gliders + underwater; stable under 80 ms with Lime3DS direct connect' },
            { title: 'Monster Hunter 4 Ultimate', badge: '4P Co-op', why: 'Peak 3DS multiplayer — mounting system makes hunts more dynamic than ever' },
            { title: "Luigi's Mansion: Dark Moon", badge: '4P ScareScraper', why: 'Timed 4-player co-op tower; no other 3DS game quite matches it' },
            { title: 'Super Smash Bros. for 3DS', badge: '4P Fighting', why: 'First portable Smash — full roster, online battles, and Smash Run mode' },
            { title: 'Pokémon X/Y/OR/AS/Sun/Moon', badge: '2P Battle/Trade', why: 'Six games to trade and battle across — PSS makes connecting instant' },
            { title: 'Kirby: Triple Deluxe', badge: '4P Kirby Fighters', why: 'The standalone Kirby Fighters VS mode is a hidden gem for local sessions' },
          ].map(({ title, badge, why }) => (
            <div key={title} className="flex gap-2 items-start">
              <span className="text-red-400 font-medium flex-none">▸</span>
              <div>
                <span className="text-white font-medium">{title}</span>
                <span
                  className="ml-2 text-xs px-1.5 py-0.5 rounded"
                  style={{ background: SYSTEM_COLOR + '33', color: SYSTEM_COLOR }}
                >
                  {badge}
                </span>
                <p className="text-gray-400 text-xs mt-0.5">{why}</p>
              </div>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="Performance Tips" emoji="⚡">
        <ul className="space-y-1 text-sm text-gray-300">
          <li><span className="text-red-300 font-medium">Hardware shaders:</span> Enable in Graphics settings — required for full-speed emulation on most GPUs</li>
          <li><span className="text-red-300 font-medium">Shader cache:</span> Lime3DS pre-compiles and caches shaders between sessions — first launch is slowest</li>
          <li><span className="text-red-300 font-medium">3D effect:</span> Stereoscopic 3D emulation is optional — disable it in Display settings for a small performance gain</li>
          <li><span className="text-red-300 font-medium">Resolution scale:</span> 2× (800×480) or 3× (1200×720) gives a sharp picture without significant overhead</li>
          <li><span className="text-red-300 font-medium">Region:</span> Most games auto-detect; force override in System settings if a title misbehaves</li>
        </ul>
      </GuideSection>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Main component
// ---------------------------------------------------------------------------

export default function ThreeDSPage() {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [selectedGameId, setSelectedGameId] = useState<string | null>(null);
  const [showHost, setShowHost] = useState(false);
  const [showJoin, setShowJoin] = useState(false);
  const [rankings, setRankings] = useState<ThreeDSRanking[]>([]);
  const [rankingsError, setRankingsError] = useState<string | null>(null);

  useEffect(() => {
    if (activeTab !== 'leaderboard') return;
    apiFetch<{ rankings: ThreeDSRanking[] }>('/api/rankings/3ds')
      .then((data) => setRankings(data.rankings ?? []))
      .catch((err: unknown) =>
        setRankingsError(err instanceof Error ? err.message : 'Failed to load leaderboard'),
      );
  }, [activeTab]);

  const handleQuickMatch = useCallback((gameId: string) => {
    setSelectedGameId(gameId);
    setShowJoin(true);
  }, []);

  const handleHost = useCallback((gameId: string) => {
    setSelectedGameId(gameId);
    setShowHost(true);
  }, []);

  const threeDsRooms = publicRooms.filter(
    (r) => r.system?.toLowerCase() === '3ds',
  );

  const TABS: { id: ActiveTab; label: string }[] = [
    { id: 'lobby',       label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'guide',       label: '📖 Guide' },
  ];

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Hero header */}
      <div
        className="rounded-3xl p-6 flex items-center gap-5"
        style={{ background: 'linear-gradient(135deg, rgba(204,0,0,0.18) 0%, rgba(204,0,0,0.05) 60%, transparent 100%)', border: '1px solid rgba(204,0,0,0.2)' }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: `linear-gradient(135deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})` }}
        >
          🎴
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">Nintendo 3DS</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Lime3DS emulator · Stereoscopic 3D · Direct connect &amp; relay
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {THREEDS_GAMES.length} games
          </span>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            8th Generation · 2011
          </span>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(204,0,0,0.15)' }}>
        {TABS.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(204,0,0,0.1)', color: SYSTEM_COLOR, borderBottom: `2px solid ${SYSTEM_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Lobby tab */}
      {activeTab === 'lobby' && (
        <div className="space-y-6">
          {threeDsRooms.length > 0 && (
            <div className="space-y-2">
              <p className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>🔴 Live Rooms</p>
              {threeDsRooms.map((room) => (
                <div
                  key={room.id}
                  className="rounded-xl p-4 flex items-center justify-between"
                  style={{ background: 'rgba(204,0,0,0.06)', border: '1px solid rgba(204,0,0,0.15)' }}
                >
                  <div>
                    <span className="font-medium text-white">{room.name}</span>
                    <span className="ml-2 text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>{room.gameTitle}</span>
                  </div>
                  <div className="flex items-center gap-3 text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    <span>{room.players.length}/{room.maxPlayers} players</span>
                    <button
                      onClick={() => setShowJoin(true)}
                      className="px-3 py-1.5 rounded-xl text-white text-xs font-bold transition-all hover:scale-105"
                      style={{ background: `linear-gradient(90deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})` }}
                    >
                      Join
                    </button>
                  </div>
                </div>
              ))}
            </div>
          )}

          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {THREEDS_GAMES.map((game) => (
              <div
                key={game.id}
                className="n-card rounded-2xl p-4 flex flex-col gap-3"
                style={{ background: 'linear-gradient(135deg, rgba(204,0,0,0.07), rgba(204,0,0,0.02))', border: '1px solid rgba(204,0,0,0.15)' }}
              >
                <div className="flex items-start justify-between gap-2">
                  <div className="flex items-center gap-2">
                    <span className="text-3xl leading-none">{game.coverEmoji}</span>
                    <div className="min-w-0">
                      <p className="text-sm font-bold text-white leading-tight">{game.title}</p>
                      <p className="text-xs mt-0.5 line-clamp-2" style={{ color: 'var(--color-oasis-text-muted)' }}>{game.description}</p>
                    </div>
                  </div>
                  <span
                    className="text-xs font-bold px-2 py-0.5 rounded-full shrink-0"
                    style={{ background: 'rgba(204,0,0,0.15)', color: SYSTEM_COLOR }}
                  >
                    {game.maxPlayers}P
                  </span>
                </div>
                <div className="flex flex-wrap gap-1.5">
                  {game.badges?.map((b) => (
                    <span
                      key={b}
                      className="text-xs px-2 py-0.5 rounded-full"
                      style={{ background: 'rgba(255,255,255,0.06)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.08)' }}
                    >
                      {b}
                    </span>
                  ))}
                  {FOURPLAYER_IDS.has(game.id) && (
                    <span
                      className="text-xs px-2 py-0.5 rounded-full font-semibold"
                      style={{ background: 'rgba(99,102,241,0.12)', color: '#a5b4fc', border: '1px solid rgba(99,102,241,0.3)' }}
                    >
                      4P Online
                    </span>
                  )}
                  {STREETPASS_IDS.has(game.id) && (
                    <span
                      className="text-xs px-2 py-0.5 rounded-full font-semibold"
                      style={{ background: 'rgba(234,179,8,0.12)', color: '#fde68a', border: '1px solid rgba(234,179,8,0.3)' }}
                    >
                      StreetPass
                    </span>
                  )}
                </div>
                {game.maxPlayers > 1 && (
                  <div className="flex gap-2 mt-auto">
                    <button
                      onClick={() => handleQuickMatch(game.id)}
                      className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
                      style={{ background: `linear-gradient(90deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})`, color: '#fff' }}
                    >
                      ⚡ Quick Match
                    </button>
                    <button
                      onClick={() => handleHost(game.id)}
                      className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
                      style={{ background: 'rgba(204,0,0,0.08)', border: '1px solid rgba(204,0,0,0.35)', color: SYSTEM_COLOR }}
                    >
                      🎮 Host Room
                    </button>
                  </div>
                )}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Guide tab */}
      {activeTab === 'guide' && <ThreeDSGuide />}

      {/* Leaderboard tab */}
      {activeTab === 'leaderboard' && (
        <div className="space-y-2 max-w-lg">
          <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Top players ranked by session count
          </p>
          {rankingsError ? (
            <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>{rankingsError}</p>
          ) : rankings.length === 0 ? (
            <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
              <p className="text-4xl mb-3">🏆</p>
              <p className="font-semibold">No rankings yet</p>
              <p className="text-sm mt-1">Play some 3DS sessions to appear here!</p>
            </div>
          ) : (
            rankings.map((r, i) => (
              <div
                key={r.displayName}
                className="flex items-center gap-3 rounded-xl px-4 py-3"
                style={{ background: 'rgba(204,0,0,0.05)', border: '1px solid rgba(204,0,0,0.12)' }}
              >
                <RankBadge rank={i + 1} />
                <span className="flex-1 text-sm font-semibold text-white">{r.displayName}</span>
                <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{r.sessions} sessions</span>
              </div>
            ))
          )}
        </div>
      )}

      {showHost && selectedGameId && (
        <HostRoomModal
          preselectedGameId={selectedGameId}
          onConfirm={(payload, dn) => { createRoom(payload, dn); setShowHost(false); }}
          onClose={() => setShowHost(false)}
        />
      )}
      {showJoin && (
        <JoinRoomModal
          onConfirm={(code, dn) => { joinByCode(code, dn); setShowJoin(false); }}
          onClose={() => setShowJoin(false)}
        />
      )}
    </div>
  );
}
