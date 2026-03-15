import { useState, useEffect, useCallback } from 'react';
import { useLobby } from '../context/LobbyContext';
import { HostRoomModal } from '../components/HostRoomModal';
import type { CreateRoomPayload } from '../services/lobby-types';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

const HUNTERS_GAME_ID = 'nds-metroid-prime-hunters';
const HUNTERS_GAME_TITLE = 'Metroid Prime Hunters';

type HuntersMode = 'online' | 'quickmatch' | 'rankings';

interface HuntersRanking {
  displayName: string;
  sessions: number;
}

interface WfcProvider {
  id: string;
  name: string;
  dnsServer: string;
  description: string;
  url: string;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

async function apiFetch<T>(path: string, opts?: RequestInit): Promise<T> {
  const res = await fetch(`${API}${path}`, {
    headers: { 'Content-Type': 'application/json' },
    ...opts,
  });
  if (!res.ok) throw new Error('Request failed');
  return res.json() as Promise<T>;
}

function RankBadge({ rank }: { rank: number }) {
  const medals: Record<number, string> = { 1: '🥇', 2: '🥈', 3: '🥉' };
  return (
    <span className="text-base w-7 text-center inline-block">
      {medals[rank] ?? `#${rank}`}
    </span>
  );
}

// ---------------------------------------------------------------------------
// Online Matches panel (4P deathmatch lobby)
// ---------------------------------------------------------------------------

function OnlineMatchesPanel({ displayName }: { displayName: string }) {
  const { publicRooms, createRoom, joinByCode, listRooms } = useLobby();
  const [hostModalOpen, setHostModalOpen] = useState(false);
  const [status, setStatus] = useState('');
  const [wfcProviders, setWfcProviders] = useState<WfcProvider[]>([]);
  const [selectedProvider, setSelectedProvider] = useState<string>('wiimmfi');

  useEffect(() => {
    listRooms();
    const iv = setInterval(listRooms, 8000);
    return () => clearInterval(iv);
  }, [listRooms]);

  useEffect(() => {
    apiFetch<{ providers: WfcProvider[] }>('/api/wfc/providers')
      .then((d) => { setWfcProviders(d.providers); })
      .catch(() => {/* non-critical */});
  }, []);

  const openRooms = publicRooms.filter(
    (r) => r.gameId === HUNTERS_GAME_ID && r.players.length < r.maxPlayers
  );

  const handleHostConfirm = useCallback(
    (payload: Omit<CreateRoomPayload, 'displayName'>, dn: string) => {
      createRoom(payload, dn);
      setHostModalOpen(false);
      setStatus('Deathmatch room created! Waiting for hunters…');
    },
    [createRoom]
  );

  return (
    <div className="space-y-6">
      {/* WFC auto-config banner */}
      <div
        className="rounded-2xl p-4 flex items-start gap-3"
        style={{ backgroundColor: 'rgba(26,160,198,0.12)', border: '1px solid rgba(26,160,198,0.3)' }}
      >
        <span className="text-2xl flex-shrink-0">🌐</span>
        <div className="flex-1 min-w-0">
          <p className="text-sm font-black" style={{ color: 'var(--color-oasis-blue)' }}>
            Zero-Setup WFC
          </p>
          <p className="text-xs leading-relaxed mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            RetroOasis automatically configures your DNS to use{' '}
            <strong style={{ color: 'var(--color-oasis-text)' }}>Wiimmfi</strong> — no manual
            network config required. Just host or join and play.
          </p>
          {wfcProviders.length > 1 && (
            <div className="mt-2 flex gap-2 flex-wrap">
              {wfcProviders.map((p) => (
                <button
                  key={p.id}
                  onClick={() => setSelectedProvider(p.id)}
                  className="text-[10px] font-bold px-2.5 py-1 rounded-full transition-all"
                  style={{
                    backgroundColor: selectedProvider === p.id ? 'var(--color-oasis-blue)' : 'var(--color-oasis-card)',
                    color: selectedProvider === p.id ? '#fff' : 'var(--color-oasis-text-muted)',
                    border: '1px solid var(--n-border)',
                  }}
                >
                  {p.name}
                </button>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Host action */}
      <div
        className="rounded-2xl p-4 flex items-center justify-between gap-4"
        style={{
          background: 'linear-gradient(135deg,#1a0a2e,#0a1628)',
          border: '1px solid var(--n-border)',
        }}
      >
        <div className="flex items-center gap-3">
          <span className="text-3xl">🔫</span>
          <div>
            <p className="text-sm font-black" style={{ color: '#fff' }}>4P Deathmatch</p>
            <p className="text-xs" style={{ color: 'rgba(255,255,255,0.55)' }}>
              Up to 4 bounty hunters · Wiimmfi online
            </p>
          </div>
        </div>
        <button
          onClick={() => {
            if (!displayName) { setStatus('Enter your display name in Settings.'); return; }
            setHostModalOpen(true);
          }}
          className="text-sm font-black px-4 py-2 rounded-xl flex-shrink-0"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
        >
          Host Match
        </button>
      </div>

      {status && (
        <div className="p-3 rounded-xl text-sm font-semibold" style={{ backgroundColor: 'rgba(230,0,18,0.12)', color: 'var(--color-oasis-accent)' }}>
          {status}
        </div>
      )}

      {/* Open rooms */}
      <div>
        <h3 className="text-xs font-black uppercase tracking-widest mb-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Open Matches ({openRooms.length})
        </h3>
        {openRooms.length === 0 ? (
          <div
            className="rounded-2xl p-5 text-center"
            style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
          >
            <p className="text-2xl mb-1">👾</p>
            <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              No hunters in the field. Host a deathmatch to start!
            </p>
          </div>
        ) : (
          <div className="space-y-2">
            {openRooms.map((room) => (
              <div
                key={room.id}
                className="rounded-2xl p-3 flex items-center justify-between gap-3"
                style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
              >
                <div>
                  <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>{room.name}</p>
                  <p className="text-[11px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    {room.players.length}/{room.maxPlayers} hunters
                  </p>
                </div>
                <button
                  onClick={() => joinByCode(room.roomCode, displayName)}
                  className="text-xs font-black px-3 py-1.5 rounded-xl"
                  style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
                >
                  Join
                </button>
              </div>
            ))}
          </div>
        )}
      </div>

      {hostModalOpen && (
        <HostRoomModal
          preselectedGameId={HUNTERS_GAME_ID}
          onConfirm={handleHostConfirm}
          onClose={() => setHostModalOpen(false)}
        />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Quick Match panel
// ---------------------------------------------------------------------------

function QuickMatchPanel({ displayName }: { displayName: string }) {
  const { publicRooms, createRoom, joinByCode, listRooms } = useLobby();
  const [queued, setQueued] = useState(false);
  const [status, setStatus] = useState('');

  useEffect(() => {
    if (!queued) return;
    const iv = setInterval(listRooms, 5000);
    return () => clearInterval(iv);
  }, [queued, listRooms]);

  // Auto-join when an open room appears while queued
  useEffect(() => {
    if (!queued) return;
    const room = publicRooms.find(
      (r) => r.gameId === HUNTERS_GAME_ID && r.players.length < r.maxPlayers
    );
    if (room) {
      joinByCode(room.roomCode, displayName);
      setQueued(false);
      setStatus('');
    }
  }, [queued, publicRooms, joinByCode, displayName]);

  async function handleQuickMatch() {
    if (!displayName) { setStatus('Enter your display name in Settings.'); return; }
    // Try existing open room first
    const open = publicRooms.find(
      (r) => r.gameId === HUNTERS_GAME_ID && r.players.length < r.maxPlayers
    );
    if (open) {
      joinByCode(open.roomCode, displayName);
      setStatus('Joining open match…');
      return;
    }
    // Auto-host a quick 1v1 room
    createRoom({
      name: `${displayName}'s Quick Match`,
      gameId: HUNTERS_GAME_ID,
      gameTitle: HUNTERS_GAME_TITLE,
      system: 'nds',
      isPublic: true,
      maxPlayers: 2,
      displayName,
    });
    setQueued(true);
    setStatus('No open match found — hosting a quick 1v1 and waiting for a challenger…');
  }

  return (
    <div className="space-y-6">
      <div
        className="rounded-2xl p-6 text-center"
        style={{ background: 'linear-gradient(135deg,#1a0a2e,#0a1628)', border: '1px solid var(--n-border)' }}
      >
        <p className="text-5xl mb-4">🎯</p>
        <h3 className="text-lg font-black mb-2" style={{ color: '#fff' }}>
          Quick Match
        </h3>
        <p className="text-sm mb-4 leading-relaxed" style={{ color: 'rgba(255,255,255,0.55)' }}>
          One click finds or creates a Metroid Prime Hunters deathmatch. No setup needed — WFC is configured automatically.
        </p>
        {status ? (
          <div className="mb-4 p-3 rounded-xl text-sm font-semibold" style={{ backgroundColor: 'rgba(230,0,18,0.15)', color: 'var(--color-oasis-accent)' }}>
            {status}
            {queued && (
              <button
                className="ml-3 text-xs underline"
                onClick={() => { setQueued(false); setStatus(''); }}
              >
                Cancel
              </button>
            )}
          </div>
        ) : null}
        <button
          onClick={handleQuickMatch}
          disabled={queued}
          className="text-sm font-black px-6 py-3 rounded-xl disabled:opacity-50"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
        >
          {queued ? '⏳ Waiting for a hunter…' : '⚡ Quick Match'}
        </button>
      </div>

      <div
        className="rounded-2xl p-4"
        style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
      >
        <p className="text-xs font-black uppercase tracking-widest mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
          How It Works
        </p>
        <ol className="text-xs space-y-1.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <li>1. RetroOasis scans for an open Metroid Prime Hunters match.</li>
          <li>2. Found one? You're joined immediately.</li>
          <li>3. No open match? A 1v1 room is auto-hosted while you wait.</li>
          <li>4. WFC / Wiimmfi DNS is set automatically — no manual config.</li>
        </ol>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Rankings panel
// ---------------------------------------------------------------------------

function RankingsPanel() {
  const [rankings, setRankings] = useState<HuntersRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: HuntersRanking[] }>('/api/leaderboard?metric=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  return (
    <div className="space-y-4">
      <div
        className="rounded-2xl p-4"
        style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
      >
        <h3 className="text-xs font-black uppercase tracking-widest mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          🏆 Bounty Hunter Rankings
        </h3>
        {loading ? (
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>Loading…</p>
        ) : rankings.length === 0 ? (
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No ranked hunters yet. Host a match to be first!
          </p>
        ) : (
          <div className="space-y-2">
            {rankings.map((r, i) => (
              <div key={r.displayName} className="flex items-center gap-3">
                <RankBadge rank={i + 1} />
                <span className="flex-1 text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
                  {r.displayName}
                </span>
                <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {r.sessions} matches
                </span>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Hunter roster */}
      <div
        className="rounded-2xl p-4"
        style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
      >
        <p className="text-xs font-black uppercase tracking-widest mb-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Playable Hunters
        </p>
        <div className="grid grid-cols-2 gap-2 sm:grid-cols-3">
          {[
            { name: 'Samus Aran', type: 'All-rounder', emoji: '💛' },
            { name: 'Kanden', type: 'Berserker', emoji: '🟢' },
            { name: 'Spire', type: 'Rock Form', emoji: '🔴' },
            { name: 'Trace', type: 'Sniper', emoji: '🔵' },
            { name: 'Noxus', type: 'Judicator', emoji: '⚪' },
            { name: 'Sylux', type: 'Shock Coil', emoji: '🟣' },
            { name: 'Weavel', type: 'Halfturret', emoji: '🟠' },
          ].map((h) => (
            <div
              key={h.name}
              className="rounded-xl p-2.5 text-center"
              style={{ backgroundColor: 'var(--color-oasis-surface)', border: '1px solid var(--n-border)' }}
            >
              <p className="text-lg">{h.emoji}</p>
              <p className="text-xs font-bold mt-1" style={{ color: 'var(--color-oasis-text)' }}>{h.name}</p>
              <p className="text-[9px]" style={{ color: 'var(--color-oasis-text-muted)' }}>{h.type}</p>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Main MetroidPage
// ---------------------------------------------------------------------------

export function MetroidPage() {
  const [activeTab, setActiveTab] = useState<HuntersMode>('online');
  const [displayName, setDisplayName] = useState('');

  useEffect(() => {
    const stored = localStorage.getItem('retro-oasis-display-name');
    if (stored) setDisplayName(stored);
  }, []);

  const tabs: { id: HuntersMode; label: string; icon: string }[] = [
    { id: 'online', label: 'Online Matches', icon: '🔫' },
    { id: 'quickmatch', label: 'Quick Match', icon: '⚡' },
    { id: 'rankings', label: 'Rankings', icon: '🏆' },
  ];

  return (
    <div className="space-y-6">
      {/* Header */}
      <div>
        <div className="flex items-center gap-3 mb-1">
          <span className="text-4xl">🌌</span>
          <div>
            <h1 className="text-2xl font-black" style={{ color: '#fff' }}>
              Metroid Prime Hunters
            </h1>
            <p className="text-sm mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              4-player online deathmatch via Wiimmfi · Zero-setup WFC auto-config
            </p>
          </div>
        </div>

        {/* Feature chips */}
        <div className="flex flex-wrap gap-2 mt-3">
          {[
            { label: '4P Deathmatch', color: 'var(--color-oasis-accent)' },
            { label: 'Wiimmfi Online', color: 'var(--color-oasis-blue)' },
            { label: '7 Hunters', color: 'var(--color-oasis-green)' },
            { label: 'NDS', color: 'var(--color-oasis-text-muted)' },
          ].map((chip) => (
            <span
              key={chip.label}
              className="text-[11px] font-bold px-2.5 py-1 rounded-full"
              style={{ backgroundColor: 'var(--color-oasis-card)', color: chip.color, border: '1px solid var(--n-border)' }}
            >
              {chip.label}
            </span>
          ))}
        </div>
      </div>

      {/* Tabs */}
      <div className="flex gap-1 p-1 rounded-2xl" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="flex-1 flex items-center justify-center gap-2 py-2.5 rounded-xl text-sm font-bold transition-all"
            style={{
              backgroundColor: activeTab === tab.id ? 'var(--color-oasis-accent)' : 'transparent',
              color: activeTab === tab.id ? '#fff' : 'var(--color-oasis-text-muted)',
            }}
          >
            <span>{tab.icon}</span>
            <span>{tab.label}</span>
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'online' && <OnlineMatchesPanel displayName={displayName} />}
      {activeTab === 'quickmatch' && <QuickMatchPanel displayName={displayName} />}
      {activeTab === 'rankings' && <RankingsPanel />}
    </div>
  );
}
