import { useState, useEffect } from 'react';
import { Outlet, Link, useLocation, useNavigate } from 'react-router-dom';
import { usePresence } from '../context/PresenceContext';
import { useLobby } from '../context/LobbyContext';
import { JoinRoomModal } from './JoinRoomModal';
import { ToastContainer } from './ToastContainer';
import type { FriendInfo } from '@retro-oasis/presence-client';
const NAV_ITEMS = [
  { path: '/',             label: 'Home',        icon: '🏠' },
  { path: '/library',      label: 'Library',     icon: '🎮' },
  { path: '/pokemon',      label: 'Pokémon',     icon: '🔴' },
  { path: '/mario-kart',    label: 'Mario Kart',   icon: '🏎️' },
  { path: '/mario-sports',  label: 'Mario Sports', icon: '🎾' },
  { path: '/zelda',         label: 'Zelda',        icon: '🗡️' },
  { path: '/metroid',       label: 'Metroid',      icon: '🌌' },
  { path: '/community',    label: 'Community',    icon: '🌐' },
  { path: '/chat',         label: 'Chat',         icon: '💬' },
  { path: '/events',       label: 'Events',       icon: '🎪' },
  { path: '/friends',      label: 'Friends',      icon: '👥' },
  { path: '/notifications', label: 'Notifications', icon: '🔔' },
  { path: '/tournaments',  label: 'Tournaments', icon: '🏆' },
  { path: '/clips',        label: 'Clips',       icon: '🎬' },
  { path: '/saves',        label: 'Saves',       icon: '💾' },
  { path: '/profile',      label: 'Profile',     icon: '👤' },
  { path: '/settings',     label: 'Settings',    icon: '⚙️' },
];

