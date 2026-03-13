import { useState } from 'react';
import { usePresence } from '../context/PresenceContext';
import { useLobby } from '../context/LobbyContext';
import { JoinRoomModal } from '../components/JoinRoomModal';
import { activityEmoji, activityVerb, relativeTime, avatarColor } from '../lib/presence-utils';
import type { FriendInfo } from '@retro-oasis/presence-client';

type StatusFilter = 'all' | 'online' | 'in-game';

export function FriendsPage() {
  const { friends, recentActivity } = usePresence();
  const { joinByCode, joinAsSpectator, pendingFriendRequests, acceptFriendRequest, declineFriendRequest, sendFriendRequest } = useLobby();
  const [filter, setFilter] = useState<StatusFilter>('all');
  const [joinTarget, setJoinTarget] = useState<FriendInfo | null>(null);
  const [addInput, setAddInput] = useState('');
  const [addStatus, setAddStatus] = useState('');

  const filtered = friends.filter((f) => {
    if (filter === 'online') return f.status === 'online' || f.status === 'in-game' || f.status === 'idle';
    if (filter === 'in-game') return f.status === 'in-game';
    return true;
  });

  const inGame = friends.filter((f) => f.status === 'in-game');
  const online = friends.filter((f) => f.status === 'online');
  const idle = friends.filter((f) => f.status === 'idle');

  function handleJoinConfirm(roomCode: string, displayName: string) {
    joinByCode(roomCode, displayName);
    setJoinTarget(null);
  }

  function handleSpectate(roomCode: string, displayName: string) {
    joinAsSpectator({ roomCode }, displayName);
    setJoinTarget(null);
  }

  function handleSendRequest() {
    const id = addInput.trim();
    if (!id) return;
    sendFriendRequest(id);
    setAddInput('');
    setAddStatus('Request sent!');
    setTimeout(() => setAddStatus(''), 3000);
  }

  return (
    <div className="space-y-8 max-w-4xl">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
          👥 Friends
        </h1>
        <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {inGame.length} in game · {online.length} online · {idle.length} idle
        </p>
      </div>

      {/* Add Friend */}
      <section>
        <h2 className="text-base font-bold mb-2" style={{ color: 'var(--color-oasis-accent-light)' }}>
          ➕ Add Friend
        </h2>
        <div className="flex gap-2 items-center">
          <input
            value={addInput}
            onChange={(e) => setAddInput(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && handleSendRequest()}
            placeholder="Player ID…"
            className="rounded-xl px-3 py-2 text-sm border-0 focus:outline-none flex-1 max-w-xs"
            style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)' }}
          />
          <button
            onClick={handleSendRequest}
            disabled={!addInput.trim()}
            className="text-sm font-bold px-4 py-2 rounded-xl transition-opacity"
            style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff', opacity: addInput.trim() ? 1 : 0.5 }}
          >
            Send Request
          </button>
          {addStatus && (
            <span className="text-xs" style={{ color: 'var(--color-oasis-accent-light)' }}>
              {addStatus}
            </span>
          )}
        </div>
      </section>

      {/* Pending Friend Requests */}
      {pendingFriendRequests.length > 0 && (
        <section>
          <h2 className="text-base font-bold mb-3" style={{ color: 'var(--color-oasis-accent-light)' }}>
            📬 Friend Requests ({pendingFriendRequests.length})
          </h2>
          <div className="rounded-2xl overflow-hidden" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
            {pendingFriendRequests.map((req, i) => (
              <div
                key={req.requestId}
                className="px-4 py-3 flex items-center gap-3"
                style={{ borderTop: i > 0 ? '1px solid rgba(255,255,255,0.06)' : undefined }}
              >
                <div
                  className="w-9 h-9 rounded-full flex items-center justify-center text-sm font-bold flex-shrink-0"
                  style={{ backgroundColor: avatarColor(req.fromId), color: 'white' }}
                >
                  {req.fromDisplayName[0]?.toUpperCase() ?? '?'}
                </div>
                <div className="flex-1 min-w-0">
                  <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text)' }}>
                    {req.fromDisplayName}
                  </p>
                  <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    wants to be your friend
                  </p>
                </div>
                <button
                  onClick={() => acceptFriendRequest(req.requestId)}
                  className="text-xs font-bold px-3 py-1.5 rounded-lg"
                  style={{ backgroundColor: 'var(--color-oasis-green)', color: '#0a0a0a' }}
                >
                  ✓ Accept
                </button>
                <button
                  onClick={() => declineFriendRequest(req.requestId)}
                  className="text-xs font-bold px-3 py-1.5 rounded-lg"
                  style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
                >
                  ✕
                </button>
              </div>
            ))}
          </div>
        </section>
      )}

      {/* Joinable sessions spotlight */}
      {inGame.filter((f) => f.isJoinable).length > 0 && (
        <section>
          <h2 className="text-base font-bold mb-3" style={{ color: 'var(--color-oasis-accent-light)' }}>
            🚀 Open Rooms from Friends
          </h2>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-3">
            {inGame.filter((f) => f.isJoinable).map((friend) => (
              <JoinableSessionCard
                key={friend.userId}
                friend={friend}
                onJoin={() => setJoinTarget(friend)}
              />
            ))}
          </div>
        </section>
      )}

      {/* Filter tabs */}
      <div className="flex gap-2">
        {(['all', 'online', 'in-game'] as StatusFilter[]).map((tab) => (
          <button
            key={tab}
            onClick={() => setFilter(tab)}
            className="px-3 py-1.5 rounded-lg text-xs font-semibold transition-colors capitalize"
            style={{
              backgroundColor: filter === tab ? 'var(--color-oasis-accent)' : 'var(--color-oasis-card)',
              color: filter === tab ? 'white' : 'var(--color-oasis-text-muted)',
            }}
          >
            {tab === 'all' ? `All (${friends.length})` : tab === 'in-game' ? `In Game (${inGame.length})` : `Online (${online.length + idle.length})`}
          </button>
        ))}
      </div>

      {/* Friends list */}
      <div
        className="rounded-2xl overflow-hidden"
        style={{ backgroundColor: 'var(--color-oasis-card)' }}
      >
        {filtered.length === 0 && (
          <p className="p-6 text-sm text-center" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No friends to show
          </p>
        )}
        {filtered.map((friend, i) => (
          <div
            key={friend.userId}
            className="px-4 py-3 flex items-center gap-3"
            style={{ borderTop: i > 0 ? '1px solid rgba(255,255,255,0.06)' : undefined }}
          >
            {/* Avatar placeholder */}
            <div
              className="w-9 h-9 rounded-full flex items-center justify-center text-sm font-bold flex-shrink-0"
              style={{ backgroundColor: avatarColor(friend.userId), color: 'white' }}
            >
              {friend.displayName[0].toUpperCase()}
            </div>

            {/* Info */}
            <div className="flex-1 min-w-0">
              <div className="flex items-center gap-2">
                <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text)' }}>
                  {friend.displayName}
                </p>
                <StatusDot status={friend.status} />
              </div>
              <p className="text-xs truncate" style={{ color: 'var(--color-oasis-text-muted)' }}>
                {statusLabel(friend)}
              </p>
            </div>

            {/* Action */}
            {friend.isJoinable && (
              <button
                onClick={() => setJoinTarget(friend)}
                className="text-xs font-bold px-3 py-1.5 rounded-lg transition-transform hover:scale-[1.05] active:scale-[0.97]"
                style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
              >
                ▶ Join
              </button>
            )}
          </div>
        ))}
      </div>

      {/* Recent Activity */}
      <section>
        <h2 className="text-base font-bold mb-3" style={{ color: 'var(--color-oasis-accent-light)' }}>
          ⚡ Recent Activity
        </h2>
        <div
          className="rounded-2xl overflow-hidden"
          style={{ backgroundColor: 'var(--color-oasis-card)' }}
        >
          {recentActivity.length === 0 && (
            <p className="p-6 text-sm text-center" style={{ color: 'var(--color-oasis-text-muted)' }}>
              No recent activity
            </p>
          )}
          {recentActivity.map((item, i) => (
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
              {/* Only show Join if the friend is currently joinable */}
              {item.roomCode && friends.some((f) => f.userId === item.userId && f.isJoinable) && (
                <button
                  onClick={() => {
                    const friend = friends.find((f) => f.userId === item.userId && f.isJoinable);
                    if (friend) setJoinTarget(friend);
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

      {/* Join modal */}
      {joinTarget && (
        <JoinRoomModal
          initialCode={joinTarget.roomCode}
          onConfirm={handleJoinConfirm}
          onSpectate={handleSpectate}
          onClose={() => setJoinTarget(null)}
        />
      )}
    </div>
  );
}

function JoinableSessionCard({ friend, onJoin }: { friend: FriendInfo; onJoin: () => void }) {
  return (
    <div
      className="rounded-xl p-4 flex items-center gap-3"
      style={{ backgroundColor: 'var(--color-oasis-surface)' }}
    >
      <div
        className="w-10 h-10 rounded-full flex items-center justify-center text-base font-bold flex-shrink-0"
        style={{ backgroundColor: avatarColor(friend.userId), color: 'white' }}
      >
        {friend.displayName[0].toUpperCase()}
      </div>
      <div className="flex-1 min-w-0">
        <p className="text-sm font-bold truncate" style={{ color: 'var(--color-oasis-text)' }}>
          {friend.displayName}
        </p>
        <p className="text-xs truncate" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {friend.currentGameTitle ?? 'In a game'}
        </p>
        {friend.roomCode && (
          <p className="text-[10px] font-mono mt-0.5" style={{ color: 'var(--color-oasis-accent-light)' }}>
            Room: {friend.roomCode}
          </p>
        )}
      </div>
      <button
        onClick={onJoin}
        className="text-xs font-bold px-3 py-2 rounded-lg transition-transform hover:scale-[1.05] active:scale-[0.97]"
        style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
      >
        ▶ Join
      </button>
    </div>
  );
}

function StatusDot({ status }: { status: string }) {
  const color =
    status === 'in-game'
      ? 'var(--color-oasis-green)'
      : status === 'online'
        ? 'var(--color-oasis-accent-light)'
        : status === 'idle'
          ? 'var(--color-oasis-yellow)'
          : 'var(--color-oasis-text-muted)';
  const label =
    status === 'in-game' ? 'In Game'
    : status === 'online' ? 'Online'
    : status === 'idle' ? 'Idle'
    : 'Offline';

  return (
    <span className="flex items-center gap-1">
      <span className="w-1.5 h-1.5 rounded-full inline-block" style={{ backgroundColor: color }} />
      <span className="text-[10px]" style={{ color: 'var(--color-oasis-text-muted)' }}>{label}</span>
    </span>
  );
}

function statusLabel(friend: FriendInfo): string {
  if (friend.status === 'in-game' && friend.currentGameTitle) return `Playing ${friend.currentGameTitle}`;
  if (friend.status === 'in-game') return 'In a game';
  if (friend.status === 'online') return 'Online';
  if (friend.status === 'idle') return `Idle · last seen ${relativeTime(friend.lastSeenAt)}`;
  return `Offline · last seen ${relativeTime(friend.lastSeenAt)}`;
}
