/**
 * Phase 30 — QoL & "Wow" Layer
 *
 * PartyCollectionsPage — browse curated party-mode game collections.
 * Each collection groups games that work especially well in social/party
 * settings.  Clicking a collection card expands it to show all games
 * with links to their detail pages.
 */

import { useState, useEffect } from 'react';
import { Link } from 'react-router-dom';

// ---------------------------------------------------------------------------
// Types (mirrors party-collection-store.ts)
// ---------------------------------------------------------------------------

interface PartyCollection {
  id: string;
  name: string;
  description: string;
  emoji: string;
  curatorNote: string;
  gameIds: string[];
  tags: string[];
  idealPlayers: string;
}

// ---------------------------------------------------------------------------
// Data fetching
// ---------------------------------------------------------------------------

const API_BASE = 'http://localhost:8080';

async function fetchCollections(tag?: string): Promise<PartyCollection[]> {
  const url = tag
    ? `${API_BASE}/api/party-collections?tag=${encodeURIComponent(tag)}`
    : `${API_BASE}/api/party-collections`;
  const res = await fetch(url);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  const data = await res.json() as { collections: PartyCollection[] };
  return data.collections;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const TAG_COLORS: Record<string, string> = {
  racing:       '#e8a000',
  fighting:     '#b22222',
  party:        '#7c3aed',
  co-op:        '#16a34a',
  competitive:  '#dc2626',
  casual:       '#0891b2',
  rpg:          '#9333ea',
  action:       '#ea580c',
  puzzle:       '#0284c7',
  online:       '#2563eb',
  wfc:          '#2563eb',
  speedrun:     '#d97706',
  'all-ages':   '#059669',
  adventure:    '#0369a1',
  platform:     '#d97706',
  grinding:     '#6b7280',
  trading:      '#be185d',
  '1v1':        '#dc2626',
  solo:         '#4b5563',
  arcade:       '#ea580c',
  strategy:     '#0f766e',
  recommended:  '#16a34a',
};

function tagColor(tag: string): string {
  return TAG_COLORS[tag] ?? 'var(--color-oasis-accent)';
}

function gameIdToTitle(id: string): string {
  return id
    .replace(/^(nes|snes|n64|gba|gb|gbc|nds|wii|gc|wiiu|genesis|dc|psx|ps2|psp)-/, '')
    .replace(/-/g, ' ')
    .replace(/\b\w/g, (c) => c.toUpperCase());
}

// ---------------------------------------------------------------------------
// CollectionCard
// ---------------------------------------------------------------------------

function CollectionCard({ collection }: { collection: PartyCollection }) {
  const [expanded, setExpanded] = useState(false);

  return (
    <div
      className="rounded-2xl overflow-hidden"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      {/* Header row */}
      <button
        type="button"
        className="w-full text-left px-5 py-4 flex items-start gap-4 hover:opacity-90 transition-opacity"
        onClick={() => setExpanded((v) => !v)}
      >
        <span className="text-4xl flex-shrink-0 mt-0.5">{collection.emoji}</span>
        <div className="flex-1 min-w-0">
          <div className="flex items-center gap-2 flex-wrap">
            <h3 className="font-black text-base">{collection.name}</h3>
            <span
              className="text-[10px] font-bold px-2 py-0.5 rounded-full"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
            >
              👥 {collection.idealPlayers}
            </span>
          </div>
          <p className="text-sm mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {collection.description}
          </p>
          {/* Tags */}
          <div className="flex flex-wrap gap-1.5 mt-2">
            {collection.tags.map((tag) => (
              <span
                key={tag}
                className="text-[10px] font-bold px-2 py-0.5 rounded-full"
                style={{ backgroundColor: `${tagColor(tag)}22`, color: tagColor(tag) }}
              >
                {tag}
              </span>
            ))}
          </div>
        </div>
        <span
          className="text-xs font-semibold flex-shrink-0 mt-1"
          style={{ color: 'var(--color-oasis-text-muted)' }}
        >
          {collection.gameIds.length} games {expanded ? '▴' : '▾'}
        </span>
      </button>

      {/* Expanded body */}
      {expanded && (
        <div
          className="border-t px-5 py-4"
          style={{ borderColor: 'var(--color-oasis-border)' }}
        >
          {/* Curator note */}
          <p
            className="text-xs italic mb-4"
            style={{ color: 'var(--color-oasis-text-muted)' }}
          >
            💬 {collection.curatorNote}
          </p>

          {/* Game list */}
          <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
            {collection.gameIds.map((id) => (
              <Link
                key={id}
                to={`/game/${id}`}
                className="flex items-center gap-3 px-3 py-2 rounded-xl transition-opacity hover:opacity-80"
                style={{ backgroundColor: 'var(--color-oasis-surface)' }}
              >
                <span className="text-lg flex-shrink-0">🎮</span>
                <span className="text-sm font-semibold truncate">{gameIdToTitle(id)}</span>
                <span
                  className="ml-auto text-[10px] font-mono flex-shrink-0"
                  style={{ color: 'var(--color-oasis-text-muted)' }}
                >
                  {id.split('-')[0].toUpperCase()}
                </span>
              </Link>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// All available tags for filter pills
// ---------------------------------------------------------------------------

const ALL_TAGS = [
  'racing', 'fighting', 'party', 'co-op', 'competitive', 'casual',
  'rpg', 'action', 'online', 'wfc', 'speedrun', 'all-ages', 'adventure',
];

// ---------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------

export function PartyCollectionsPage() {
  const [collections, setCollections] = useState<PartyCollection[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [activeTag, setActiveTag] = useState<string | null>(null);

  useEffect(() => {
    setLoading(true);
    setError(null);
    fetchCollections(activeTag ?? undefined)
      .then(setCollections)
      .catch((err: unknown) => setError(String(err)))
      .finally(() => setLoading(false));
  }, [activeTag]);

  return (
    <div className="max-w-3xl">
      {/* Header */}
      <div className="mb-6">
        <h1 className="text-2xl font-black mb-1">🎉 Party Collections</h1>
        <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Curated groups of games that shine in social and party settings.
          Browse by category or expand any collection to see the full game list.
        </p>
      </div>

      {/* Tag filter pills */}
      <div className="flex flex-wrap gap-2 mb-6">
        <button
          type="button"
          onClick={() => setActiveTag(null)}
          className="px-3 py-1 rounded-full text-xs font-bold transition-opacity hover:opacity-80"
          style={{
            backgroundColor: activeTag === null ? 'var(--color-oasis-accent)' : 'var(--color-oasis-surface)',
            color: activeTag === null ? 'white' : 'var(--color-oasis-text-muted)',
          }}
        >
          All
        </button>
        {ALL_TAGS.map((tag) => (
          <button
            key={tag}
            type="button"
            onClick={() => setActiveTag(activeTag === tag ? null : tag)}
            className="px-3 py-1 rounded-full text-xs font-bold transition-opacity hover:opacity-80"
            style={{
              backgroundColor: activeTag === tag ? tagColor(tag) : 'var(--color-oasis-surface)',
              color: activeTag === tag ? 'white' : tagColor(tag),
            }}
          >
            {tag}
          </button>
        ))}
      </div>

      {/* Loading */}
      {loading && (
        <div className="text-center py-12">
          <p style={{ color: 'var(--color-oasis-text-muted)' }}>Loading collections…</p>
        </div>
      )}

      {/* Error */}
      {!loading && error && (
        <div
          className="px-4 py-3 rounded-xl text-sm"
          style={{ backgroundColor: 'rgba(230,0,18,0.1)', color: '#f87171' }}
        >
          ⚠️ Could not load collections: {error}
        </div>
      )}

      {/* Collections grid */}
      {!loading && !error && collections.length === 0 && (
        <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
          No collections found for this filter.
        </p>
      )}

      {!loading && !error && collections.length > 0 && (
        <div className="flex flex-col gap-4">
          {collections.map((c) => (
            <CollectionCard key={c.id} collection={c} />
          ))}
        </div>
      )}
    </div>
  );
}
