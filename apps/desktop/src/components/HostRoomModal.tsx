import { useState, useEffect, type FormEvent } from 'react';
import { useGames } from '../lib/use-games';
import { ROOM_THEMES, themeAccent } from '../lib/events-service';
import { getGamePresets, type SessionPreset } from '../lib/session-presets';
import type { CreateRoomPayload } from '../services/lobby-types';
import { SYSTEM_ONLINE_SUPPORT, onlineSupportBadgeColor, onlineSupportIcon, onlineSupportSummary } from '../lib/game-capability';

const NDS_LAYOUT_KEY = 'retro-oasis-nds-screen-layout';
/** NDS brand colour — matches systemColor used across the app for NDS games. */
const NDS_COLOR = '#E87722';

type NdsScreenLayout = 'stacked' | 'side-by-side' | 'top-focus' | 'bottom-focus';

const NDS_LAYOUT_OPTIONS: { value: NdsScreenLayout; label: string; description: string }[] = [
  { value: 'stacked',      label: 'Stacked',       description: 'Top screen above, touch screen below (default)' },
  { value: 'side-by-side', label: 'Side by Side',  description: 'Both screens next to each other' },
  { value: 'top-focus',    label: 'Top Focus',     description: 'Top screen enlarged, touch screen small' },
  { value: 'bottom-focus', label: 'Bottom Focus',  description: 'Touch screen enlarged (great for Phantom Hourglass)' },
];

interface HostRoomModalProps {
  preselectedGameId?: string;
  onConfirm: (payload: Omit<CreateRoomPayload, 'displayName'>, displayName: string) => void;
  onClose: () => void;
}