export function Layout() {
  const location = useLocation();
  const navigate = useNavigate();
  const { onlineFriends } = usePresence();
  const { joinByCode, joinAsSpectator, currentRoom, pendingFriendRequests, unreadDmCount, playerId } = useLobby();
  const [pendingFriend, setPendingFriend] = useState<FriendInfo | null>(null);
  const [unreadNotifCount, setUnreadNotifCount] = useState(0);

  const BACKEND_URL: string =
    (import.meta.env?.VITE_BACKEND_URL as string | undefined) ?? '';

  useEffect(() => {
    if (!playerId) return;
    let cancelled = false;
    const controller = new AbortController();

    function poll() {
      fetch(`${BACKEND_URL}/api/notifications/${encodeURIComponent(playerId)}`, { signal: controller.signal })
        .then((r) => r.json())
        .then((data: { notifications: Array<{ read: boolean }> }) => {
          if (!cancelled) {
            setUnreadNotifCount((data.notifications ?? []).filter((n) => !n.read).length);
          }
        })
        .catch(() => {
          // best-effort; AbortError is expected on cleanup
        });
    }

    poll();
    const interval = setInterval(poll, 30_000);
    return () => {
      cancelled = true;
      clearInterval(interval);
      controller.abort();
    };
  }, [playerId, BACKEND_URL]);

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
      {/* ── Nintendo-style sidebar ── */}
      <nav
        className="w-60 flex-shrink-0 flex flex-col border-r"
        style={{
          backgroundColor: 'var(--color-oasis-surface)',
          borderColor: 'var(--n-border)',
        }}
      >
        {/* Logo */}
        <div className="px-5 py-5 flex items-center gap-3">
          <div
            className="w-9 h-9 rounded-xl flex items-center justify-center text-lg font-black flex-shrink-0"
            style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
          >
            R
          </div>
          <div>
            <h1 className="text-base font-black tracking-tight leading-none" style={{ color: '#fff' }}>
              RetroOasis
            </h1>
            <p className="text-[10px] font-semibold mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Play Together
            </p>
          </div>
        </div>

        {/* Divider */}
        <div className="mx-4 mb-3" style={{ height: 1, backgroundColor: 'var(--n-border)' }} />

        {/* Nav items */}
        <div className="flex-1 px-3 space-y-0.5">
          {NAV_ITEMS.map((item) => {
            const active = location.pathname === item.path;
            return (
              <Link
                key={item.path}
                to={item.path}
                className="flex items-center gap-3 px-3 py-2.5 rounded-xl text-sm font-bold transition-all relative"
                style={{
                  backgroundColor: active ? 'rgba(230,0,18,0.15)' : 'transparent',
                  color: active ? 'var(--color-oasis-accent)' : 'var(--color-oasis-text-muted)',
                }}
              >
                {/* Active left-border accent */}
                {active && (
                  <span
                    className="absolute left-0 top-1/2 -translate-y-1/2 rounded-r-full"
                    style={{
                      width: 3,
                      height: 22,
                      backgroundColor: 'var(--color-oasis-accent)',
                    }}
                  />
                )}
                <span className="text-base leading-none w-5 text-center">{item.icon}</span>
                <span className="flex-1">{item.label}</span>
                {item.path === '/friends' && onlineCount > 0 && (
                  <span
                    className="text-[9px] font-black px-1.5 py-0.5 rounded-full min-w-[18px] text-center"
                    style={{ backgroundColor: 'var(--color-oasis-green)', color: '#0a0a0a' }}
                  >
                    {onlineCount}
                  </span>
                )}
                {item.path === '/friends' && pendingFriendRequests.length > 0 && (
                  <span
                    className="text-[9px] font-black px-1.5 py-0.5 rounded-full min-w-[18px] text-center"
                    style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
                    title={`${pendingFriendRequests.length} pending friend request${pendingFriendRequests.length !== 1 ? 's' : ''}`}
                  >
                    {pendingFriendRequests.length}
                  </span>
                )}
                {item.path === '/friends' && unreadDmCount > 0 && (
                  <span
                    className="text-[9px] font-black px-1.5 py-0.5 rounded-full min-w-[18px] text-center"
                    style={{ backgroundColor: 'var(--color-oasis-blue)', color: '#fff' }}
                    title={`${unreadDmCount} unread message${unreadDmCount !== 1 ? 's' : ''}`}
                  >
                    💬
                  </span>
                )}
                {item.path === '/notifications' && unreadNotifCount > 0 && (
                  <span
                    className="text-[9px] font-black px-1.5 py-0.5 rounded-full min-w-[18px] text-center"
                    style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
                    title={`${unreadNotifCount} unread notification${unreadNotifCount !== 1 ? 's' : ''}`}
                  >
                    {unreadNotifCount}
                  </span>
                )}
              </Link>
            );
          })}
        </div>

        {/* Friends online panel */}
        <div
          className="mx-3 mb-4 rounded-2xl p-3"
          style={{ backgroundColor: 'var(--color-oasis-card)' }}
        >
          <p
            className="text-[10px] font-black uppercase tracking-widest mb-2 flex items-center justify-between"
            style={{ color: 'var(--color-oasis-text-muted)' }}
          >
            <span>Friends Online</span>
            <span
              className="font-black px-1.5 py-0.5 rounded-full text-[9px]"
              style={{
                backgroundColor: onlineCount > 0 ? 'var(--color-oasis-green)' : 'var(--color-oasis-card)',
                color: onlineCount > 0 ? '#0a0a0a' : 'var(--color-oasis-text-muted)',
              }}
            >
              {onlineCount}
            </span>
          </p>
          <div className="space-y-1.5">
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
              <Link
                to="/friends"
                className="text-[10px] font-bold"
                style={{ color: 'var(--color-oasis-accent)' }}
              >
                +{onlineFriends.length - 4} more →
              </Link>
            )}
          </div>
        </div>
      </nav>

      {/* ── Main content ── */}
      <main className="flex-1 overflow-y-auto" style={{ backgroundColor: 'var(--color-oasis-bg)' }}>
        <div className="p-7">
          <Outlet />
        </div>
      </main>

      {/* Modals & overlays */}
      {pendingFriend && (
        <JoinRoomModal
          initialCode={pendingFriend.roomCode}
          onConfirm={handleJoinConfirm}
          onSpectate={handleSpectate}
          onClose={() => setPendingFriend(null)}
        />
      )}
      <ToastContainer />
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
        ? 'var(--color-oasis-blue)'
        : 'var(--color-oasis-text-muted)';

  return (
    <div className="flex items-center gap-2 group">
      <div
        className="w-2 h-2 rounded-full flex-shrink-0"
        style={{ backgroundColor: statusColor }}
      />
      <div className="flex-1 min-w-0">
        <p className="text-xs font-bold truncate" style={{ color: 'var(--color-oasis-text)' }}>
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
          className="text-[10px] font-black px-2 py-0.5 rounded-full opacity-0 group-hover:opacity-100 transition-opacity"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
          title={`Join ${friend.displayName}'s room`}
        >
          Join
        </button>
      )}
    </div>
  );
}
