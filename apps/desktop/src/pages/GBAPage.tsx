import { useState, useEffect, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';
import type { MockGame } from '../data/mock-games';
import { MOCK_GAMES } from '../data/mock-games';
import { JoinRoomModal } from '../components/JoinRoomModal';

const LINK_GAMES = MOCK_GAMES.filter((g) => g.system === 'GB' || g.system === 'GBC' || g.system === 'GBA');

const SERVER_URL =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

interface GbaRanking { displayName: string; sessions: number; }
type ActiveTab = 'lobby' | 'leaderboard';
type SystemFilter = 'all' | 'GB' | 'GBC' | 'GBA';

function RankBadge({ rank }: { rank: number }) {
  if (rank === 1) return <span style={{ fontSize: 18 }}>🥇</span>;
  if (rank === 2) return <span style={{ fontSize: 18 }}>🥈</span>;
  if (rank === 3) return <span style={{ fontSize: 18 }}>🥉</span>;
  return <span style={{ display: 'inline-flex', alignItems: 'center', justifyContent: 'center', width: 24, height: 24, borderRadius: '50%', background: '#333', color: '#ccc', fontSize: 12, fontWeight: 700 }}>{rank}</span>;
}

function GBABanner() {
  return (
    <div style={{ background: 'linear-gradient(135deg, #4B0082 0%, #2d0060 100%)', borderRadius: 12, padding: '16px 20px', marginBottom: 20, color: '#fff', fontSize: 13 }}>
      <strong>🎮 Game Boy / GBC / GBA · mGBA Emulator</strong>
      <span style={{ margin: '0 8px', opacity: 0.6 }}>·</span>
      <span>Link cable over TCP relay</span>
      <span style={{ margin: '0 8px', opacity: 0.6 }}>·</span>
      <span>Automatic .gb / .gbc / .gba ROM detection</span>
      <p style={{ marginTop: 6, marginBottom: 0, opacity: 0.85, fontSize: 12 }}>
        mGBA emulates the Game Boy link cable over TCP for trading and battling. SameBoy (GB/GBC) and VisualBoyAdvance-M are available as fallbacks.
      </p>
    </div>
  );
}

function GBAGameCard({ game, openRooms, onQuickMatch, onHostRoom }: { game: MockGame; openRooms: number; onQuickMatch: (g: MockGame) => void; onHostRoom: (g: MockGame) => void }) {
  const systemBadgeColor = game.system === 'GB' ? '#8B956D' : game.system === 'GBC' ? '#6A5ACD' : '#4B0082';
  return (
    <div style={{ background: '#1a1a1a', borderRadius: 10, padding: 16, display: 'flex', flexDirection: 'column', gap: 8, border: '1px solid #2a2a2a' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
        <span style={{ fontSize: 32 }}>{game.coverEmoji}</span>
        <div>
          <div style={{ fontWeight: 700, fontSize: 14 }}>{game.title}</div>
          <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
            <span style={{ background: systemBadgeColor, borderRadius: 4, padding: '1px 6px', fontSize: 10, color: '#fff', fontWeight: 700 }}>{game.system}</span>
            <span style={{ color: '#888', fontSize: 12 }}>{game.maxPlayers === 1 ? '1 Player' : `Up to ${game.maxPlayers} players`}</span>
          </div>
        </div>
      </div>
      <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
        {game.tags.map((t) => (
          <span key={t} style={{ background: '#2a2a2a', borderRadius: 4, padding: '2px 6px', fontSize: 11, color: '#aaa' }}>{t}</span>
        ))}
        {openRooms > 0 && <span style={{ background: '#1a0a3a', borderRadius: 4, padding: '2px 6px', fontSize: 11, color: '#9575cd' }}>{openRooms} open</span>}
      </div>
      <div style={{ color: '#888', fontSize: 12, lineHeight: 1.4 }}>{game.description}</div>
      {game.maxPlayers > 1 && (
        <div style={{ display: 'flex', gap: 8, marginTop: 4 }}>
          <button onClick={() => onQuickMatch(game)} style={{ flex: 1, padding: '8px 0', borderRadius: 6, border: 'none', background: 'linear-gradient(135deg, #4B0082, #2d0060)', color: '#fff', fontWeight: 700, fontSize: 13, cursor: 'pointer' }}>⚡ Quick Match</button>
          <button onClick={() => onHostRoom(game)} style={{ flex: 1, padding: '8px 0', borderRadius: 6, border: '1px solid #4B0082', background: 'transparent', color: '#9575cd', fontWeight: 700, fontSize: 13, cursor: 'pointer' }}>🎮 Host Room</button>
        </div>
      )}
    </div>
  );
}

function LeaderboardPanel() {
  const [rankings, setRankings] = useState<GbaRanking[]>([]);
  useEffect(() => {
    fetch(`${SERVER_URL}/api/leaderboard?orderBy=sessions&limit=10`)
      .then((r) => r.json())
      .then((data: { leaderboard: GbaRanking[] }) => setRankings(data.leaderboard ?? []))
      .catch(() => {});
  }, []);
  return (
    <div style={{ maxWidth: 500 }}>
      <h3 style={{ margin: '0 0 16px', fontSize: 16, color: '#9575cd' }}>🏆 Top GB/GBC/GBA Players</h3>
      {rankings.length === 0 && <p style={{ color: '#666', fontSize: 13 }}>No rankings yet. Be the first to play!</p>}
      {rankings.map((r, i) => (
        <div key={r.displayName} style={{ display: 'flex', alignItems: 'center', gap: 12, padding: '10px 0', borderBottom: '1px solid #1a1a1a' }}>
          <RankBadge rank={i + 1} />
          <span style={{ flex: 1, fontWeight: 600 }}>{r.displayName}</span>
          <span style={{ color: '#888', fontSize: 13 }}>{r.sessions} sessions</span>
        </div>
      ))}
    </div>
  );
}

function LobbyPanel({ onJoin }: { onJoin: (roomCode: string) => void }) {
  const { createRoom } = useLobby();
  const [filter, setFilter] = useState<SystemFilter>('all');

  const filtered = filter === 'all' ? LINK_GAMES : LINK_GAMES.filter((g) => g.system === filter);

  const handleQuickMatch = useCallback(async (game: MockGame) => {
    const displayName = localStorage.getItem('retro-oasis-display-name') ?? 'Player';
    try {
      const res = await fetch(`${SERVER_URL}/api/rooms?gameId=${encodeURIComponent(game.id)}`);
      const data = (await res.json()) as { rooms?: Array<{ roomCode: string }> };
      const rooms = data.rooms ?? [];
      if (rooms.length > 0) {
        onJoin(rooms[0].roomCode);
      } else {
        createRoom({ name: `${game.title} Room`, gameId: game.id, gameTitle: game.title, system: game.system, isPublic: true, maxPlayers: game.maxPlayers }, displayName);
      }
    } catch {
      createRoom({ name: `${game.title} Room`, gameId: game.id, gameTitle: game.title, system: game.system, isPublic: true, maxPlayers: game.maxPlayers }, displayName);
    }
  }, [createRoom, onJoin]);

  const handleHostRoom = useCallback((game: MockGame) => {
    const displayName = localStorage.getItem('retro-oasis-display-name') ?? 'Player';
    createRoom({ name: `${game.title} Room`, gameId: game.id, gameTitle: game.title, system: game.system, isPublic: true, maxPlayers: game.maxPlayers }, displayName);
  }, [createRoom]);

  return (
    <>
      <div style={{ display: 'flex', gap: 8, marginBottom: 16 }}>
        {(['all', 'GB', 'GBC', 'GBA'] as SystemFilter[]).map((f) => (
          <button key={f} onClick={() => setFilter(f)} style={{ padding: '6px 14px', borderRadius: 6, border: filter === f ? '2px solid #4B0082' : '1px solid #333', background: filter === f ? '#4B0082' : 'transparent', color: filter === f ? '#fff' : '#aaa', fontWeight: 600, fontSize: 12, cursor: 'pointer' }}>
            {f === 'all' ? 'All Systems' : f}
          </button>
        ))}
      </div>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(260px, 1fr))', gap: 16 }}>
        {filtered.map((game) => (
          <GBAGameCard key={game.id} game={game} openRooms={0} onQuickMatch={handleQuickMatch} onHostRoom={handleHostRoom} />
        ))}
      </div>
    </>
  );
}

export default function GBAPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [joinCode, setJoinCode] = useState<string | null>(null);
  const { joinByCode, currentRoom } = useLobby();
  const navigate = useNavigate();

  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  return (
    <div style={{ padding: 24, maxWidth: 960, margin: '0 auto' }}>
      <h1 style={{ margin: '0 0 4px', fontSize: 24, fontWeight: 800 }}>🎮 Game Boy / GBC / GBA</h1>
      <p style={{ margin: '0 0 20px', color: '#888', fontSize: 14 }}>Link cable emulation via mGBA — trade Pokémon, battle friends, play co-op</p>
      <GBABanner />
      <div style={{ display: 'flex', gap: 8, marginBottom: 20 }}>
        {(['lobby', 'leaderboard'] as ActiveTab[]).map((tab) => (
          <button key={tab} onClick={() => setActiveTab(tab)} style={{ padding: '8px 18px', borderRadius: 6, border: activeTab === tab ? '2px solid #4B0082' : '1px solid #333', background: activeTab === tab ? '#4B0082' : 'transparent', color: activeTab === tab ? '#fff' : '#aaa', fontWeight: 700, fontSize: 13, cursor: 'pointer', textTransform: 'capitalize' }}>
            {tab === 'lobby' ? '🎮 Lobby' : '🏆 Leaderboard'}
          </button>
        ))}
      </div>
      {activeTab === 'lobby' && <LobbyPanel onJoin={(code) => setJoinCode(code)} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
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
