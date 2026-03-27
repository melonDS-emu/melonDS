import { useState, useEffect } from 'react';
import { Outlet, Link, useLocation, useNavigate } from 'react-router-dom';
import { usePresence } from '../context/PresenceContext';
import { useLobby } from '../context/LobbyContext';
import { JoinRoomModal } from './JoinRoomModal';
import { ToastContainer } from './ToastContainer';
import { getRomDirectory } from '../lib/rom-settings';
import type { FriendInfo } from '@retro-oasis/presence-client';
const NAV_ITEMS = [
  { path: '/',              label: 'Home',         icon: '⌂',  group: 'main' },
  { path: '/library',       label: 'Library',      icon: '▣',  group: 'main' },
  { path: '/pokemon',       label: 'Pokémon',      icon: '◉',  group: 'games' },
  { path: '/mario-kart',    label: 'Mario Kart',   icon: '◈',  group: 'games' },
  { path: '/mario-sports',  label: 'Mario Sports', icon: '◆',  group: 'games' },
  { path: '/zelda',         label: 'Zelda',        icon: '◇',  group: 'games' },
  { path: '/metroid',       label: 'Metroid',      icon: '◑',  group: 'games' },
  { path: '/switch',        label: 'Switch',       icon: '🔴', group: 'systems' },
  { path: '/wii',           label: 'Wii',          icon: '⊕',  group: 'systems' },
  { path: '/wiiu',          label: 'Wii U',        icon: '⊞',  group: 'systems' },
  { path: '/3ds',           label: '3DS',          icon: '🎴', group: 'systems' },
  { path: '/gc',            label: 'GameCube',     icon: '⬡',  group: 'systems' },
  { path: '/genesis',       label: 'Genesis',      icon: '⬢',  group: 'systems' },
  { path: '/segacd',        label: 'Sega CD',      icon: '💿', group: 'systems' },
  { path: '/sms',           label: 'Master System', icon: '⊡',  group: 'systems' },
  { path: '/dreamcast',     label: 'Dreamcast',    icon: '◎',  group: 'systems' },
  { path: '/psx',           label: 'PlayStation',  icon: '□',  group: 'systems' },
  { path: '/ps2',           label: 'PS2',          icon: '◧',  group: 'systems' },
  { path: '/psp',           label: 'PSP',          icon: '▬',  group: 'systems' },
  { path: '/nes',           label: 'NES',          icon: '◻',  group: 'systems' },
  { path: '/snes',          label: 'SNES',         icon: '◼',  group: 'systems' },
  { path: '/gba',           label: 'GBA / GB',     icon: '◫',  group: 'systems' },
  { path: '/n64',           label: 'N64',          icon: '⬟',  group: 'systems' },
  { path: '/nds',           label: 'NDS',          icon: '⬠',  group: 'systems' },
  { path: '/community',     label: 'Community',    icon: '◉',  group: 'social' },
  { path: '/chat',          label: 'Chat',         icon: '◫',  group: 'social' },
  { path: '/events',        label: 'Events',       icon: '★',  group: 'social' },
  { path: '/party-collections', label: 'Party Collections', icon: '🎉', group: 'social' },
  { path: '/online-services',   label: 'Online Services',   icon: '🌐', group: 'social' },
  { path: '/friends',       label: 'Friends',      icon: '◈',  group: 'social' },
  { path: '/notifications', label: 'Notifications', icon: '◐', group: 'social' },
  { path: '/tournaments',   label: 'Tournaments',  icon: '◆',  group: 'more' },
  { path: '/clips',         label: 'Clips',        icon: '▶',  group: 'more' },
  { path: '/saves',         label: 'Saves',        icon: '◼',  group: 'more' },
  { path: '/retro-achievements', label: 'Retro Achievements', icon: '🏅', group: 'more' },
  { path: '/compatibility',      label: 'Compatibility',      icon: '🛡️', group: 'more' },
  { path: '/profile',       label: 'Profile',      icon: '◙',  group: 'more' },
  { path: '/settings',      label: 'Settings',     icon: '✦',  group: 'more' },
];

