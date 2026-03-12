import { useState, useEffect, useRef, useCallback } from 'react';
import {
  listClipMeta,
  loadClip,
  deleteClip,
  exportClip,
  copyClipLink,
  updateClipTag,
  generateClipThumbnail,
  formatClipDuration,
  type ClipMeta,
  type Clip,
} from '../lib/clip-service';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function groupByGame(metas: ClipMeta[]): Map<string, ClipMeta[]> {
  const map = new Map<string, ClipMeta[]>();
  for (const m of metas) {
    const key = m.gameId;
    const list = map.get(key) ?? [];
    list.push(m);
    map.set(key, list);
  }
  return map;
}

function relativeDate(iso: string): string {
  const diff = Date.now() - new Date(iso).getTime();
  const mins = Math.floor(diff / 60_000);
  if (mins < 1) return 'just now';
  if (mins < 60) return `${mins}m ago`;
  const hours = Math.floor(mins / 60);
  if (hours < 24) return `${hours}h ago`;
  const days = Math.floor(hours / 24);
  return `${days}d ago`;
}

// ---------------------------------------------------------------------------
// Thumbnail component — generates on first mount
// ---------------------------------------------------------------------------

function ClipThumbnail({ meta }: { meta: ClipMeta }) {
  const [thumb, setThumb] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    generateClipThumbnail(meta).then((url) => {
      if (!cancelled) setThumb(url);
    });
    return () => { cancelled = true; };
  }, [meta.id]); // eslint-disable-line react-hooks/exhaustive-deps

  if (!thumb) {
    return (
      <div
        className="w-full aspect-video flex items-center justify-center rounded-t-xl text-2xl"
        style={{ backgroundColor: 'var(--color-oasis-surface)' }}
      >
        🎬
      </div>
    );
  }
  return (
    <img
      src={thumb}
      alt="clip thumbnail"
      className="w-full aspect-video object-cover rounded-t-xl"
    />
  );
}

// ---------------------------------------------------------------------------
// In-page video player modal
// ---------------------------------------------------------------------------

