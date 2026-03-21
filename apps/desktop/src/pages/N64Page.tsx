import { useState, useEffect, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';
import type { MockGame } from '../data/mock-games';
import { MOCK_GAMES } from '../data/mock-games';
import { JoinRoomModal } from '../components/JoinRoomModal';

const N64_GAMES = MOCK_GAMES.filter((g) => g.system === 'N64');

const SERVER_URL =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

interface N64Ranking { displayName: string; sessions: number; }
type ActiveTab = 'lobby' | 'leaderboard';

function RankBadge({ rank }: { rank: number }) {
  if (rank === 1) return <span style={{ fontSize: 18 }}>🥇</span>;
  if (rank === 2) return <span style={{ fontSize: 18 }}>🥈</span>;
  if (rank === 3) return <span style={{ fontSize: 18 }}>🥉</span>;
  return <span style={{ display: 'inline-flex', alignItems: 'center', justifyContent: 'center', width: 24, height: 24, borderRadius: '50%', background: '#333', color: '#ccc', fontSize: 12, fontWeight: 700 }}>{rank}</span>;
}

function N64Banner() {
  return (
    <div style={{ background: 'linear-gradient(135deg, #007500 0%, #004d00 100%)', borderRadius: 12, padding: '16px 20px', marginBottom: 20, color: '#fff', fontSize: 13 }}>
      <strong>🎮 N64 · Mupen64Plus Emulator</strong>
      <span style={{ margin: '0 8px', opacity: 0.6 }}>·</span>
      <span>2–4 player relay netplay</span>
      <span style={{ margin: '0 8px', opacity: 0.6 }}>·</span>
      <span>Automatic .z64 / .n64 / .v64 ROM detection</span>
      <p style={{ marginTop: 6, marginBottom: 0, opacity: 0.85, fontSize: 12 }}>
        Mupen64Plus with dynarec (fast preset) for best performance, or interpreter mode (accurate preset) for maximum compatibility. Fallback to Project64 or RetroArch (mupen64plus-next core).
      </p>
    </div>
  );
}

function N64GameCard({ game, openRooms, onQuickMatch, onHostRoom }: { game: MockGame; openRooms: number; onQuickMatch: (g: MockGame) => void; onHostRoom: (g: MockGame) => void }) {
  return (
    <div style={{ background: '#1a1a1a', borderRadius: 10, padding: 16, display: 'flex', flexDirection: 'column', gap: 8, border: '1px solid #2a2a2a' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
        <span style={{ fontSize: 32 }}>{game.coverEmoji}</span>
        <div>
          <div style={{ fontWeight: 700, fontSize: 14 }}>{game.title}</div>
          <div style={{ color: '#888', fontSize: 12 }}>{game.maxPlayers === 1 ? '1 Player' : `Up to ${game.maxPlayers} players`}</div>
        </div>
      </div>
      <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
        {game.tags.map((t) => (
          <span key={t} style={{ background: '#2a2a2a', borderRadius: 4, padding: '2px 6px', fontSize: 11, color: '#aaa' }}>{t}</span>
        ))}
        {openRooms > 0 && <span style={{ background: '#0a2a0a', borderRadius: 4, padding: '2px 6px', fontSize: 11, color: '#4caf50' }}>{openRooms} open</span>}
      </div>
      <div style={{ color: '#888', fontSize: 12, lineHeight: 1.4 }}>{game.description}</div>
      {game.maxPlayers > 1 && (
        <div style={{ display: 'flex', gap: 8, marginTop: 4 }}>
          <button onClick={() => onQuickMatch(game)} style={{ flex: 1, padding: '8px 0', borderRadius: 6, border: 'none', background: 'linear-gradient(135deg, #007500, #004d00)', color: '#fff', fontWeight: 700, fontSize: 13, cursor: 'pointer' }}>⚡ Quick Match</button>
          <button onClick={() => onHostRoom(game)} style={{ flex: 1, padding: '8px 0', borderRadius: 6, border: '1px solid #009900', background: 'transparent', color: '#4caf50', fontWeight: 700, fontSize: 13, cursor: 'pointer' }}>🎮 Host Room</button>
        </div>
      )}
    </div>
  );
}

function LeaderboardPanel() {
  const [rankings, setRankings] = useState<N64Ranking[]>([]);
  useEffect(() => {
    fetch(`${SERVER_URL}/api/leaderboard?orderBy=sessions&limit=10`)
      .then((r) => r.json())
      .then((data: { leaderboard: N64Ranking[] }) => setRankings(data.leaderboard ?? []))
      .catch(() => {});
  }, []);
  return (
    <div style={{ maxWidth: 500 }}>
      <h3 style={{ margin: '0 0 16px', fontSize: 16, color: '#4caf50' }}>🏆 Top N64 Players</h3>
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
  const navigate = useNavigate();

  const handleQuickMatch = useCallback(async (game: MockGame) => {
    try {
      const res = await fetch(`${SERVER_URL}/api/rooms?gameId=${encodeURIComponent(game.id)}`);
      const data = (await res.json()) as { rooms?: Array<{ roomCode: string }> };
      const rooms = data.rooms ?? [];
      if (rooms.length > 0) {
        onJoin(rooms[0].roomCode);
      } else {
        const room = await createRoom({ gameId: game.id, maxPlayers: game.maxPlayers });
        if (room) navigate(`/lobby/${room.roomCode}`);
      }
    } catch {
      const room = await createRoom({ gameId: game.id, maxPlayers: game.maxPlayers });
      if (room) navigate(`/lobby/${room.roomCode}`);
    }
  }, [createRoom, navigate, onJoin]);

  const handleHostRoom = useCallback(async (game: MockGame) => {
    const room = await createRoom({ gameId: game.id, maxPlayers: game.maxPlayers });
    if (room) navigate(`/lobby/${room.roomCode}`);
  }, [createRoom, navigate]);

  return (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(260px, 1fr))', gap: 16 }}>
      {N64_GAMES.map((game) => (
        <N64GameCard key={game.id} game={game} openRooms={0} onQuickMatch={handleQuickMatch} onHostRoom={handleHostRoom} />
      ))}
    </div>
  );
}

export default function N64Page() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [joinCode, setJoinCode] = useState<string | null>(null);
  return (
    <div style={{ padding: 24, maxWidth: 960, margin: '0 auto' }}>
      <h1 style={{ margin: '0 0 4px', fontSize: 24, fontWeight: 800 }}>🎮 Nintendo 64</h1>
      <p style={{ margin: '0 0 20px', color: '#888', fontSize: 14 }}>Classic N64 games — up to 4 players via Mupen64Plus relay netplay</p>
      <N64Banner />
      <div style={{ display: 'flex', gap: 8, marginBottom: 20 }}>
        {(['lobby', 'leaderboard'] as ActiveTab[]).map((tab) => (
          <button key={tab} onClick={() => setActiveTab(tab)} style={{ padding: '8px 18px', borderRadius: 6, border: activeTab === tab ? '2px solid #009900' : '1px solid #333', background: activeTab === tab ? '#009900' : 'transparent', color: activeTab === tab ? '#fff' : '#aaa', fontWeight: 700, fontSize: 13, cursor: 'pointer', textTransform: 'capitalize' }}>
            {tab === 'lobby' ? '🎮 Lobby' : '🏆 Leaderboard'}
          </button>
        ))}
      </div>
      {activeTab === 'lobby' && <LobbyPanel onJoin={(code) => setJoinCode(code)} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
      {joinCode && <JoinRoomModal roomCode={joinCode} onClose={() => setJoinCode(null)} />}
    </div>
  );
}
