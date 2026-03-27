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

type ActiveTab = 'lobby' | 'guide' | 'leaderboard';

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
    <div className="bg-gray-800 rounded-lg p-5 mb-4">
      <h3 className="text-base font-semibold text-white mb-3">
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
    { id: 'lobby', label: '🎮 Lobby' },
    { id: 'guide', label: '📖 Guide' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
  ];

  return (
    <div className="p-6 max-w-5xl mx-auto">
      {/* Header */}
      <div className="flex items-center gap-3 mb-6">
        <span className="text-4xl">🎴</span>
        <div>
          <h1 className="text-2xl font-bold" style={{ color: SYSTEM_COLOR }}>
            Nintendo 3DS
          </h1>
          <p className="text-sm text-gray-400">
            Dual-screen handheld gaming with stereoscopic 3D — Lime3DS emulator · Direct connect & relay
          </p>
        </div>
      </div>

      {/* Tabs */}
      <div className="flex gap-2 mb-6">
        {TABS.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className={`px-4 py-2 rounded-lg text-sm font-medium transition-colors ${
              activeTab === tab.id
                ? 'text-white'
                : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
            }`}
            style={activeTab === tab.id ? { backgroundColor: SYSTEM_COLOR } : undefined}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Lobby tab */}
      {activeTab === 'lobby' && (
        <>
          {threeDsRooms.length > 0 && (
            <section className="mb-6">
              <h2 className="text-lg font-semibold text-white mb-3">🔴 Live Rooms</h2>
              <div className="space-y-2">
                {threeDsRooms.map((room) => (
                  <div
                    key={room.id}
                    className="bg-gray-800 rounded-lg p-4 flex items-center justify-between"
                  >
                    <div>
                      <span className="font-medium text-white">{room.name}</span>
                      <span className="ml-2 text-sm text-gray-400">{room.gameTitle}</span>
                    </div>
                    <div className="flex items-center gap-3 text-sm text-gray-400">
                      <span>{room.players.length}/{room.maxPlayers} players</span>
                      <button
                        onClick={() => setShowJoin(true)}
                        className="px-3 py-1 rounded text-white text-xs font-medium"
                        style={{ backgroundColor: SYSTEM_COLOR }}
                      >
                        Join
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            </section>
          )}

          <section>
            <h2 className="text-lg font-semibold text-white mb-3">🎴 3DS Games</h2>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              {THREEDS_GAMES.map((game) => (
                <div key={game.id} className="bg-gray-800 rounded-lg p-4 flex flex-col gap-3">
                  <div className="flex items-start gap-3">
                    <span className="text-3xl">{game.coverEmoji}</span>
                    <div className="flex-1 min-w-0">
                      <p className="font-medium text-white leading-tight">{game.title}</p>
                      <p className="text-xs text-gray-400 mt-0.5 line-clamp-2">{game.description}</p>
                      <div className="flex flex-wrap gap-1 mt-2">
                        {game.badges?.map((b) => (
                          <span key={b} className="text-xs px-1.5 py-0.5 rounded bg-gray-700 text-gray-300">{b}</span>
                        ))}
                        {FOURPLAYER_IDS.has(game.id) && (
                          <span className="text-xs px-1.5 py-0.5 rounded bg-blue-900 text-blue-300">4P Online</span>
                        )}
                        {STREETPASS_IDS.has(game.id) && (
                          <span className="text-xs px-1.5 py-0.5 rounded bg-yellow-900 text-yellow-300">StreetPass</span>
                        )}
                      </div>
                    </div>
                    <span
                      className="text-xs font-bold px-2 py-1 rounded shrink-0"
                      style={{ backgroundColor: SYSTEM_COLOR + '33', color: SYSTEM_COLOR }}
                    >
                      {game.maxPlayers}P
                    </span>
                  </div>
                  <div className="flex gap-2">
                    <button
                      onClick={() => handleQuickMatch(game.id)}
                      className="flex-1 px-3 py-2 rounded text-white text-sm font-medium hover:opacity-90"
                      style={{ backgroundColor: SYSTEM_COLOR }}
                    >
                      Quick Match
                    </button>
                    <button
                      onClick={() => handleHost(game.id)}
                      className="flex-1 px-3 py-2 rounded bg-gray-700 text-white text-sm font-medium hover:bg-gray-600"
                    >
                      Host Room
                    </button>
                  </div>
                </div>
              ))}
            </div>
          </section>
        </>
      )}

      {/* Guide tab */}
      {activeTab === 'guide' && <ThreeDSGuide />}

      {/* Leaderboard tab */}
      {activeTab === 'leaderboard' && (
        <section>
          <h2 className="text-lg font-semibold text-white mb-3">🏆 3DS Leaderboard</h2>
          {rankingsError ? (
            <p className="text-red-400 text-sm">{rankingsError}</p>
          ) : rankings.length === 0 ? (
            <p className="text-gray-400 text-sm">No rankings yet — be the first to play!</p>
          ) : (
            <div className="space-y-2">
              {rankings.map((r, i) => (
                <div key={r.displayName} className="bg-gray-800 rounded-lg p-3 flex items-center gap-3">
                  <RankBadge rank={i + 1} />
                  <span className="flex-1 text-white font-medium">{r.displayName}</span>
                  <span className="text-gray-400 text-sm">{r.sessions} sessions</span>
                </div>
              ))}
            </div>
          )}
        </section>
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