function ClipPlayerModal({ clip, onClose }: { clip: Clip; onClose: () => void }) {
  const videoRef = useRef<HTMLVideoElement>(null);

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
      if (e.key === 'f' || e.key === 'F') {
        videoRef.current?.requestFullscreen().catch(() => undefined);
      }
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [onClose]);

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center"
      style={{ backgroundColor: 'rgba(0,0,0,0.85)' }}
      onClick={onClose}
    >
      <div
        className="relative rounded-2xl overflow-hidden shadow-2xl max-w-3xl w-full mx-4"
        style={{ backgroundColor: 'var(--color-oasis-card)' }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-center justify-between px-4 py-3">
          <div>
            <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
              {clip.gameTitle}
            </p>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {formatClipDuration(clip.durationSecs)} · {relativeDate(clip.capturedAt)}
              {clip.tag && <> · <span style={{ color: 'var(--color-oasis-accent-light)' }}>#{clip.tag}</span></>}
            </p>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={() => videoRef.current?.requestFullscreen().catch(() => undefined)}
              className="text-xs px-2 py-1 rounded"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
              title="Fullscreen (F)"
            >
              ⛶ Fullscreen
            </button>
            <button
              onClick={onClose}
              className="text-lg font-black w-7 h-7 flex items-center justify-center rounded"
              style={{ color: 'var(--color-oasis-text-muted)' }}
            >
              ✕
            </button>
          </div>
        </div>

        {/* Video */}
        <video
          ref={videoRef}
          src={clip.url}
          controls
          autoPlay
          className="w-full"
          style={{ maxHeight: '60vh', backgroundColor: '#000' }}
        />
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Clip card
// ---------------------------------------------------------------------------

interface ClipCardProps {
  meta: ClipMeta;
  onPlay: (meta: ClipMeta) => void;
  onDelete: (id: string) => void;
  onTagChange: (id: string, tag: string) => void;
}

function ClipCard({ meta, onPlay, onDelete, onTagChange }: ClipCardProps) {
  const [menuOpen, setMenuOpen] = useState(false);
  const [editingTag, setEditingTag] = useState(false);
  const [tagInput, setTagInput] = useState(meta.tag ?? '');
  const [copyMsg, setCopyMsg] = useState('');

  async function handleExport() {
    setMenuOpen(false);
    await exportClip(meta);
  }

  async function handleCopyLink() {
    setMenuOpen(false);
    try {
      await copyClipLink(meta);
      setCopyMsg('Copied!');
      setTimeout(() => setCopyMsg(''), 2000);
    } catch {
      setCopyMsg('Failed');
      setTimeout(() => setCopyMsg(''), 2000);
    }
  }

  function handleTagSubmit() {
    onTagChange(meta.id, tagInput);
    setEditingTag(false);
  }

  return (
    <div
      className="rounded-xl overflow-hidden flex flex-col"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      {/* Thumbnail — click to play */}
      <button
        className="relative group focus:outline-none"
        onClick={() => onPlay(meta)}
        aria-label={`Play clip: ${meta.gameTitle}`}
      >
        <ClipThumbnail meta={meta} />
        {/* Play overlay */}
        <div
          className="absolute inset-0 flex items-center justify-center opacity-0 group-hover:opacity-100 transition-opacity rounded-t-xl"
          style={{ backgroundColor: 'rgba(0,0,0,0.45)' }}
        >
          <span className="text-4xl">▶</span>
        </div>
        {/* Duration badge */}
        <span
          className="absolute bottom-2 right-2 text-[10px] font-bold px-1.5 py-0.5 rounded"
          style={{ backgroundColor: 'rgba(0,0,0,0.72)', color: '#fff' }}
        >
          {formatClipDuration(meta.durationSecs)}
        </span>
      </button>

      {/* Info row */}
      <div className="px-3 py-2 flex-1 flex flex-col gap-1">
        <p className="text-xs font-bold truncate" style={{ color: 'var(--color-oasis-text)' }}>
          {meta.gameTitle}
        </p>
        <p className="text-[10px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {relativeDate(meta.capturedAt)}
        </p>

        {/* Tag */}
        {editingTag ? (
          <div className="flex gap-1 mt-1">
            <input
              autoFocus
              value={tagInput}
              onChange={(e) => setTagInput(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') handleTagSubmit();
                if (e.key === 'Escape') setEditingTag(false);
              }}
              placeholder="label or match ID…"
              className="flex-1 text-[10px] px-2 py-1 rounded border-0 focus:outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
              }}
            />
            <button
              onClick={handleTagSubmit}
              className="text-[10px] font-bold px-2 rounded"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
            >
              ✓
            </button>
            <button
              onClick={() => setEditingTag(false)}
              className="text-[10px] px-2 rounded"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
            >
              ✕
            </button>
          </div>
        ) : (
          <button
            onClick={() => { setTagInput(meta.tag ?? ''); setEditingTag(true); }}
            className="text-[10px] text-left truncate"
            style={{ color: meta.tag ? 'var(--color-oasis-accent-light)' : 'var(--color-oasis-text-muted)' }}
          >
            {meta.tag ? `#${meta.tag}` : '+ Add tag'}
          </button>
        )}
      </div>

      {/* Action bar */}
      <div
        className="flex items-center gap-1 px-3 pb-3"
        style={{ borderTop: '1px solid rgba(255,255,255,0.05)', paddingTop: 6 }}
      >
        {copyMsg ? (
          <span className="text-[10px] flex-1" style={{ color: 'var(--color-oasis-accent-light)' }}>
            {copyMsg}
          </span>
        ) : (
          <>
            <button
              onClick={() => onPlay(meta)}
              className="flex-1 text-[10px] font-bold py-1 rounded"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
            >
              ▶ Play
            </button>

            {/* Overflow menu */}
            <div className="relative">
              <button
                onClick={() => setMenuOpen((v) => !v)}
                className="text-[10px] px-2 py-1 rounded"
                style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
              >
                ⋯
              </button>
              {menuOpen && (
                <div
                  className="absolute bottom-full right-0 mb-1 rounded-xl overflow-hidden shadow-xl z-10 min-w-[120px]"
                  style={{ backgroundColor: 'var(--color-oasis-surface)' }}
                >
                  {[
                    { label: '⬇ Export .webm', action: handleExport },
                    { label: '🔗 Copy link', action: handleCopyLink },
                    { label: '✏️ Tag', action: () => { setMenuOpen(false); setTagInput(meta.tag ?? ''); setEditingTag(true); } },
                    { label: '🗑 Delete', action: () => { setMenuOpen(false); onDelete(meta.id); }, danger: true },
                  ].map((item) => (
                    <button
                      key={item.label}
                      onClick={item.action}
                      className="w-full text-left text-xs px-3 py-2 transition-colors"
                      style={{
                        color: item.danger ? '#f87171' : 'var(--color-oasis-text)',
                        borderBottom: '1px solid rgba(255,255,255,0.05)',
                      }}
                    >
                      {item.label}
                    </button>
                  ))}
                </div>
              )}
            </div>
          </>
        )}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------

export function ClipsPage() {
  const [allMeta, setAllMeta] = useState<ClipMeta[]>([]);
  const [activeClip, setActiveClip] = useState<Clip | null>(null);
  const [loadingId, setLoadingId] = useState<string | null>(null);
  const [search, setSearch] = useState('');

  const refresh = useCallback(() => setAllMeta(listClipMeta()), []);

  useEffect(() => {
    refresh();
  }, [refresh]);

  const filtered = search.trim()
    ? allMeta.filter(
        (m) =>
          m.gameTitle.toLowerCase().includes(search.toLowerCase()) ||
          (m.tag ?? '').toLowerCase().includes(search.toLowerCase())
      )
    : allMeta;

  const grouped = groupByGame(filtered);

  async function handlePlay(meta: ClipMeta) {
    setLoadingId(meta.id);
    try {
      const clip = await loadClip(meta);
      if (clip) setActiveClip(clip);
    } finally {
      setLoadingId(null);
    }
  }

  function handleClosePlayer() {
    if (activeClip) URL.revokeObjectURL(activeClip.url);
    setActiveClip(null);
  }

  async function handleDelete(id: string) {
    await deleteClip(id);
    refresh();
  }

  function handleTagChange(id: string, tag: string) {
    updateClipTag(id, tag);
    refresh();
  }

  return (
    <div className="space-y-8 max-w-5xl">
      {/* Header */}
      <div className="flex items-center justify-between gap-4 flex-wrap">
        <div>
          <h1 className="text-2xl font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
            🎬 Clip Library
          </h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {allMeta.length} clip{allMeta.length !== 1 ? 's' : ''} recorded across{' '}
            {new Set(allMeta.map((m) => m.gameId)).size} game
            {new Set(allMeta.map((m) => m.gameId)).size !== 1 ? 's' : ''}
          </p>
        </div>
        <input
          value={search}
          onChange={(e) => setSearch(e.target.value)}
          placeholder="Search by game or tag…"
          className="rounded-xl px-3 py-2 text-sm border-0 focus:outline-none w-56"
          style={{
            backgroundColor: 'var(--color-oasis-card)',
            color: 'var(--color-oasis-text)',
          }}
        />
      </div>

      {allMeta.length === 0 ? (
        <div
          className="rounded-2xl p-10 text-center"
          style={{ backgroundColor: 'var(--color-oasis-card)' }}
        >
          <p className="text-3xl mb-3">🎬</p>
          <p className="text-base font-bold" style={{ color: 'var(--color-oasis-text)' }}>
            No clips yet
          </p>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Start a recording from the Lobby page while you play.
          </p>
        </div>
      ) : filtered.length === 0 ? (
        <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          No clips match &quot;{search}&quot;.
        </p>
      ) : (
        Array.from(grouped.entries()).map(([gameId, clips]) => (
          <section key={gameId}>
            <h2
              className="text-base font-bold mb-3 flex items-center gap-2"
              style={{ color: 'var(--color-oasis-accent-light)' }}
            >
              🕹️ {clips[0].gameTitle}
              <span
                className="text-[10px] font-bold px-2 py-0.5 rounded-full"
                style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)' }}
              >
                {clips.length}
              </span>
            </h2>
            <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4">
              {clips.map((meta) => (
                <div key={meta.id} style={{ opacity: loadingId === meta.id ? 0.6 : 1 }}>
                  <ClipCard
                    meta={meta}
                    onPlay={handlePlay}
                    onDelete={handleDelete}
                    onTagChange={handleTagChange}
                  />
                </div>
              ))}
            </div>
          </section>
        ))
      )}

      {/* Player modal */}
      {activeClip && (
        <ClipPlayerModal clip={activeClip} onClose={handleClosePlayer} />
      )}
    </div>
  );
}
