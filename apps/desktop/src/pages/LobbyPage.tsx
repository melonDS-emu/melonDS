import { useState, useEffect, useRef, useMemo, type FormEvent } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';
import { MOCK_GAMES } from '../data/mock-games';
import type { ConnectionQuality } from '../services/lobby-types';
import { DSControlsGuide } from '../components/DSControlsGuide';
import { useVoiceChat } from '../lib/voice-chat';
import { themeGradient, themeAccent, themeLabel } from '../lib/events-service';

function qualityDot(quality: ConnectionQuality): { color: string; label: string; text: string } {
  switch (quality) {
    case 'excellent': return { color: '#4ade80', label: '●●●●', text: 'Excellent' };
    case 'good':      return { color: '#86efac', label: '●●●○', text: 'Good' };
    case 'fair':      return { color: '#fbbf24', label: '●●○○', text: 'Fair' };
    case 'poor':      return { color: '#f87171', label: '●○○○', text: 'Poor' };
    default:          return { color: 'var(--color-oasis-text-muted)', label: '○○○○', text: 'Unknown' };
  }
}

/** Returns a human-friendly party hint for the given game/system. */
function getSessionHint(system: string, maxPlayers: number, gameTitle: string, isDsiWare?: boolean): string | null {
  const title = gameTitle.toLowerCase();
  if (system === 'n64' || system === 'N64') {
    if (maxPlayers >= 4) {
      if (title.includes('kart') || title.includes('party') || title.includes('racing')) {
        return `🏎️ This session needs all 4 players for the full experience — invite friends!`;
      }
      if (title.includes('smash') || title.includes('goldeneye')) {
        return `⚔️ Fill all 4 slots for maximum chaos. Best played with a full lobby!`;
      }
      return `🎮 N64 party game — best with ${maxPlayers} players. Share your room code!`;
    }
    return `🎮 N64 session — grab your N64 adapter or use a gamepad with left-stick analog support.`;
  }
  if (system === 'nds' || system === 'NDS') {
    if (isDsiWare) {
      return `🟦 DSiWare session — melonDS launches in DSi mode. Ensure DSi BIOS files are in your melonDS config folder.`;
    }
    if (title.includes('pokemon') || title.includes('pokémon')) {
      return `🌐 Pokémon WFC session — trades and battles via Wiimmfi. Start the emulator after everyone readies up!`;
    }
    if (title.includes('kart')) {
      return `🏎️ DS multiplayer kart racing — up to ${maxPlayers} players via WFC. Touchscreen controls the track map!`;
    }
    if (title.includes('hunters') || title.includes('metroid')) {
      return `🔫 DS first-person shooter — use mouse on the bottom screen to aim. Everyone needs a steady hand!`;
    }
    if (title.includes('tetris')) {
      return `🧩 Tetris DS — send garbage lines to knock out your opponents. Fast reaction time wins!`;
    }
    if (title.includes('party')) {
      return `🎉 DS party game — touch mini-games ahead! Mouse clicks drive the bottom screen.`;
    }
    if (maxPlayers >= 4) {
      return `🎮 NDS session — dual-screen layout. Click the bottom screen area to interact with touch controls.`;
    }
    return `📱 NDS dual-screen session — the bottom screen responds to mouse clicks (touchscreen emulation).`;
  }
  return null;
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
    sessionToken,
    ws,
  } = useLobby();

  const [chatInput, setChatInput] = useState('');
  const [roomCodeCopied, setRoomCodeCopied] = useState(false);
  const [showDsGuide, setShowDsGuide] = useState(false);
  const chatEndRef = useRef<HTMLDivElement>(null);

  // Voice chat — resolve peer IDs (all players in the room except ourselves)
  const peerIds = useMemo(
    () =>
      (currentRoom?.players ?? [])
        .filter((p) => p.id !== playerId)
        .map((p) => p.id),
    [currentRoom?.players, playerId],
  );
  const voiceChat = useVoiceChat(ws, playerId ?? '', peerIds);

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

  function handleChatSubmit(e: FormEvent<HTMLFormElement>) {
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
        {error && (
          <div
            className="mb-3 px-4 py-2 rounded-xl text-sm font-semibold flex items-center justify-between"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: '#f87171' }}
          >
            <span>⚠️ {error}</span>
            <button onClick={clearError} className="ml-4 text-xs opacity-70">✕</button>
          </div>
        )}
        <div className="rounded-2xl p-6 text-center" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {error ? 'Could not join — check the room code and try again.' : 'Joining lobby…'}
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
  const allReady = room.players.length >= 1 && room.players.every((p) => p.readyState === 'ready');
  const myQuality = qualityDot(myPlayer?.connectionQuality ?? 'unknown');

  // Game metadata for context card
  const gameData = MOCK_GAMES.find((g) => g.id === room.gameId);
  const sessionHint = getSessionHint(room.system, room.maxPlayers, room.gameTitle, gameData?.isDsiWare);
  const slotsNeeded = room.maxPlayers - room.players.length;
  const isNds = room.system === 'nds' || room.system === 'NDS';
  const isDsiWare = isNds && (gameData?.isDsiWare === true);

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

      {/* Session live banner — appears when game has started */}
      {relayInfo && (
        <div
          className="mb-3 px-4 py-3 rounded-xl text-sm"
          style={{ backgroundColor: 'var(--color-oasis-green)', color: '#1a1025' }}
        >
          <p className="font-bold">🎮 Session Live — Everyone's in!</p>
          <p className="text-xs mt-1 opacity-80">
            Launch your emulator and use the netplay address shown in Connection Diagnostics below.
          </p>
        </div>
      )}

      {/* Game summary card */}
      <div
        className="mb-3 rounded-2xl px-4 py-3 flex items-center gap-4"
        style={{ backgroundColor: 'var(--color-oasis-card)' }}
      >
        <div
          className="w-14 h-14 rounded-xl flex items-center justify-center text-3xl flex-shrink-0"
          style={{ backgroundColor: 'var(--color-oasis-surface)' }}
        >
          {gameData?.coverEmoji ?? '🎮'}
        </div>
        <div className="flex-1 min-w-0">
          <div className="flex items-center gap-2 mb-0.5">
            <span
              className="px-1.5 py-0.5 rounded text-[9px] font-bold"
              style={{ backgroundColor: gameData?.systemColor ?? 'var(--color-oasis-accent)', color: 'white' }}
            >
              {room.system.toUpperCase()}
            </span>
            {room.maxPlayers >= 4 && (
              <span
                className="px-1.5 py-0.5 rounded-full text-[9px] font-bold"
                style={{ backgroundColor: 'var(--color-oasis-yellow)', color: '#1a1025' }}
              >
                Up to {room.maxPlayers}P
              </span>
            )}
          </div>
          <p className="text-sm font-bold truncate">{room.gameTitle}</p>
          {sessionHint && (
            <p className="text-[11px] mt-0.5 leading-snug" style={{ color: 'var(--color-oasis-accent-light)' }}>
              {sessionHint}
            </p>
          )}
          {isNds && !isDsiWare && (
            <p className="text-[10px] mt-1 opacity-70" style={{ color: 'var(--color-oasis-text-muted)' }}>
              💡 Press <kbd className="px-1 py-0.5 rounded text-[9px] font-mono" style={{ backgroundColor: 'var(--color-oasis-surface)' }}>F11</kbd> in melonDS to swap screens
            </p>
          )}
          {isDsiWare && (
            <p className="text-[10px] mt-1 font-semibold" style={{ color: '#7dd3fc' }}>
              🟦 DSi Mode — requires DSi BIOS files in your melonDS config folder
            </p>
          )}
        </div>
      </div>

      <div
        className="rounded-2xl p-6"
        style={{
          background: room.theme ? themeGradient(room.theme) : 'var(--color-oasis-card)',
          border: room.theme ? `1px solid ${themeAccent(room.theme)}44` : undefined,
        }}
      >
        <div className="flex items-center justify-between mb-4">
          <div>
            <h1 className="text-xl font-bold">{room.name}</h1>
            <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {room.gameTitle} · {room.system.toUpperCase()}
            </p>
            {room.theme && (
              <span
                className="text-[10px] px-1.5 py-0.5 rounded mt-1 inline-block font-semibold"
                style={{ backgroundColor: `${themeAccent(room.theme)}22`, color: themeAccent(room.theme) }}
              >
                {themeLabel(room.theme)}
              </span>
            )}
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
          {sessionToken && (
            <div className="mt-2 pt-2" style={{ borderTop: '1px solid rgba(255,255,255,0.06)' }}>
              <p className="text-[11px] mb-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Session token (for manual emulator setup)
              </p>
              <p className="font-mono text-[11px] break-all" style={{ color: 'var(--color-oasis-text)' }}>
                {sessionToken}
              </p>
            </div>
          )}
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
              style={{ borderColor: isNds ? '#E87722' : 'var(--color-oasis-text-muted)', opacity: isNds ? 0.5 : 0.3 }}
            >
              {isNds ? (
                <span className="text-xs flex items-center gap-2" style={{ color: '#E87722' }}>
                  <span className="inline-block animate-pulse">📡</span>
                  Searching for DS nearby…
                </span>
              ) : (
                <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  Open Slot
                </p>
              )}
            </div>
          ))}
        </div>

        {/* DS wireless "searching" banner — shown for NDS sessions with open slots */}
        {isNds && slotsNeeded > 0 && !relayInfo && (
          <div
            className="mb-4 px-4 py-3 rounded-xl"
            style={{ backgroundColor: 'rgba(232,119,34,0.1)', border: '1px solid rgba(232,119,34,0.3)' }}
          >
            <div className="flex items-center gap-2 mb-1">
              <span className="text-sm font-bold" style={{ color: '#E87722' }}>📡 DS Wireless</span>
              <span className="flex gap-0.5">
                {[0, 1, 2].map((bar) => (
                  <span
                    key={bar}
                    className="inline-block rounded-sm animate-pulse"
                    style={{
                      width: '3px',
                      height: `${6 + bar * 4}px`,
                      backgroundColor: '#E87722',
                      opacity: 0.6 + bar * 0.15,
                      animationDelay: `${bar * 0.2}s`,
                    }}
                  />
                ))}
              </span>
            </div>
            <p className="text-xs" style={{ color: '#E87722', opacity: 0.8 }}>
              Scanning for {slotsNeeded} player{slotsNeeded > 1 ? 's' : ''} — share room code{' '}
              <button
                className="font-bold underline"
                style={{ color: '#E87722', background: 'none', border: 'none', padding: 0, cursor: 'pointer' }}
                onClick={() => handleCopyCode(room.roomCode)}
              >
                {room.roomCode}
              </button>{' '}
              to invite friends!
            </p>
          </div>
        )}

        {/* Waiting for players nudge (non-NDS sessions) */}
        {!isNds && slotsNeeded > 0 && !relayInfo && (
          <div
            className="mb-4 px-3 py-2 rounded-xl text-xs text-center"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            👋 {slotsNeeded} open slot{slotsNeeded > 1 ? 's' : ''} — share your room code{' '}
            <button
              className="font-bold underline"
              style={{ color: 'var(--color-oasis-accent-light)', background: 'none', border: 'none', padding: 0, cursor: 'pointer' }}
              onClick={() => handleCopyCode(room.roomCode)}
            >
              {room.roomCode}
            </button>{' '}
            to invite friends!
          </div>
        )}

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

        {/* Actions — only show pre-game controls while waiting */}
        {!isSpectating && room.status === 'waiting' && (
          <div className="mb-6">
            <div className="flex gap-3">
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
            {/* "Waiting" hint is only relevant when there are other players to wait for */}
            {isHost && !allReady && room.players.length > 1 && (
              <p className="text-xs mt-2 text-center" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Waiting for all players to ready up…
              </p>
            )}
          </div>
        )}

        {/* Leave button shown separately once in-game (ready/start controls are hidden) */}
        {!isSpectating && room.status !== 'waiting' && (
          <div className="mb-6">
            <button
              onClick={handleLeave}
              className="w-full py-3 rounded-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
            >
              Leave Room
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

        {/* Voice Chat */}
        <div className="mb-4 rounded-xl p-3" style={{ backgroundColor: 'var(--color-oasis-surface)' }}>
          <p className="text-xs font-semibold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            🎙️ Voice Chat
          </p>
          {voiceChat.error && (
            <p className="text-xs mb-2" style={{ color: '#f87171' }}>{voiceChat.error}</p>
          )}
          <div className="flex items-center gap-2">
            {!voiceChat.active ? (
              <button
                type="button"
                onClick={() => void voiceChat.enable()}
                className="px-3 py-1.5 rounded-lg text-xs font-semibold transition-opacity hover:opacity-80"
                style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
              >
                🎤 Join Voice
              </button>
            ) : (
              <>
                <button
                  type="button"
                  onClick={voiceChat.toggleMute}
                  className="px-3 py-1.5 rounded-lg text-xs font-semibold transition-opacity hover:opacity-80"
                  style={{
                    backgroundColor: voiceChat.muted ? '#4b5563' : 'var(--color-oasis-accent)',
                    color: 'white',
                  }}
                >
                  {voiceChat.muted ? '🔇 Unmute' : '🎤 Mute'}
                </button>
                <button
                  type="button"
                  onClick={voiceChat.disable}
                  className="px-3 py-1.5 rounded-lg text-xs font-semibold transition-opacity hover:opacity-80"
                  style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.1)' }}
                >
                  Leave Voice
                </button>
              </>
            )}
            {/* Per-player talking indicators */}
            {voiceChat.active && peerIds.length > 0 && (
              <div className="flex items-center gap-1 ml-2">
                {peerIds.map((pid) => {
                  const player = currentRoom?.players.find((p) => p.id === pid);
                  const talking = voiceChat.peerActivity[pid];
                  return (
                    <span
                      key={pid}
                      title={`${player?.displayName ?? pid}${talking ? ' (talking)' : ''}`}
                      className="text-xs px-1.5 py-0.5 rounded-full transition-colors"
                      style={{
                        backgroundColor: talking ? '#4ade80' : 'var(--color-oasis-surface)',
                        color: talking ? '#000' : 'var(--color-oasis-text-muted)',
                        border: '1px solid rgba(255,255,255,0.1)',
                        fontSize: '10px',
                      }}
                    >
                      {talking ? '🔊' : '○'} {player?.displayName ?? pid.slice(0, 6)}
                    </span>
                  );
                })}
              </div>
            )}
          </div>
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
                <div key={`${msg.sentAt}-${msg.userId}-${i}`} className="text-xs">
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

        {/* DS Controls Guide — shown for NDS sessions */}
        {isNds && (
          <div className="mt-4">
            <button
              onClick={() => setShowDsGuide((v) => !v)}
              className="w-full py-2 rounded-xl text-xs font-semibold flex items-center justify-center gap-2 transition-opacity hover:opacity-80"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: '#E87722' }}
            >
              📱 {showDsGuide ? 'Hide' : 'Show'} DS Controls &amp; Setup Guide
            </button>
            {showDsGuide && (
              <div className="mt-3">
                <DSControlsGuide compact showDsiSection={isDsiWare} />
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
