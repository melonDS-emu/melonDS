/**
 * Community Hub — Phase 15
 *
 * Three-tab page:
 *   1. Activity Feed  — recent platform-wide events
 *   2. Game Ratings   — top-rated games + submit a review
 *   3. Rankings       — global ELO leaderboard + per-tier breakdown
 */

import { useState, useEffect } from 'react';
import {
  fetchActivity,
  fetchTopRatedGames,
  fetchGlobalRankings,
  submitReview,
  fetchGameReviews,
  TIER_COLORS,
  TIER_ICONS,
  playerTitle,
  type ActivityEvent,
  type GameRatingSummary,
  type PlayerRank,
  type GameReview,
} from '../lib/community-service';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function timeAgo(iso: string): string {
  const diff = Date.now() - new Date(iso).getTime();
  const mins = Math.floor(diff / 60000);
  if (mins < 1) return 'just now';
  if (mins < 60) return `${mins}m ago`;
  const hrs = Math.floor(mins / 60);
  if (hrs < 24) return `${hrs}h ago`;
  return `${Math.floor(hrs / 24)}d ago`;
}

const EVENT_ICONS: Record<string, string> = {
  'session-started':    '🎮',
  'session-ended':      '✅',
  'achievement-unlocked': '🏆',
  'tournament-created': '🎪',
  'tournament-won':     '👑',
  'review-submitted':   '⭐',
  'friend-added':       '🤝',
};

