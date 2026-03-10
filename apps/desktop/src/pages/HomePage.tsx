import { useState, useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useGames } from '../lib/use-games';
import { GameCard } from '../components/GameCard';
import { LobbyCard } from '../components/LobbyCard';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';
import { useLobby } from '../context/LobbyContext';
import { usePresence } from '../context/PresenceContext';
import { activityEmoji, activityVerb, relativeTime } from '../lib/presence-utils';
import type { Room } from '../services/lobby-types';
import type { FriendInfo } from '@retro-oasis/presence-client';

export function HomePage() {
  const navigate = useNavigate();
  const { publicRooms, createRoom, joinByCode, joinAsSpectator, currentRoom, listRooms, connectionState } = useLobby();
  const { joinableSessions, recentActivity } = usePresence();
  const [showHost, setShowHost] = useState(false);
  const [showJoin, setShowJoin] = useState(false);
  const [joinFriend, setJoinFriend] = useState<FriendInfo | null>(null);

  const { data: allGames } = useGames();
  const n64PartyGames = allGames.filter((g) => g.system === 'N64' && g.tags.includes('Party'));
  const partyPicks = allGames.filter((g) => g.tags.includes('Party') && g.system !== 'N64');
  const recentGames = allGames.slice(0, 4);

  // Refresh public rooms when the page mounts / becomes visible
  useEffect(() => {
    if (connectionState === 'connected') listRooms();
  }, [connectionState, listRooms]);

  // When we successfully join or create a room, navigate to the lobby
  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  function handleHostConfirm(payload: Parameters<typeof createRoom>[0], displayName: string) {
    createRoom(payload, displayName);
    setShowHost(false);
  }

  function handleJoinConfirm(roomCode: string, displayName: string) {
    joinByCode(roomCode, displayName);
    setShowJoin(false);
    setJoinFriend(null);
  }

  function handleSpectateConfirm(roomCode: string, displayName: string) {
    joinAsSpectator({ roomCode }, displayName);
    setShowJoin(false);
    setJoinFriend(null);
  }

  // Build a lobby-card-compatible object from a real Room
  function toLobbyCard(room: Room) {
    const game = allGames.find((g) => g.id === room.gameId);
    return {
      id: room.id,
      name: room.name,
      host: room.players[0]?.displayName ?? 'Unknown',
      gameTitle: room.gameTitle,
      system: room.system.toUpperCase(),
      systemColor: game?.systemColor ?? '#7c5cbf',
      playerCount: room.players.length,
      maxPlayers: room.maxPlayers,
      status: room.status,
    };
  }

  const displayLobbies = publicRooms.length > 0
    ? publicRooms.slice(0, 6).map(toLobbyCard)
    : [];

  return (
    <div className="space-y-8 max-w-5xl">
      {/* Connection status indicator */}
      {connectionState !== 'connected' && (
        <div
          className="px-4 py-2 rounded-xl text-xs font-semibold"
          style={{
            backgroundColor: connectionState === 'connecting' ? 'var(--color-oasis-yellow)' : 'var(--color-oasis-surface)',
            color: connectionState === 'connecting' ? '#1a1025' : 'var(--color-oasis-text-muted)',
          }}
        >
          {connectionState === 'connecting' ? '⏳ Connecting to lobby server…' : '⚠️ Offline — lobby server unreachable'}
        </div>
      )}

      {/* Hero actions */}
      <div className="flex gap-4">
        <button
          onClick={() => setShowHost(true)}
          className="flex-1 py-4 rounded-2xl text-lg font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
        >
          🎮 Host a Room
        </button>
        <button
          onClick={() => setShowJoin(true)}
          className="flex-1 py-4 rounded-2xl text-lg font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
          style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)' }}
        >
          🚪 Join a Room
        </button>
      </div>

      {/* Joinable Lobbies */}
      <section>
        <div className="flex items-center justify-between mb-3">
          <h2 className="text-lg font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
            🔥 Joinable Lobbies
          </h2>
          {connectionState === 'connected' && (
            <button
              onClick={listRooms}
              className="text-xs"
              style={{ color: 'var(--color-oasis-text-muted)' }}
            >
              ↻ Refresh
            </button>
          )}
        </div>
        {displayLobbies.length > 0 ? (
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3">
            {displayLobbies.map((lobby) => (
              <LobbyCard key={lobby.id} lobby={lobby} />
            ))}
          </div>
        ) : (
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {connectionState === 'connected'
              ? 'No open lobbies right now — be the first to host!'
              : 'Connect to the lobby server to see open rooms.'}
          </p>
        )}
      </section>

      {/* Friends Playing Now */}
      {joinableSessions.length > 0 && (
        <section>
          <div className="flex items-center justify-between mb-3">
            <div>
              <h2 className="text-lg font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
                👥 Friends Playing Now
              </h2>
              <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Jump in and play with your crew
              </p>
            </div>
            <Link to="/friends" className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              All Friends →
            </Link>
          </div>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3">
            {joinableSessions.map((friend) => (
              <FriendSessionCard
                key={friend.userId}
                friend={friend}
                onJoin={() => setJoinFriend(friend)}
              />
            ))}
          </div>
        </section>
      )}

      {/* Recent Activity */}
      {recentActivity.length > 0 && (
        <section>
          <div className="flex items-center justify-between mb-3">
            <h2 className="text-lg font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
              ⚡ Recent Activity
            </h2>
            <Link to="/friends" className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              See All →
            </Link>
          </div>
          <div
            className="rounded-2xl overflow-hidden"
            style={{ backgroundColor: 'var(--color-oasis-card)' }}
          >
            {recentActivity.slice(0, 4).map((item, i) => (
              <div
                key={item.id}
                className="px-4 py-3 flex items-center gap-3"
                style={{ borderTop: i > 0 ? '1px solid rgba(255,255,255,0.06)' : undefined }}
              >
                <span className="text-base">{activityEmoji(item.type)}</span>
                <div className="flex-1 min-w-0">
                  <p className="text-xs" style={{ color: 'var(--color-oasis-text)' }}>
                    <span className="font-semibold">{item.displayName}</span>{' '}
                    <span style={{ color: 'var(--color-oasis-text-muted)' }}>{activityVerb(item.type)}</span>
                    {item.gameTitle && (
                      <> <span className="font-medium">{item.gameTitle}</span></>
                    )}
                  </p>
                </div>
                <span className="text-[10px] flex-shrink-0" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {relativeTime(item.timestamp)}
                </span>
                {/* Only show Join if the friend is still joinable */}
                {item.roomCode && joinableSessions.some((f) => f.userId === item.userId) && (
                  <button
                    onClick={() => {
                      const friend = joinableSessions.find((f) => f.userId === item.userId);
                      if (friend) setJoinFriend(friend);
                    }}
                    className="text-[10px] font-bold px-2 py-1 rounded"
                    style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
                  >
                    Join
                  </button>
                )}
              </div>
            ))}
          </div>
        </section>
      )}

      {/* N64 Party Spotlight */}
      {n64PartyGames.length > 0 && (
        <section>
          <div className="flex items-center justify-between mb-3">
            <div>
              <h2 className="text-lg font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
                🟢 N64 Party Games
              </h2>
              <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Best with 4 players — grab your friends!
              </p>
            </div>
            <Link to="/library" className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              View All →
            </Link>
          </div>
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
            {n64PartyGames.map((game) => (
              <GameCard key={game.id} game={game} />
            ))}
          </div>
        </section>
      )}

      {/* Quick Party Picks (non-N64) */}
      {partyPicks.length > 0 && (
        <section>
          <div className="flex items-center justify-between mb-3">
            <h2 className="text-lg font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
              🎉 More Party Picks
            </h2>
            <Link to="/library" className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              View All →
            </Link>
          </div>
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
            {partyPicks.map((game) => (
              <GameCard key={game.id} game={game} />
            ))}
          </div>
        </section>
      )}

      {/* Recently Played */}
      <section>
        <h2 className="text-lg font-bold mb-3" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🕹️ Recently Played
        </h2>
        <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
          {recentGames.map((game) => (
            <GameCard key={game.id} game={game} />
          ))}
        </div>
      </section>

      {/* Modals */}
      {showHost && (
        <HostRoomModal
          onConfirm={handleHostConfirm}
          onClose={() => setShowHost(false)}
        />
      )}
      {(showJoin || joinFriend) && (
        <JoinRoomModal
          initialCode={joinFriend?.roomCode}
          onConfirm={handleJoinConfirm}
          onSpectate={handleSpectateConfirm}
          onClose={() => { setShowJoin(false); setJoinFriend(null); }}
        />
      )}
    </div>
  );
}

function FriendSessionCard({ friend, onJoin }: { friend: FriendInfo; onJoin: () => void }) {
  return (
    <div
      className="rounded-xl p-4 flex items-center gap-3"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      <div className="w-2.5 h-2.5 rounded-full flex-shrink-0" style={{ backgroundColor: 'var(--color-oasis-green)' }} />
      <div className="flex-1 min-w-0">
        <p className="text-sm font-bold truncate" style={{ color: 'var(--color-oasis-text)' }}>
          {friend.displayName}
        </p>
        <p className="text-xs truncate" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {friend.currentGameTitle ?? 'In a game'}
        </p>
      </div>
      <button
        onClick={onJoin}
        className="text-xs font-bold px-3 py-1.5 rounded-lg transition-transform hover:scale-[1.05] active:scale-[0.97]"
        style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
      >
        ▶ Join
      </button>
    </div>
  );
}

