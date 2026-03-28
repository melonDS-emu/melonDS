import { useState, useEffect, useRef, type KeyboardEvent, type CSSProperties } from 'react';
import { useLobby } from '../context/LobbyContext';

const BACKEND_URL: string =
  (import.meta.env?.VITE_BACKEND_URL as string | undefined) ?? '';

interface GlobalChatMessage {
  id: string;
  playerId: string;
  displayName: string;
  text: string;
  timestamp: string;
}

export function GlobalChatPage() {
  const { ws, playerId } = useLobby();
  const [messages, setMessages] = useState<GlobalChatMessage[]>([]);
  const [inputText, setInputText] = useState('');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const bottomRef = useRef<HTMLDivElement>(null);

  // Derive display name from playerId (best-effort; the server echoes it back)
  const [displayName] = useState<string>(() => {
    try {
      return localStorage.getItem('retro-oasis-display-name') ?? 'Player';
    } catch {
      return 'Player';
    }
  });

  // Fetch recent messages on mount
  useEffect(() => {
    const url = `${BACKEND_URL}/api/chat?limit=100`;
    fetch(url)
      .then((r) => r.json())
      .then((data: { messages: GlobalChatMessage[] }) => {
        setMessages(data.messages ?? []);
        setLoading(false);
      })
      .catch((err: unknown) => {
        setError(String(err));
        setLoading(false);
      });
  }, []);

  // Subscribe to live global-chat-message WS events
  useEffect(() => {
    if (!ws) return;
    const handler = (event: MessageEvent<string>) => {
      try {
        const msg = JSON.parse(event.data) as { type: string } & Partial<GlobalChatMessage>;
        if (
          msg.type === 'global-chat-message' &&
          msg.id && msg.playerId && msg.displayName && msg.text && msg.timestamp
        ) {
          const chatMsg: GlobalChatMessage = {
            id: msg.id,
            playerId: msg.playerId,
            displayName: msg.displayName,
            text: msg.text,
            timestamp: msg.timestamp,
          };
          // Cap client-side history at 500 to mirror server ring-buffer limit
          setMessages((prev) => {
            const next = [...prev, chatMsg];
            return next.length > 500 ? next.slice(next.length - 500) : next;
          });
        }
      } catch {
        // ignore parse errors
      }
    };
    ws.addEventListener('message', handler);
    return () => ws.removeEventListener('message', handler);
  }, [ws]);

  // Auto-scroll to bottom when messages arrive
  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages]);

  function handleSend() {
    const text = inputText.trim();
    if (!text || !ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({ type: 'send-global-chat', text }));
    setInputText('');
  }

  function handleKeyDown(e: KeyboardEvent) {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  }

  return (
    <div className="max-w-3xl flex flex-col" style={{ height: 'calc(100vh - 56px)' }}>
      {/* Header */}
      <div className="mb-4 flex-shrink-0">
        <h1 className="text-2xl font-black tracking-tight" style={{ color: '#fff' }}>
          💬 Global Chat
        </h1>
        <p className="text-xs font-semibold mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Talk with everyone in the lobby
        </p>
      </div>

      {/* Message list */}
      <div
        className="flex-1 overflow-y-auto rounded-2xl p-4 space-y-2 min-h-0"
        style={{ backgroundColor: 'var(--color-oasis-surface)', border: '1px solid var(--n-border)' }}
      >
        {loading && (
          <p className="text-center py-8 text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Loading…
          </p>
        )}
        {error && (
          <p className="text-center py-8 text-sm font-semibold" style={{ color: '#f87171' }}>
            ⚠️ {error}
          </p>
        )}
        {!loading && !error && messages.length === 0 && (
          <p className="text-center py-8 text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No messages yet — say hello!
          </p>
        )}
        {messages.map((msg) => {
          const isOwn = msg.playerId === playerId;
          return (
            <div key={msg.id} className={`flex gap-2 ${isOwn ? 'flex-row-reverse' : ''}`}>
              <div
                className="w-7 h-7 rounded-full flex items-center justify-center text-xs font-black flex-shrink-0"
                style={{
                  backgroundColor: isOwn ? 'var(--color-oasis-accent)' : 'var(--color-oasis-card)',
                  color: isOwn ? '#fff' : 'var(--color-oasis-text-muted)',
                }}
              >
                {msg.displayName.charAt(0).toUpperCase()}
              </div>
              <div className={`flex flex-col max-w-[70%] ${isOwn ? 'items-end' : ''}`}>
                <span className="text-[10px] font-bold mb-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {msg.displayName} · {new Date(msg.timestamp).toLocaleTimeString()}
                </span>
                <div
                  className="px-3 py-2 rounded-2xl text-sm font-medium break-words"
                  style={{
                    backgroundColor: isOwn ? 'var(--color-oasis-accent)' : 'var(--color-oasis-card)',
                    color: isOwn ? '#fff' : 'var(--color-oasis-text)',
                  }}
                >
                  {msg.text}
                </div>
              </div>
            </div>
          );
        })}
        <div ref={bottomRef} />
      </div>

      {/* Input bar */}
      <div className="mt-3 flex gap-2 flex-shrink-0">
        <div className="flex-1 relative">
          <span
            className="absolute left-3 top-1/2 -translate-y-1/2 text-xs font-bold"
            style={{ color: 'var(--color-oasis-text-muted)' }}
          >
            {displayName}:
          </span>
          <input
            type="text"
            value={inputText}
            onChange={(e) => setInputText(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder="Type a message…"
            maxLength={500}
            className="w-full pl-20 pr-4 py-2.5 rounded-full text-sm font-medium outline-none focus:ring-2"
            style={{
              backgroundColor: 'var(--color-oasis-card)',
              border: '1px solid var(--n-border)',
              color: 'var(--color-oasis-text)',
              '--tw-ring-color': 'var(--color-oasis-accent)',
            } as CSSProperties}
          />
        </div>
        <button
          onClick={handleSend}
          disabled={!inputText.trim() || !ws || ws.readyState !== WebSocket.OPEN}
          className="px-5 py-2.5 rounded-full text-sm font-black transition-all hover:brightness-110 active:scale-[0.97] disabled:opacity-40"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
        >
          Send
        </button>
      </div>
    </div>
  );
}