function StarRow({ rating, onChange }: { rating: number; onChange?: (r: number) => void }) {
  const [hover, setHover] = useState(0);
  return (
    <div className="flex gap-0.5">
      {[1, 2, 3, 4, 5].map(n => (
        <button
          key={n}
          className="text-xl leading-none transition-transform hover:scale-110"
          style={{ color: n <= (hover || rating) ? '#fbbf24' : 'var(--color-oasis-border)' }}
          onMouseEnter={() => onChange && setHover(n)}
          onMouseLeave={() => onChange && setHover(0)}
          onClick={() => onChange?.(n)}
          disabled={!onChange}
        >
          ★
        </button>
      ))}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Sub-pages
// ---------------------------------------------------------------------------

function ActivityFeedTab() {
  const [events, setEvents] = useState<ActivityEvent[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetchActivity({ limit: 60 })
      .then(d => setEvents(d.events))
      .catch(() => setEvents([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>Loading activity…</p>;
  }
  if (events.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">📡</p>
        <p className="font-bold text-lg">No community activity yet</p>
        <p className="text-sm mt-1">Play some sessions, win tournaments, or rate games to see events here!</p>
      </div>
    );
  }

  return (
    <div className="flex flex-col gap-2">
      {events.map(ev => (
        <div
          key={ev.id}
          className="flex items-start gap-3 rounded-xl px-4 py-3"
          style={{ backgroundColor: 'var(--color-oasis-surface)', border: '1px solid var(--color-oasis-border)' }}
        >
          <span className="text-xl mt-0.5 flex-shrink-0">{EVENT_ICONS[ev.type] ?? '📌'}</span>
          <div className="flex-1 min-w-0">
            <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text)' }}>
              {ev.description}
            </p>
            {ev.subject && (
              <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                {ev.subject}
              </p>
            )}
          </div>
          <span className="text-xs flex-shrink-0 mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {timeAgo(ev.createdAt)}
          </span>
        </div>
      ))}
    </div>
  );
}

// ---------------------------------------------------------------------------

function GameRatingsTab() {
  const [topGames, setTopGames] = useState<GameRatingSummary[]>([]);
  const [selectedGame, setSelectedGame] = useState<GameRatingSummary | null>(null);
  const [reviews, setReviews] = useState<GameReview[]>([]);
  const [reviewForm, setReviewForm] = useState({ playerId: '', playerName: '', rating: 0, text: '' });
  const [submitting, setSubmitting] = useState(false);
  const [submitMsg, setSubmitMsg] = useState('');
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetchTopRatedGames(20)
      .then(setTopGames)
      .catch(() => setTopGames([]))
      .finally(() => setLoading(false));
  }, []);

  function openGame(game: GameRatingSummary) {
    setSelectedGame(game);
    setSubmitMsg('');
    fetchGameReviews(game.gameId)
      .then(setReviews)
      .catch(() => setReviews([]));
  }

  async function handleSubmit() {
    if (!selectedGame) return;
    if (!reviewForm.playerName.trim()) { setSubmitMsg('Enter your display name.'); return; }
    if (reviewForm.rating === 0) { setSubmitMsg('Select a star rating.'); return; }
    setSubmitting(true);
    try {
      await submitReview(
        selectedGame.gameId,
        selectedGame.gameTitle,
        reviewForm.playerId || reviewForm.playerName,
        reviewForm.playerName,
        reviewForm.rating,
        reviewForm.text.trim() || undefined,
      );
      setSubmitMsg('Review submitted! ⭐');
      setReviewForm(f => ({ ...f, rating: 0, text: '' }));
      // Refresh
      const [updated, updatedReviews] = await Promise.all([
        fetchTopRatedGames(20),
        fetchGameReviews(selectedGame.gameId),
      ]);
      setTopGames(updated);
      setReviews(updatedReviews);
      const refreshed = updated.find(g => g.gameId === selectedGame.gameId) ?? selectedGame;
      setSelectedGame(refreshed);
    } catch {
      setSubmitMsg('Failed to submit review.');
    } finally {
      setSubmitting(false);
    }
  }

  if (loading) {
    return <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>Loading ratings…</p>;
  }

  if (selectedGame) {
    return (
      <div>
        <button
          className="flex items-center gap-2 mb-4 text-sm font-bold hover:underline"
          style={{ color: 'var(--color-oasis-accent)' }}
          onClick={() => setSelectedGame(null)}
        >
          ← Back to top-rated games
        </button>
        <div className="rounded-2xl p-5 mb-5" style={{ backgroundColor: 'var(--color-oasis-surface)', border: '1px solid var(--color-oasis-border)' }}>
          <h2 className="text-xl font-black mb-1" style={{ color: 'var(--color-oasis-text)' }}>{selectedGame.gameTitle}</h2>
          <div className="flex items-center gap-3 mb-3">
            <StarRow rating={Math.round(selectedGame.averageRating)} />
            <span className="font-bold text-sm" style={{ color: '#fbbf24' }}>{selectedGame.averageRating.toFixed(1)}</span>
            <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{selectedGame.reviewCount} {selectedGame.reviewCount === 1 ? 'review' : 'reviews'}</span>
          </div>

          {/* Submit review form */}
          <div className="mt-4 pt-4 border-t" style={{ borderColor: 'var(--color-oasis-border)' }}>
            <p className="text-sm font-bold mb-2" style={{ color: 'var(--color-oasis-text)' }}>Rate this game</p>
            <input
              className="w-full rounded-lg px-3 py-2 text-sm mb-2"
              style={{ backgroundColor: 'var(--color-oasis-bg)', border: '1px solid var(--color-oasis-border)', color: 'var(--color-oasis-text)' }}
              placeholder="Your display name"
              value={reviewForm.playerName}
              onChange={e => setReviewForm(f => ({ ...f, playerName: e.target.value, playerId: e.target.value }))}
            />
            <div className="mb-2"><StarRow rating={reviewForm.rating} onChange={r => setReviewForm(f => ({ ...f, rating: r }))} /></div>
            <textarea
              className="w-full rounded-lg px-3 py-2 text-sm mb-2 resize-none"
              style={{ backgroundColor: 'var(--color-oasis-bg)', border: '1px solid var(--color-oasis-border)', color: 'var(--color-oasis-text)' }}
              rows={2}
              placeholder="Optional review text…"
              value={reviewForm.text}
              onChange={e => setReviewForm(f => ({ ...f, text: e.target.value }))}
            />
            {submitMsg && <p className="text-xs mb-2" style={{ color: submitMsg.includes('!') ? 'var(--color-oasis-green)' : '#ef4444' }}>{submitMsg}</p>}
            <button
              className="px-4 py-2 rounded-lg text-sm font-bold"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }}
              onClick={handleSubmit}
              disabled={submitting}
            >
              {submitting ? 'Submitting…' : 'Submit Review'}
            </button>
          </div>
        </div>

        {/* Reviews list */}
        <div className="flex flex-col gap-3">
          {reviews.length === 0 && <p className="text-sm text-center py-4" style={{ color: 'var(--color-oasis-text-muted)' }}>No reviews yet — be the first!</p>}
          {reviews.map(r => (
            <div key={r.id} className="rounded-xl px-4 py-3" style={{ backgroundColor: 'var(--color-oasis-surface)', border: '1px solid var(--color-oasis-border)' }}>
              <div className="flex items-center justify-between mb-1">
                <span className="font-bold text-sm" style={{ color: 'var(--color-oasis-text)' }}>{r.playerName}</span>
                <div className="flex items-center gap-2">
                  <StarRow rating={r.rating} />
                  <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{timeAgo(r.createdAt)}</span>
                </div>
              </div>
              {r.text && <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>{r.text}</p>}
            </div>
          ))}
        </div>
      </div>
    );
  }

  return (
    <div>
      {topGames.length === 0 && (
        <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <p className="text-4xl mb-3">⭐</p>
          <p className="font-bold text-lg">No ratings yet</p>
          <p className="text-sm mt-1">Be the first to rate a game! Games appear here after receiving at least one review.</p>
        </div>
      )}
      <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
        {topGames.map((g, i) => (
          <button
            key={g.gameId}
            className="rounded-2xl p-4 text-left hover:opacity-90 transition-opacity"
            style={{ backgroundColor: 'var(--color-oasis-surface)', border: '1px solid var(--color-oasis-border)' }}
            onClick={() => openGame(g)}
          >
            <div className="flex items-start justify-between gap-2">
              <div>
                <p className="font-black text-sm" style={{ color: 'var(--color-oasis-text)' }}>
                  <span className="mr-2" style={{ color: 'var(--color-oasis-text-muted)' }}>#{i + 1}</span>
                  {g.gameTitle}
                </p>
                <div className="flex items-center gap-2 mt-1">
                  <StarRow rating={Math.round(g.averageRating)} />
                  <span className="font-bold text-xs" style={{ color: '#fbbf24' }}>{g.averageRating.toFixed(1)}</span>
                </div>
              </div>
              <span className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
                {g.reviewCount} {g.reviewCount === 1 ? 'review' : 'reviews'}
              </span>
            </div>
          </button>
        ))}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------

function RankingsTab() {
  const [rankings, setRankings] = useState<PlayerRank[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetchGlobalRankings(50)
      .then(setRankings)
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>Loading rankings…</p>;
  }
  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏅</p>
        <p className="font-bold text-lg">No ranked players yet</p>
        <p className="text-sm mt-1">Host a ranked room (toggle Ranked mode in the host dialog) and play matches to appear on the leaderboard.</p>
      </div>
    );
  }

  return (
    <div className="flex flex-col gap-2">
      {/* Header */}
      <div className="grid grid-cols-[2rem_1fr_auto_auto_auto] gap-2 px-4 py-2 text-xs font-bold"
        style={{ color: 'var(--color-oasis-text-muted)' }}>
        <span>#</span><span>Player</span><span>ELO</span><span>W/L</span><span>Tier</span>
      </div>
      {rankings.map((r, i) => {
        const tierColor = TIER_COLORS[r.tier];
        const tierIcon = TIER_ICONS[r.tier];
        const title = playerTitle(r.tier, r.gamesPlayed);
        return (
          <div
            key={r.playerId}
            className="grid grid-cols-[2rem_1fr_auto_auto_auto] gap-2 items-center rounded-xl px-4 py-3"
            style={{ backgroundColor: 'var(--color-oasis-surface)', border: `1px solid ${tierColor}33` }}
          >
            <span className="text-sm font-black" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {i === 0 ? '👑' : i === 1 ? '🥈' : i === 2 ? '🥉' : `${i + 1}`}
            </span>
            <div>
              <p className="font-bold text-sm" style={{ color: 'var(--color-oasis-text)' }}>{r.playerName}</p>
              <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{title}</p>
            </div>
            <span className="font-black text-sm" style={{ color: tierColor }}>{r.elo}</span>
            <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{r.wins}W/{r.losses}L</span>
            <span
              className="text-xs font-black px-2 py-0.5 rounded-full"
              style={{ color: tierColor, backgroundColor: `${tierColor}18` }}
            >
              {tierIcon} {r.tier}
            </span>
          </div>
        );
      })}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Main page
// ---------------------------------------------------------------------------

type Tab = 'activity' | 'ratings' | 'rankings';

const TABS: { id: Tab; label: string; icon: string }[] = [
  { id: 'activity',  label: 'Activity Feed', icon: '📡' },
  { id: 'ratings',   label: 'Game Ratings',  icon: '⭐' },
  { id: 'rankings',  label: 'Rankings',      icon: '🏅' },
];

export function CommunityPage() {
  const [tab, setTab] = useState<Tab>('activity');

  return (
    <div className="p-6 max-w-4xl mx-auto">
      {/* Page header */}
      <div className="mb-6">
        <h1 className="text-3xl font-black mb-1" style={{ color: 'var(--color-oasis-text)' }}>
          🌐 Community Hub
        </h1>
        <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          See what's happening across RetroOasis — live activity, game ratings, and ranked leaderboards.
        </p>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 mb-6">
        {TABS.map(t => (
          <button
            key={t.id}
            className="flex items-center gap-1.5 px-4 py-2 rounded-xl text-sm font-bold transition-colors"
            style={{
              backgroundColor: tab === t.id ? 'var(--color-oasis-accent)' : 'var(--color-oasis-surface)',
              color: tab === t.id ? '#fff' : 'var(--color-oasis-text-muted)',
              border: '1px solid var(--color-oasis-border)',
            }}
            onClick={() => setTab(t.id)}
          >
            <span>{t.icon}</span> {t.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {tab === 'activity'  && <ActivityFeedTab />}
      {tab === 'ratings'   && <GameRatingsTab />}
      {tab === 'rankings'  && <RankingsTab />}
    </div>
  );
}
