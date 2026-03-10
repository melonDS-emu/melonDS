import { useState, useEffect, useRef } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';
import type { ConnectionQuality } from '../services/lobby-types';

function qualityDot(quality: ConnectionQuality): { color: string; label: string; text: string } {
  switch (quality) {
    case 'excellent': return { color: '#4ade80', label: '●●●●', text: 'Excellent' };
    case 'good':      return { color: '#86efac', label: '●●●○', text: 'Good' };
    case 'fair':      return { color: '#fbbf24', label: '●●○○', text: 'Fair' };
    case 'poor':      return { color: '#f87171', label: '●○○○', text: 'Poor' };
    default:          return { color: 'var(--color-oasis-text-muted)', label: '○○○○', text: 'Unknown' };
  }
}

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
    latencyMs,
    relayInfo,
  } = useLobby();

  const [chatInput, setChatInput] = useState('');
  const [roomCodeCopied, setRoomCodeCopied] = useState(false);
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

  function handleCopyCode(code: string) {
    navigator.clipboard.writeText(code).then(() => {
      setRoomCodeCopied(true);
      setTimeout(() => setRoomCodeCopied(false), 2000);
    });
  }

  // Use real room or show loading
  const room = currentRoom;
  const roomMessages = chatMessages.filter((m) => m.roomId === (room?.id ?? lobbyId));

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
  const mySpectator = room.spectators?.find((s) => s.id === playerId);
  const isSpectating = !!mySpectator && !myPlayer;
  const isHost = myPlayer?.isHost ?? false;
  const imReady = myPlayer?.readyState === 'ready';
  const allReady = room.players.length > 1 && room.players.every((p) => p.readyState === 'ready');
  const myQuality = qualityDot(myPlayer?.connectionQuality ?? 'unknown');

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

      {/* Relay info banner — appears when game is starting */}
      {relayInfo && (
        <div
          className="mb-3 px-4 py-3 rounded-xl text-sm"
          style={{ backgroundColor: 'var(--color-oasis-green)', color: '#1a1025' }}
        >
          <p className="font-bold">🕹️ Game Starting — Netplay Relay Active</p>
          <p className="text-xs mt-1 font-mono">
            Connect your emulator to: <strong>{relayInfo.host}:{relayInfo.port}</strong>
          </p>
          <p className="text-xs mt-1 opacity-70">
            Each player should configure their emulator netplay to use this relay address.
          </p>
        </div>
      )}

      <div className="rounded-2xl p-6" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
        <div className="flex items-center justify-between mb-4">
          <div>
            <h1 className="text-xl font-bold">{room.name}</h1>
            <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {room.gameTitle} · {room.system.toUpperCase()}
            </p>
            {isSpectating && (
              <span
                className="text-[10px] px-1.5 py-0.5 rounded mt-1 inline-block font-semibold"
                style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
              >
                👁 Spectating
              </span>
            )}
          </div>
          <div className="text-right">
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>Room Code</p>
            <button
              className="text-sm font-mono font-bold cursor-pointer hover:opacity-80 transition-opacity"
              style={{ color: 'var(--color-oasis-accent-light)', background: 'none', border: 'none', padding: 0 }}
              title="Click to copy room code"
              onClick={() => handleCopyCode(room.roomCode)}
            >
              {roomCodeCopied ? '✓ Copied!' : room.roomCode}
            </button>
            {latencyMs !== null && (
              <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
                🌐 {latencyMs}ms
              </p>
            )}
          </div>
        </div>

        {/* Connection diagnostics */}
        <div
          className="rounded-xl p-3 mb-4"
          style={{ backgroundColor: 'var(--color-oasis-surface)' }}
        >
          <div className="flex items-center justify-between">
            <div>
              <p className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Connection diagnostics
              </p>
              <p className="text-sm font-medium flex items-center gap-2" style={{ color: 'var(--color-oasis-text)' }}>
                {latencyMs !== null ? `${latencyMs}ms ping` : 'Measuring ping…'}
                <span className="text-[11px] font-semibold" style={{ color: myQuality.color }}>
                  {myQuality.label} {myQuality.text}
                </span>
              </p>
            </div>
            {relayInfo && (
              <div className="text-right">
                <p className="text-[11px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  Relay endpoint
                </p>
                <p className="font-mono text-xs" style={{ color: 'var(--color-oasis-text)' }}>
                  {relayInfo.host}:{relayInfo.port}
                </p>
              </div>
            )}
          </div>
        </div>

        {/* Player list */}
        <div className="space-y-2 mb-4">
          <p className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Players ({room.players.length}/{room.maxPlayers})
          </p>
          {room.players.map((player) => {
            const dot = qualityDot(player.connectionQuality);
            return (
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
                    {player.latencyMs !== undefined && (
                      <p className="text-[10px]" style={{ color: dot.color }} title={dot.text}>
                        {dot.label} {dot.text && `${dot.text} · `}{player.latencyMs}ms
                      </p>
                    )}
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
            );
          })}

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

        {/* Spectators section */}
        {room.spectators && room.spectators.length > 0 && (
          <div className="mb-4">
            <p className="text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              👁 Spectators ({room.spectators.length})
            </p>
            <div className="flex flex-wrap gap-1">
              {room.spectators.map((spec) => (
                <span
                  key={spec.id}
                  className="text-xs px-2 py-1 rounded"
                  style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
                >
                  {spec.displayName}
                  {spec.id === playerId && <span className="ml-1 opacity-60">(you)</span>}
                </span>
              ))}
            </div>
          </div>
        )}

        {/* Actions */}
        {!isSpectating && (
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
        )}

        {isSpectating && (
          <div className="mb-6">
            <button
              onClick={handleLeave}
              className="w-full py-3 rounded-xl font-bold"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
            >
              Leave
            </button>
          </div>
        )}

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
