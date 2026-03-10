import { useState, useEffect, useRef } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';

export function LobbyPage() {
  const { lobbyId } = useParams<{ lobbyId: string }>();
  const navigate = useNavigate();
  const {
    currentRoom,
    playerId,
    chatMessages,
    toggleReady,
    leaveRoom,
    startGame,
    sendChat,
    joinById,
    error,
    clearError,
  } = useLobby();

  const [chatInput, setChatInput] = useState('');
  const chatEndRef = useRef<HTMLDivElement>(null);

  // Auto-join if we navigated directly to a lobby URL
  useEffect(() => {
    if (lobbyId && !currentRoom) {
      const savedName = localStorage.getItem('retro-oasis-display-name') ?? 'Guest';
      joinById(lobbyId, savedName);
    }
  }, [lobbyId, currentRoom, joinById]);

  // Scroll chat to bottom on new messages
  useEffect(() => {
    chatEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [chatMessages]);

  function handleLeave() {
    leaveRoom();
    navigate('/');
  }

  function handleChatSubmit(e: React.FormEvent) {
    e.preventDefault();
    if (chatInput.trim()) {
      sendChat(chatInput.trim());
      setChatInput('');
    }
  }

  // Use real room or show loading
  const room = currentRoom;
  const roomMessages = chatMessages.filter((m) => m.roomId === lobbyId);

  if (!room) {
    return (
      <div className="max-w-2xl">
        <button onClick={() => navigate('/')} className="text-sm mb-4 inline-block" style={{ color: 'var(--color-oasis-text-muted)' }}>
          ← Back to Home
        </button>
        <div className="rounded-2xl p-6 text-center" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Joining lobby…
          </p>
        </div>
      </div>
    );
  }

  const myPlayer = room.players.find((p) => p.id === playerId);
  const isHost = myPlayer?.isHost ?? false;
  const imReady = myPlayer?.readyState === 'ready';
  const allReady = room.players.length > 1 && room.players.every((p) => p.readyState === 'ready');

  return (
    <div className="max-w-2xl">
      <button onClick={handleLeave} className="text-sm mb-4 inline-block" style={{ color: 'var(--color-oasis-text-muted)' }}>
        ← Leave Room
      </button>

      {error && (
        <div
          className="mb-3 px-4 py-2 rounded-xl text-sm font-semibold flex items-center justify-between"
          style={{ backgroundColor: 'var(--color-oasis-surface)', color: '#f87171' }}
        >
          <span>⚠️ {error}</span>
          <button onClick={clearError} className="ml-4 text-xs opacity-70">✕</button>
        </div>
      )}

      <div className="rounded-2xl p-6" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
        <div className="flex items-center justify-between mb-4">
          <div>
            <h1 className="text-xl font-bold">{room.name}</h1>
            <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {room.gameTitle} · {room.system.toUpperCase()}
            </p>
          </div>
          <div className="text-right">
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>Room Code</p>
            <p
              className="text-sm font-mono font-bold cursor-pointer"
              style={{ color: 'var(--color-oasis-accent-light)' }}
              title="Click to copy"
              onClick={() => navigator.clipboard.writeText(room.roomCode)}
            >
              {room.roomCode}
            </p>
          </div>
        </div>

        {/* Player list */}
        <div className="space-y-2 mb-6">
          <p className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Players ({room.players.length}/{room.maxPlayers})
          </p>
          {room.players.map((player) => (
            <div
              key={player.id}
              className="flex items-center justify-between p-3 rounded-xl"
              style={{ backgroundColor: 'var(--color-oasis-surface)' }}
            >
              <div className="flex items-center gap-3">
                <div
                  className="w-8 h-8 rounded-full flex items-center justify-center text-sm font-bold"
                  style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
                >
                  P{player.slot + 1}
                </div>
                <div>
                  <p className="text-sm font-medium">
                    {player.displayName}
                    {player.id === playerId && (
                      <span className="ml-2 text-[10px] opacity-60">(you)</span>
                    )}
                    {player.isHost && (
                      <span
                        className="ml-2 text-[10px] px-1.5 py-0.5 rounded"
                        style={{ backgroundColor: 'var(--color-oasis-yellow)', color: '#1a1025' }}
                      >
                        HOST
                      </span>
                    )}
                  </p>
                </div>
              </div>
              <span
                className="text-xs font-semibold px-2 py-1 rounded"
                style={{
                  backgroundColor: player.readyState === 'ready' ? 'var(--color-oasis-green)' : 'var(--color-oasis-surface)',
                  color: player.readyState === 'ready' ? '#1a1025' : 'var(--color-oasis-text-muted)',
                }}
              >
                {player.readyState === 'ready' ? '✓ Ready' : 'Not Ready'}
              </span>
            </div>
          ))}

          {/* Empty slots */}
          {Array.from({ length: room.maxPlayers - room.players.length }).map((_, i) => (
            <div
              key={`empty-${i}`}
              className="flex items-center justify-center p-3 rounded-xl border border-dashed"
              style={{ borderColor: 'var(--color-oasis-text-muted)', opacity: 0.3 }}
            >
              <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Open Slot
              </p>
            </div>
          ))}
        </div>

        {/* Actions */}
        <div className="flex gap-3 mb-6">
          <button
            onClick={toggleReady}
            className="flex-1 py-3 rounded-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
            style={{
              backgroundColor: imReady ? 'var(--color-oasis-surface)' : 'var(--color-oasis-green)',
              color: imReady ? 'var(--color-oasis-text-muted)' : '#1a1025',
            }}
          >
            {imReady ? '⏸ Unready' : '✓ Ready Up'}
          </button>
          {isHost && (
            <button
              onClick={startGame}
              disabled={!allReady}
              className="flex-1 py-3 rounded-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98] disabled:opacity-40 disabled:cursor-not-allowed"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
            >
              ▶ Start Game
            </button>
          )}
          <button
            onClick={handleLeave}
            className="py-3 px-6 rounded-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            Leave
          </button>
        </div>

        {/* Chat */}
        <div>
          <p className="text-xs font-semibold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            💬 Room Chat
          </p>
          <div
            className="rounded-xl p-3 mb-2 h-32 overflow-y-auto space-y-1"
            style={{ backgroundColor: 'var(--color-oasis-surface)' }}
          >
            {roomMessages.length === 0 ? (
              <p className="text-xs text-center mt-8" style={{ color: 'var(--color-oasis-text-muted)' }}>
                No messages yet
              </p>
            ) : (
              roomMessages.map((msg, i) => (
                <div key={i} className="text-xs">
                  <span className="font-semibold" style={{ color: msg.userId === playerId ? 'var(--color-oasis-accent-light)' : 'var(--color-oasis-text)' }}>
                    {msg.displayName}:
                  </span>{' '}
                  <span style={{ color: 'var(--color-oasis-text)' }}>{msg.content}</span>
                </div>
              ))
            )}
            <div ref={chatEndRef} />
          </div>
          <form onSubmit={handleChatSubmit} className="flex gap-2">
            <input
              type="text"
              value={chatInput}
              onChange={(e) => setChatInput(e.target.value)}
              placeholder="Say something…"
              maxLength={200}
              className="flex-1 px-3 py-2 rounded-xl text-sm outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
              }}
            />
            <button
              type="submit"
              className="px-4 py-2 rounded-xl font-bold text-sm"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
            >
              Send
            </button>
          </form>
        </div>
      </div>
    </div>
  );
}