const NAV_GROUP_LABELS: Record<string, string> = {
  main: 'Discover',
  games: 'Games',
  systems: 'Systems',
  social: 'Social',
  more: 'More',
};

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
    const pid = playerId;
    let cancelled = false;
    const controller = new AbortController();

    function poll() {
      fetch(`${BACKEND_URL}/api/notifications/${encodeURIComponent(pid)}`, { signal: controller.signal })
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
  const showSetupPrompt = !getRomDirectory();

  return (
    <div className="flex h-screen" style={{ backgroundColor: 'var(--color-oasis-bg)' }}>
      {/* ── Modern sidebar ── */}
      <nav
        className="w-56 flex-shrink-0 flex flex-col"
        style={{
          background: 'linear-gradient(180deg, #0c0c20 0%, var(--color-oasis-surface) 100%)',
          borderRight: '1px solid rgba(255,255,255,0.06)',
        }}
      >
        {/* Logo */}
        <div className="px-4 py-5 flex items-center gap-2.5">
          <div
            className="w-8 h-8 rounded-lg flex items-center justify-center text-sm font-black flex-shrink-0 shimmer-overlay"
            style={{
              background: 'linear-gradient(135deg, #ff1a2e 0%, #e60012 50%, #a00010 100%)',
              boxShadow: '0 0 16px rgba(230,0,18,0.5), 0 2px 8px rgba(0,0,0,0.5)',
              color: '#fff',
            }}
          >
            R
          </div>
          <div>
            <h1 className="text-sm font-black tracking-tight leading-none hero-title">
              RetroOasis
            </h1>
            <p className="text-[9px] font-semibold mt-0.5" style={{ color: 'var(--color-oasis-text-subtle)' }}>
              Play Together
            </p>
          </div>
        </div>

        {/* Nav items — grouped */}
        <div className="flex-1 overflow-y-auto px-2 pb-2 space-y-4">
          {(['main', 'games', 'systems', 'social', 'more'] as const).map((group) => {
            const items = NAV_ITEMS.filter((i) => i.group === group);
            return (
              <div key={group}>
                <p
                  className="text-[9px] font-black uppercase tracking-widest px-2 mb-1"
                  style={{ color: 'var(--color-oasis-text-subtle)' }}
                >
                  {NAV_GROUP_LABELS[group]}
                </p>
                {items.map((item) => {
                  const active = location.pathname === item.path;
                  return (
                    <Link
                      key={item.path}
                      to={item.path}
                      className={`flex items-center gap-2.5 px-2.5 py-2 rounded-lg text-xs font-bold transition-all relative${active ? ' nav-item-active' : ''}`}
                      style={{
                        color: active ? 'var(--color-oasis-accent-light)' : 'var(--color-oasis-text-muted)',
                      }}
                    >
                      <span
                        className="w-5 h-5 flex items-center justify-center rounded-md text-[11px] flex-shrink-0 font-black"
                        style={{
                          backgroundColor: active
                            ? 'rgba(230,0,18,0.2)'
                            : 'rgba(255,255,255,0.04)',
                          color: active ? 'var(--color-oasis-accent-light)' : 'var(--color-oasis-text-muted)',
                        }}
                      >
                        {item.icon}
                      </span>
                      <span className="flex-1 leading-none">{item.label}</span>
                      {item.path === '/friends' && onlineCount > 0 && (
                        <span
                          className="text-[8px] font-black px-1 py-0.5 rounded-full min-w-[16px] text-center"
                          style={{ backgroundColor: 'var(--color-oasis-green)', color: '#0a0a0a' }}
                        >
                          {onlineCount}
                        </span>
                      )}
                      {item.path === '/friends' && pendingFriendRequests.length > 0 && (
                        <span
                          className="text-[8px] font-black px-1 py-0.5 rounded-full min-w-[16px] text-center"
                          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
                          title={`${pendingFriendRequests.length} pending`}
                        >
                          {pendingFriendRequests.length}
                        </span>
                      )}
                      {item.path === '/friends' && unreadDmCount > 0 && (
                        <span
                          className="text-[8px] font-black px-1 py-0.5 rounded-full min-w-[16px] text-center"
                          style={{ backgroundColor: 'var(--color-oasis-blue)', color: '#fff' }}
                          title={`${unreadDmCount} unread`}
                        >
                          DM
                        </span>
                      )}
                      {item.path === '/notifications' && unreadNotifCount > 0 && (
                        <span
                          className="text-[8px] font-black px-1 py-0.5 rounded-full min-w-[16px] text-center"
                          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
                          title={`${unreadNotifCount} unread`}
                        >
                          {unreadNotifCount}
                        </span>
                      )}
                    </Link>
                  );
                })}
              </div>
            );
          })}
        </div>

        {/* Complete Setup prompt — shown when ROM directory not configured */}
        {showSetupPrompt && (
          <div className="mx-2 mb-2">
            <Link
              to="/setup"
              className="flex items-center gap-2 px-2.5 py-2 rounded-lg text-xs font-bold transition-all hover:brightness-110"
              style={{
                backgroundColor: 'rgba(255,179,0,0.1)',
                border: '1px solid rgba(255,179,0,0.25)',
                color: 'var(--color-oasis-yellow)',
              }}
            >
              <span className="text-[11px]">⚙️</span>
              <span className="flex-1 leading-none">Complete Setup</span>
              <span className="text-[9px] opacity-70">→</span>
            </Link>
          </div>
        )}

        {/* Friends online panel */}
        <div
          className="mx-2 mb-3 rounded-xl p-2.5"
          style={{
            background: 'rgba(255,255,255,0.03)',
            border: '1px solid rgba(255,255,255,0.06)',
          }}
        >
          <p
            className="text-[9px] font-black uppercase tracking-widest mb-2 flex items-center justify-between"
            style={{ color: 'var(--color-oasis-text-subtle)' }}
          >
            <span>Friends Online</span>
            <span
              className="font-black px-1.5 py-0.5 rounded-full text-[8px]"
              style={{
                backgroundColor: onlineCount > 0 ? 'rgba(0,209,112,0.2)' : 'rgba(255,255,255,0.05)',
                color: onlineCount > 0 ? 'var(--color-oasis-green)' : 'var(--color-oasis-text-subtle)',
              }}
            >
              {onlineCount}
            </span>
          </p>
          <div className="space-y-1">
            {onlineFriends.slice(0, 4).map((f) => (
              <FriendBadge
                key={f.userId}
                friend={f}
                onJoin={f.isJoinable ? () => setPendingFriend(f) : undefined}
              />
            ))}
            {onlineFriends.length === 0 && (
              <p className="text-[10px]" style={{ color: 'var(--color-oasis-text-subtle)' }}>
                No friends online
              </p>
            )}
            {onlineFriends.length > 4 && (
              <Link
                to="/friends"
                className="text-[9px] font-bold"
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
        <div className="p-6 page-enter">
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
        : 'rgba(255,255,255,0.2)';

  return (
    <div className="flex items-center gap-1.5 group">
      <div
        className="w-1.5 h-1.5 rounded-full flex-shrink-0"
        style={{
          backgroundColor: statusColor,
          boxShadow: friend.status !== 'offline' ? `0 0 6px ${statusColor}` : 'none',
        }}
      />
      <div className="flex-1 min-w-0">
        <p className="text-[11px] font-bold truncate" style={{ color: 'var(--color-oasis-text)' }}>
          {friend.displayName}
        </p>
        {friend.currentGameTitle && (
          <p className="text-[9px] truncate" style={{ color: 'var(--color-oasis-text-subtle)' }}>
            {friend.currentGameTitle}
          </p>
        )}
      </div>
      {onJoin && (
        <button
          onClick={onJoin}
          className="text-[9px] font-black px-1.5 py-0.5 rounded-full opacity-0 group-hover:opacity-100 transition-opacity"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
          title={`Join ${friend.displayName}'s room`}
        >
          Join
        </button>
      )}
    </div>
  );
}