export function HostRoomModal({ preselectedGameId, onConfirm, onClose }: HostRoomModalProps) {
  const { data: games } = useGames();
  const [roomName, setRoomName] = useState('');
  const [selectedGameId, setSelectedGameId] = useState('');
  const [displayName, setDisplayName] = useState(
    () => localStorage.getItem('retro-oasis-display-name') ?? ''
  );
  const [isPublic, setIsPublic] = useState(true);
  const [seeded, setSeeded] = useState(false);
  const [theme, setTheme] = useState<string>('classic');
  const [rankMode, setRankMode] = useState<'casual' | 'ranked'>('casual');
  const [ndsLayout, setNdsLayout] = useState<NdsScreenLayout>(
    () => (localStorage.getItem(NDS_LAYOUT_KEY) as NdsScreenLayout | null) ?? 'stacked'
  );
  const [selectedPresetId, setSelectedPresetId] = useState<string>('');
  const [showAdvanced, setShowAdvanced] = useState(false);

  // Seed form defaults once the game list becomes available
  useEffect(() => {
    if (seeded || games.length === 0) return;
    const defaultGame =
      (preselectedGameId ? games.find((g) => g.id === preselectedGameId) : null) ?? games[0];
    setSelectedGameId(defaultGame.id);
    setRoomName(`${defaultGame.title} Room`);
    setSeeded(true);
  }, [games, preselectedGameId, seeded]);

  const selectedGame = games.find((g) => g.id === selectedGameId) ?? games[0];
  const isNds = selectedGame?.system === 'NDS';
  const presets = selectedGame ? getGamePresets(selectedGame) : [];

  // Resolve online support level for the currently selected game's system
  const selectedSystemKey = selectedGame?.system?.toLowerCase() ?? '';
  const onlineLevel = SYSTEM_ONLINE_SUPPORT[selectedSystemKey] ?? 'supported';
  const onlineBadge = onlineSupportBadgeColor(onlineLevel);

  function handleGameChange(gameId: string) {
    const game = games.find((g) => g.id === gameId);
    setSelectedGameId(gameId);
    if (game) setRoomName(`${game.title} Room`);
    setSelectedPresetId('');
  }

  function handlePresetSelect(preset: SessionPreset) {
    setSelectedPresetId(preset.id);
  }

  function handleSubmit(e: FormEvent<HTMLFormElement>) {
    e.preventDefault();
    if (!displayName.trim() || !selectedGame) return;
    localStorage.setItem('retro-oasis-display-name', displayName.trim());
    if (isNds) {
      localStorage.setItem(NDS_LAYOUT_KEY, ndsLayout);
    }

    const selectedPreset = presets.find((p) => p.id === selectedPresetId);
    const maxPlayers = selectedPreset?.playerCount ?? selectedGame.maxPlayers;

    onConfirm(
      {
        name: roomName.trim() || `${selectedGame.title} Room`,
        gameId: selectedGame.id,
        gameTitle: selectedGame.title,
        system: selectedGame.system,
        isPublic,
        maxPlayers,
        theme: theme !== 'classic' ? theme : undefined,
        rankMode,
      },
      displayName.trim()
    );
  }

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center p-4"
      style={{ backgroundColor: 'rgba(0,0,0,0.7)' }}
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div
        className="w-full max-w-md rounded-2xl p-6 overflow-y-auto"
        style={{ backgroundColor: 'var(--color-oasis-card)', maxHeight: '90vh' }}
      >
        <h2 className="text-xl font-bold mb-4">🎮 Play with Friends</h2>

        <form onSubmit={handleSubmit} className="space-y-4">
          {/* Display name */}
          <div>
            <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Your Name
            </label>
            <input
              type="text"
              value={displayName}
              onChange={(e) => setDisplayName(e.target.value)}
              placeholder="Enter your display name"
              maxLength={20}
              required
              className="w-full px-3 py-2 rounded-xl text-sm outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
                border: '1px solid transparent',
              }}
            />
          </div>

          {/* Game selection */}
          <div>
            <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Game
            </label>
            <select
              value={selectedGameId}
              onChange={(e) => handleGameChange(e.target.value)}
              className="w-full px-3 py-2 rounded-xl text-sm outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
              }}
            >
              {games.map((g) => (
                <option key={g.id} value={g.id}>
                  {g.coverEmoji} {g.title} ({g.system}) — {g.maxPlayers}P
                </option>
              ))}
            </select>
            {/* Online support indicator for selected system */}
            {selectedGame && (
              <div
                className="mt-2 px-3 py-1.5 rounded-lg text-[11px] font-semibold flex items-center gap-1.5"
                style={{ backgroundColor: onlineBadge.bg, color: onlineBadge.text }}
              >
                <span>{onlineSupportIcon(onlineLevel)}</span>
                <span>{onlineSupportSummary(onlineLevel)}</span>
              </div>
            )}
          </div>

          {/* Session presets */}
          {presets.length > 0 && (
            <div>
              <label className="block text-xs font-semibold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
                ⚡ Quick Start
              </label>
              <div className="flex gap-2 flex-wrap">
                {presets.map((preset) => {
                  const selected = selectedPresetId === preset.id;
                  return (
                    <button
                      key={preset.id}
                      type="button"
                      onClick={() => handlePresetSelect(preset)}
                      className="flex-1 min-w-[100px] px-3 py-2 rounded-xl text-xs font-bold transition-all text-left"
                      style={{
                        backgroundColor: selected ? 'rgba(230,0,18,0.18)' : 'var(--color-oasis-surface)',
                        color: selected ? 'var(--color-oasis-accent-light)' : 'var(--color-oasis-text-muted)',
                        border: `1px solid ${selected ? 'var(--color-oasis-accent)' : 'transparent'}`,
                      }}
                    >
                      <span className="text-base mr-1">{preset.emoji}</span>
                      <span className="font-black">{preset.label}</span>
                      <p className="text-[10px] font-normal mt-0.5 opacity-75">{preset.description}</p>
                    </button>
                  );
                })}
              </div>
            </div>
          )}

          {/* Room name */}
          <div>
            <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Room Name
            </label>
            <input
              type="text"
              value={roomName}
              onChange={(e) => setRoomName(e.target.value)}
              placeholder="Room name"
              maxLength={40}
              className="w-full px-3 py-2 rounded-xl text-sm outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
              }}
            />
          </div>

          {/* Ranked / Casual mode — always visible (key game-play decision) */}
          <div>
            <label className="block text-xs font-semibold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
              🏅 Match Mode
            </label>
            <div className="flex gap-2">
              {(['casual', 'ranked'] as const).map(mode => {
                const selected = rankMode === mode;
                const accent = mode === 'ranked' ? '#e60012' : '#60a5fa';
                return (
                  <button
                    key={mode}
                    type="button"
                    onClick={() => setRankMode(mode)}
                    className="flex-1 px-3 py-2 rounded-xl text-xs font-bold capitalize transition-all"
                    style={{
                      backgroundColor: selected ? `${accent}22` : 'var(--color-oasis-surface)',
                      color: selected ? accent : 'var(--color-oasis-text-muted)',
                      border: `1px solid ${selected ? accent : 'transparent'}`,
                    }}
                  >
                    {mode === 'ranked' ? '🏆 Ranked' : '🎮 Casual'}
                    <p className="text-[10px] font-normal mt-0.5 opacity-75">
                      {mode === 'ranked' ? 'Affects ELO scores' : 'No ELO changes'}
                    </p>
                  </button>
                );
              })}
            </div>
          </div>

          {/* Public toggle */}
          <div className="flex items-center justify-between">
            <span className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Public Room
            </span>
            <button
              type="button"
              onClick={() => setIsPublic((v) => !v)}
              className="w-11 h-6 rounded-full transition-colors relative"
              style={{ backgroundColor: isPublic ? 'var(--color-oasis-accent)' : 'var(--color-oasis-surface)' }}
            >
              <span
                className="absolute top-0.5 w-5 h-5 rounded-full bg-white transition-transform"
                style={{ transform: isPublic ? 'translateX(22px)' : 'translateX(2px)' }}
              />
            </button>
          </div>

          {/* Advanced settings (collapsed by default) */}
          <div>
            <button
              type="button"
              onClick={() => setShowAdvanced((v) => !v)}
              className="flex items-center gap-1.5 text-xs font-semibold transition-opacity hover:opacity-80"
              style={{ color: 'var(--color-oasis-text-muted)' }}
            >
              <span>{showAdvanced ? '▾' : '▸'}</span>
              <span>⚙ Advanced Options</span>
            </button>
            {showAdvanced && (
              <div className="mt-3 space-y-4">
                {/* Room theme */}
                <div>
                  <label className="block text-xs font-semibold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    🎨 Room Theme
                  </label>
                  <div className="grid grid-cols-3 gap-1.5">
                    {ROOM_THEMES.map((t) => {
                      const accent = themeAccent(t.id);
                      const selected = theme === t.id;
                      return (
                        <button
                          key={t.id}
                          type="button"
                          onClick={() => setTheme(t.id)}
                          className="px-2 py-1.5 rounded-xl text-xs font-semibold transition-all"
                          style={{
                            backgroundColor: selected ? `${accent}22` : 'var(--color-oasis-surface)',
                            color: selected ? accent : 'var(--color-oasis-text-muted)',
                            border: `1px solid ${selected ? accent : 'transparent'}`,
                          }}
                        >
                          {t.label}
                        </button>
                      );
                    })}
                  </div>
                </div>

                {/* NDS screen layout picker */}
                {isNds && (
                  <div>
                    <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
                      📱 Dual-Screen Layout
                    </label>
                    <div className="grid grid-cols-2 gap-2">
                      {NDS_LAYOUT_OPTIONS.map((opt) => (
                        <button
                          key={opt.value}
                          type="button"
                          onClick={() => setNdsLayout(opt.value)}
                          className="text-left px-3 py-2 rounded-xl text-xs transition-colors"
                          style={{
                            backgroundColor: ndsLayout === opt.value ? NDS_COLOR : 'var(--color-oasis-surface)',
                            color: ndsLayout === opt.value ? 'white' : 'var(--color-oasis-text-muted)',
                            border: ndsLayout === opt.value ? `1px solid ${NDS_COLOR}` : '1px solid transparent',
                          }}
                        >
                          <div className="font-semibold">{opt.label}</div>
                          <div className="text-[10px] opacity-75 mt-0.5">{opt.description}</div>
                        </button>
                      ))}
                    </div>
                  </div>
                )}
              </div>
            )}
          </div>

          {/* Actions */}
          <div className="flex gap-3 pt-2">
            <button
              type="button"
              onClick={onClose}
              className="flex-1 py-3 rounded-xl font-bold text-sm"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
            >
              Cancel
            </button>
            <button
              type="submit"
              className="flex-1 py-3 rounded-xl font-bold text-sm transition-transform hover:scale-[1.02] active:scale-[0.98]"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
            >
              Create Room
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
