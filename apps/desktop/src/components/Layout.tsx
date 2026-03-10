import { useState, useEffect } from 'react';
import { Outlet, Link, useLocation, useNavigate } from 'react-router-dom';
import { usePresence } from '../context/PresenceContext';
import { useLobby } from '../context/LobbyContext';
import { JoinRoomModal } from './JoinRoomModal';
import type { FriendInfo } from '@retro-oasis/presence-client';

const NAV_ITEMS = [
  { path: '/', label: '🏠 Home' },
  { path: '/library', label: '🎮 Library' },
  { path: '/friends', label: '👥 Friends' },
  { path: '/saves', label: '💾 Saves' },
  { path: '/settings', label: '⚙️ Settings' },
];

export function Layout() {
  const location = useLocation();
  const navigate = useNavigate();
  const { onlineFriends } = usePresence();
  const { joinByCode, joinAsSpectator, currentRoom } = useLobby();
  const [pendingFriend, setPendingFriend] = useState<FriendInfo | null>(null);

  // Navigate to lobby once joined via the sidebar friend-join modal
  useEffect(() => {
    if (currentRoom && location.pathname !== `/lobby/${currentRoom.id}`) {
      navigate(`/lobby/${currentRoom.id}`);
    }
  }, [currentRoom, location.pathname, navigate]);

  function handleJoinConfirm(roomCode: string, displayName: string) {
    joinByCode(roomCode, displayName);
    setPendingFriend(null);
  }

  function handleSpectate(roomCode: string, displayName: string) {
    joinAsSpectator({ roomCode }, displayName);
    setPendingFriend(null);
  }

  const onlineCount = onlineFriends.length;

  return (
    <div className="flex h-screen">
      {/* Sidebar */}
      <nav className="w-56 flex-shrink-0 flex flex-col" style={{ backgroundColor: 'var(--color-oasis-surface)' }}>
        <div className="p-4 border-b border-white/10">
          <h1 className="text-xl font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
            🌴 RetroOasis
          </h1>
          <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Play Together
          </p>
        </div>

        <div className="flex-1 py-4">
          {NAV_ITEMS.map((item) => (
            <Link
              key={item.path}
              to={item.path}
              className="flex items-center justify-between px-4 py-2.5 text-sm font-medium transition-colors rounded-lg mx-2 mb-1"
              style={{
                backgroundColor: location.pathname === item.path ? 'var(--color-oasis-card)' : 'transparent',
                color: location.pathname === item.path ? 'var(--color-oasis-accent-light)' : 'var(--color-oasis-text-muted)',
              }}
            >
              <span>{item.label}</span>
              {item.path === '/friends' && onlineCount > 0 && (
                <span
                  className="text-[10px] font-bold px-1.5 py-0.5 rounded-full"
                  style={{ backgroundColor: 'var(--color-oasis-green)', color: '#0a0a0a' }}
                >
                  {onlineCount}
                </span>
              )}
            </Link>
          ))}
        </div>

        {/* Live friends panel */}
        <div className="p-4 border-t border-white/10">
          <p className="text-xs font-semibold mb-2 flex items-center justify-between" style={{ color: 'var(--color-oasis-text-muted)' }}>
            <span>Friends Online</span>
            <span style={{ color: 'var(--color-oasis-green)' }}>{onlineCount}</span>
          </p>
          <div className="space-y-2">
            {onlineFriends.slice(0, 4).map((f) => (
              <FriendBadge
                key={f.userId}
                friend={f}
                onJoin={f.isJoinable ? () => setPendingFriend(f) : undefined}
              />
            ))}
            {onlineFriends.length === 0 && (
              <p className="text-[11px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
                No friends online
              </p>
            )}
            {onlineFriends.length > 4 && (
              <Link to="/friends" className="text-[10px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
                +{onlineFriends.length - 4} more →
              </Link>
            )}
          </div>
        </div>
      </nav>

      {/* Main content */}
      <main className="flex-1 overflow-y-auto p-6">
        <Outlet />
      </main>

      {/* Join-friend modal */}
      {pendingFriend && (
        <JoinRoomModal
          initialCode={pendingFriend.roomCode}
          onConfirm={handleJoinConfirm}
          onSpectate={handleSpectate}
          onClose={() => setPendingFriend(null)}
        />
      )}
    </div>
  );
}

function FriendBadge({
  friend,
  onJoin,
}: {
  friend: FriendInfo;
  onJoin?: () => void;
}) {
  const statusColor =
    friend.status === 'in-game'
      ? 'var(--color-oasis-green)'
      : friend.status === 'online'
        ? 'var(--color-oasis-accent-light)'
        : 'var(--color-oasis-text-muted)';

  return (
    <div className="flex items-center gap-2 group">
      <div className="w-2 h-2 rounded-full flex-shrink-0" style={{ backgroundColor: statusColor }} />
      <div className="flex-1 min-w-0">
        <p className="text-xs font-medium truncate" style={{ color: 'var(--color-oasis-text)' }}>
          {friend.displayName}
        </p>
        {friend.currentGameTitle && (
          <p className="text-[10px] truncate" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {friend.currentGameTitle}
          </p>
        )}
      </div>
      {onJoin && (
        <button
          onClick={onJoin}
          className="text-[10px] font-bold px-1.5 py-0.5 rounded opacity-0 group-hover:opacity-100 transition-opacity"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
          title={`Join ${friend.displayName}'s room`}
        >
          Join
        </button>
      )}
    </div>
  );
}
