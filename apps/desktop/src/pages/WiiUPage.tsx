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

const SYSTEM_COLOR = '#009AC7';
const SYSTEM_EMOJI = '🎮';

type ActiveTab = 'lobby' | 'leaderboard';

interface WiiUGame {
  id: string;
  title: string;
  genre: string;
  genreEmoji: string;
  maxPlayers: number;
  asymmetric?: boolean;
  coopOnly?: boolean;
}

interface WiiURanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------

const WIIU_GAMES: WiiUGame[] = [
  { id: 'wiiu-mario-kart-8',            title: 'Mario Kart 8',                      genre: 'Racing',     genreEmoji: '🏎️', maxPlayers: 4 },
  { id: 'wiiu-super-smash-bros-wiiu',   title: 'Super Smash Bros. for Wii U',       genre: 'Fighting',   genreEmoji: '🥊', maxPlayers: 8 },
  { id: 'wiiu-new-super-mario-bros-u',  title: 'New Super Mario Bros. U',           genre: 'Platformer', genreEmoji: '🍄', maxPlayers: 4, coopOnly: true },
  { id: 'wiiu-nintendo-land',           title: 'Nintendo Land',                     genre: 'Party',      genreEmoji: '🎡', maxPlayers: 5, asymmetric: true },
  { id: 'wiiu-splatoon',                title: 'Splatoon',                          genre: 'Shooter',    genreEmoji: '🦑', maxPlayers: 4 },
  { id: 'wiiu-pikmin-3',                title: 'Pikmin 3',                          genre: 'Strategy',   genreEmoji: '🌿', maxPlayers: 2 },
  { id: 'wiiu-mario-party-10',          title: 'Mario Party 10',                    genre: 'Party',      genreEmoji: '🎲', maxPlayers: 5, asymmetric: true },
  { id: 'wiiu-donkey-kong-country-tf',  title: 'Donkey Kong Country: Tropical Freeze', genre: 'Platformer', genreEmoji: '🦍', maxPlayers: 2, coopOnly: true },
  { id: 'wiiu-bayonetta-2',             title: 'Bayonetta 2',                       genre: 'Action',     genreEmoji: '🔮', maxPlayers: 2, coopOnly: true },
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
// Main component
// ---------------------------------------------------------------------------

export default function WiiUPage() {
  const { rooms, playerId } = useLobby();
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [selectedGame, setSelectedGame] = useState<WiiUGame | null>(null);
  const [showHost, setShowHost] = useState(false);
  const [showJoin, setShowJoin] = useState(false);
  const [rankings, setRankings] = useState<WiiURanking[]>([]);
  const [rankingsError, setRankingsError] = useState<string | null>(null);

  // Load leaderboard
  useEffect(() => {
    if (activeTab !== 'leaderboard') return;
    apiFetch<{ rankings: WiiURanking[] }>('/api/rankings/wiiu')
      .then((data) => setRankings(data.rankings ?? []))
      .catch((err: unknown) => setRankingsError(err instanceof Error ? err.message : 'Failed to load leaderboard'));
  }, [activeTab]);

  const handleQuickMatch = useCallback(
    (game: WiiUGame) => {
      setSelectedGame(game);
      setShowJoin(true);
    },
    [],
  );

  const handleHost = useCallback(
    (game: WiiUGame) => {
      setSelectedGame(game);
      setShowHost(true);
    },
    [],
  );

  const hostPayload = useCallback(
    (): CreateRoomPayload => ({
      gameId: selectedGame?.id ?? '',
      gameTitle: selectedGame?.title ?? '',
      system: 'wiiu',
      maxPlayers: selectedGame?.maxPlayers ?? 4,
      isPrivate: false,
      hostDisplayName: playerId ?? 'Player',
    }),
    [selectedGame, playerId],
  );

  const wiiuRooms = rooms.filter((r) => r.system?.toLowerCase() === 'wii u');

  return (
    <div className="p-6 max-w-5xl mx-auto">
      {/* Header */}
      <div className="flex items-center gap-3 mb-6">
        <span className="text-4xl">{SYSTEM_EMOJI}</span>
        <div>
          <h1 className="text-2xl font-bold" style={{ color: SYSTEM_COLOR }}>
            Nintendo Wii U
          </h1>
          <p className="text-sm text-gray-400">
            HD Nintendo gaming with GamePad asymmetry — Cemu emulator
          </p>
        </div>
      </div>

      {/* Tabs */}
      <div className="flex gap-2 mb-6">
        {(['lobby', 'leaderboard'] as ActiveTab[]).map((tab) => (
          <button
            key={tab}
            onClick={() => setActiveTab(tab)}
            className={`px-4 py-2 rounded-lg text-sm font-medium transition-colors ${
              activeTab === tab
                ? 'text-white'
                : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
            }`}
            style={activeTab === tab ? { backgroundColor: SYSTEM_COLOR } : undefined}
          >
            {tab === 'lobby' ? '🎮 Lobby' : '🏆 Leaderboard'}
          </button>
        ))}
      </div>

      {activeTab === 'lobby' && (
        <>
          {/* Active Rooms */}
          {wiiuRooms.length > 0 && (
            <section className="mb-6">
              <h2 className="text-lg font-semibold text-white mb-3">🔴 Live Rooms</h2>
              <div className="space-y-2">
                {wiiuRooms.map((room) => (
                  <div
                    key={room.id}
                    className="bg-gray-800 rounded-lg p-4 flex items-center justify-between"
                  >
                    <div>
                      <span className="font-medium text-white">{room.name}</span>
                      <span className="ml-2 text-sm text-gray-400">{room.gameTitle}</span>
                    </div>
                    <div className="flex items-center gap-3 text-sm text-gray-400">
                      <span>
                        {room.playerCount}/{room.maxPlayers} players
                      </span>
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

          {/* Game Grid */}
          <section>
            <h2 className="text-lg font-semibold text-white mb-3">🎮 Wii U Games</h2>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              {WIIU_GAMES.map((game) => (
                <div
                  key={game.id}
                  className="bg-gray-800 rounded-lg p-4 flex flex-col gap-3"
                >
                  <div className="flex items-start justify-between">
                    <div>
                      <div className="flex items-center gap-2 mb-1">
                        <span className="text-xl">{game.genreEmoji}</span>
                        <span className="font-medium text-white">{game.title}</span>
                      </div>
                      <div className="flex gap-2 flex-wrap text-xs">
                        <span className="px-2 py-0.5 rounded" style={{ backgroundColor: SYSTEM_COLOR + '33', color: SYSTEM_COLOR }}>
                          {game.genre}
                        </span>
                        <span className="px-2 py-0.5 rounded bg-gray-700 text-gray-300">
                          {game.maxPlayers}P
                        </span>
                        {game.asymmetric && (
                          <span className="px-2 py-0.5 rounded bg-purple-900 text-purple-300">
                            Asymmetric
                          </span>
                        )}
                        {game.coopOnly && (
                          <span className="px-2 py-0.5 rounded bg-green-900 text-green-300">
                            Co-op
                          </span>
                        )}
                      </div>
                    </div>
                  </div>
                  <div className="flex gap-2">
                    <button
                      onClick={() => handleQuickMatch(game)}
                      className="flex-1 px-3 py-2 rounded text-white text-sm font-medium hover:opacity-90"
                      style={{ backgroundColor: SYSTEM_COLOR }}
                    >
                      Quick Match
                    </button>
                    <button
                      onClick={() => handleHost(game)}
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

      {activeTab === 'leaderboard' && (
        <section>
          <h2 className="text-lg font-semibold text-white mb-3">🏆 Wii U Leaderboard</h2>
          {rankingsError ? (
            <p className="text-red-400 text-sm">{rankingsError}</p>
          ) : rankings.length === 0 ? (
            <p className="text-gray-400 text-sm">No rankings yet — be the first to play!</p>
          ) : (
            <div className="space-y-2">
              {rankings.map((r, i) => (
                <div
                  key={r.displayName}
                  className="bg-gray-800 rounded-lg p-3 flex items-center gap-3"
                >
                  <RankBadge rank={i + 1} />
                  <span className="flex-1 text-white font-medium">{r.displayName}</span>
                  <span className="text-gray-400 text-sm">{r.sessions} sessions</span>
                </div>
              ))}
            </div>
          )}
        </section>
      )}

      {showHost && selectedGame && (
        <HostRoomModal
          isOpen={showHost}
          onClose={() => setShowHost(false)}
          initialPayload={hostPayload()}
        />
      )}
      {showJoin && (
        <JoinRoomModal
          isOpen={showJoin}
          onClose={() => setShowJoin(false)}
        />
      )}
    </div>
  );
}
