import { useState, useEffect, useCallback } from 'react';
import { useLobby } from '../context/LobbyContext';

const BACKEND_URL: string =
  (import.meta.env?.VITE_BACKEND_URL as string | undefined) ?? '';

type NotificationType =
  | 'achievement-unlocked'
  | 'friend-request'
  | 'friend-accepted'
  | 'dm-received'
  | 'tournament-result'
  | 'room-invite';

interface AppNotification {
  id: string;
  playerId: string;
  type: NotificationType;
  title: string;
  body: string;
  read: boolean;
  createdAt: string;
  entityId?: string;
}

const TYPE_ICON: Record<NotificationType, string> = {
  'achievement-unlocked': '🏅',
  'friend-request': '👤',
  'friend-accepted': '✅',
  'dm-received': '💬',
  'tournament-result': '🏆',
  'room-invite': '🎮',
};

export function NotificationsPage() {
  const { playerId } = useLobby();
  const [notifications, setNotifications] = useState<AppNotification[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const fetchNotifications = useCallback(() => {
    if (!playerId) {
      setLoading(false);
      return;
    }
    const url = `${BACKEND_URL}/api/notifications/${encodeURIComponent(playerId)}`;
    fetch(url)
      .then((r) => r.json())
      .then((data: { notifications: AppNotification[] }) => {
        setNotifications(data.notifications ?? []);
        setLoading(false);
      })
      .catch((err: unknown) => {
        setError(String(err));
        setLoading(false);
      });
  }, [playerId]);

  useEffect(() => {
    fetchNotifications();
  }, [fetchNotifications]);

  function handleMarkAllRead() {
    if (!playerId) return;
    const url = `${BACKEND_URL}/api/notifications/${encodeURIComponent(playerId)}/read`;
    fetch(url, { method: 'POST' })
      .then(() => {
        setNotifications((prev) => prev.map((n) => ({ ...n, read: true })));
      })
      .catch(() => {
        // best-effort
      });
  }

  function handleMarkOneRead(notificationId: string) {
    if (!playerId) return;
    const url = `${BACKEND_URL}/api/notifications/${encodeURIComponent(playerId)}/read/${encodeURIComponent(notificationId)}`;
    fetch(url, { method: 'POST' })
      .then(() => {
        setNotifications((prev) =>
          prev.map((n) => (n.id === notificationId ? { ...n, read: true } : n))
        );
      })
      .catch(() => {
        // best-effort
      });
  }

  const unread = notifications.filter((n) => !n.read);
  const read = notifications.filter((n) => n.read);

  return (
    <div className="max-w-2xl">
      {/* Header */}
      <div className="mb-6 flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-black tracking-tight" style={{ color: '#fff' }}>
            🔔 Notifications
          </h1>
          <p className="text-xs font-semibold mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {unread.length} unread
          </p>
        </div>
        {unread.length > 0 && (
          <button
            onClick={handleMarkAllRead}
            className="text-xs font-black px-4 py-2 rounded-full transition-all hover:brightness-110"
            style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)', border: '1px solid var(--n-border)' }}
          >
            Mark all read
          </button>
        )}
      </div>

      {loading && (
        <p className="text-center py-12 text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Loading…
        </p>
      )}
      {error && (
        <p className="text-center py-12 text-sm font-semibold" style={{ color: '#f87171' }}>
          ⚠️ {error}
        </p>
      )}
      {!playerId && !loading && (
        <p className="text-center py-12 text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Connect to the lobby to see your notifications.
        </p>
      )}

      {!loading && !error && playerId && notifications.length === 0 && (
        <div className="text-center py-16">
          <p className="text-4xl mb-3">🔔</p>
          <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No notifications yet.
          </p>
        </div>
      )}

      {/* Unread section */}
      {unread.length > 0 && (
        <section className="mb-6">
          <p className="text-[10px] font-black uppercase tracking-widest mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Unread
          </p>
          <div className="space-y-2">
            {unread.map((n) => (
              <NotificationCard key={n.id} notification={n} onMarkRead={handleMarkOneRead} />
            ))}
          </div>
        </section>
      )}

      {/* Read section */}
      {read.length > 0 && (
        <section>
          <p className="text-[10px] font-black uppercase tracking-widest mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Earlier
          </p>
          <div className="space-y-2">
            {read.map((n) => (
              <NotificationCard key={n.id} notification={n} onMarkRead={handleMarkOneRead} />
            ))}
          </div>
        </section>
      )}
    </div>
  );
}

function NotificationCard({
  notification: n,
  onMarkRead,
}: {
  notification: AppNotification;
  onMarkRead: (id: string) => void;
}) {
  return (
    <div
      className="flex items-start gap-3 px-4 py-3 rounded-2xl"
      style={{
        backgroundColor: n.read ? 'var(--color-oasis-card)' : 'rgba(230,0,18,0.08)',
        border: n.read ? '1px solid var(--n-border)' : '1px solid rgba(230,0,18,0.25)',
      }}
    >
      <span className="text-xl leading-none flex-shrink-0 mt-0.5">{TYPE_ICON[n.type]}</span>
      <div className="flex-1 min-w-0">
        <p className="text-sm font-black leading-snug" style={{ color: n.read ? 'var(--color-oasis-text-muted)' : '#fff' }}>
          {n.title}
        </p>
        <p className="text-xs font-semibold mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {n.body}
        </p>
        <p className="text-[10px] mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {new Date(n.createdAt).toLocaleString()}
        </p>
      </div>
      {!n.read && (
        <button
          onClick={() => onMarkRead(n.id)}
          className="text-[10px] font-black px-2 py-1 rounded-full flex-shrink-0"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
          title="Mark as read"
        >
          ✓
        </button>
      )}
    </div>
  );
}
