import { useState, useEffect } from 'react';
import { Link } from 'react-router-dom';
import {
  fetchAllEvents,
  fetchFeaturedGames,
  themeGradient,
  themeAccent,
  type SeasonalEvent,
  type FeaturedGame,
} from '../lib/events-service';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function formatDateRange(startDate: string, endDate: string): string {
  // startDate and endDate are expected in 'YYYY-MM-DD' format (ISO 8601 date)
  const start = new Date(startDate + 'T00:00:00Z');
  const end   = new Date(endDate   + 'T00:00:00Z');
  return `${start.toLocaleDateString(undefined, { month: 'short', day: 'numeric' })} – ${end.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' })}`;
}

function eventStatus(event: SeasonalEvent, today: string): 'active' | 'upcoming' | 'past' {
  if (today < event.startDate) return 'upcoming';
  if (today > event.endDate)   return 'past';
  return 'active';
}

// ---------------------------------------------------------------------------
// Sub-components
// ---------------------------------------------------------------------------

function StatusBadge({ status }: { status: 'active' | 'upcoming' | 'past' }) {
  const map = {
    active:   { label: '● Live',     color: 'var(--color-oasis-green)' },
    upcoming: { label: '◆ Upcoming', color: 'var(--color-oasis-yellow)' },
    past:     { label: '✓ Ended',    color: 'var(--color-oasis-text-muted)' },
  };
  const { label, color } = map[status];
  return (
    <span className="text-xs font-black px-2 py-0.5 rounded-full" style={{ color, backgroundColor: `${color}18` }}>
      {label}
    </span>
  );
}

function EventCard({ event, today }: { event: SeasonalEvent; today: string }) {
  const status  = eventStatus(event, today);
  const accent  = themeAccent(event.theme);
  const bgStyle = themeGradient(event.theme);
  return (
    <div
      className="rounded-2xl p-5 flex flex-col gap-3"
      style={{ background: bgStyle, border: `1px solid ${accent}33` }}
    >
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-2">
          <span className="text-2xl">{event.emoji}</span>
          <div>
            <h3 className="font-black text-base leading-tight" style={{ color: '#fff' }}>
              {event.name}
            </h3>
            <p className="text-xs mt-0.5 font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {formatDateRange(event.startDate, event.endDate)}
            </p>
          </div>
        </div>
        <StatusBadge status={status} />
      </div>

      <p className="text-sm leading-relaxed" style={{ color: 'var(--color-oasis-text-muted)' }}>
        {event.description}
      </p>

      {event.xpMultiplier && event.xpMultiplier > 1 && (
        <div
          className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-xl text-xs font-black w-fit"
          style={{ backgroundColor: `${accent}22`, color: accent }}
        >
          ⚡ {event.xpMultiplier}× Stat Multiplier
        </div>
      )}

      {event.featuredGameIds.length > 0 && (
        <div>
          <p className="text-xs font-black mb-1.5" style={{ color: accent }}>
            Featured Games
          </p>
          <div className="flex flex-wrap gap-1.5">
            {event.featuredGameIds.map((gid) => (
              <Link
                key={gid}
                to={`/game/${gid}`}
                className="text-xs px-2 py-0.5 rounded-full font-semibold hover:brightness-110 transition-all"
                style={{
                  backgroundColor: 'rgba(255,255,255,0.06)',
                  color: 'var(--color-oasis-text)',
                  border: '1px solid var(--n-border)',
                }}
              >
                {gid}
              </Link>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}

function FeaturedGameCard({ game }: { game: FeaturedGame }) {
  return (
    <Link
      to={`/game/${game.gameId}`}
      className="rounded-2xl p-4 flex items-center gap-3 hover:brightness-110 transition-all n-card"
      style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
    >
      <span className="text-2xl w-9 text-center flex-shrink-0">{game.emoji}</span>
      <div className="flex-1 min-w-0">
        <p className="text-sm font-black truncate" style={{ color: 'var(--color-oasis-text)' }}>
          {game.gameTitle}
        </p>
        <p className="text-xs mt-0.5 font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {game.system.toUpperCase()} · {game.reason}
        </p>
      </div>
      <span className="text-xs font-bold" style={{ color: 'var(--color-oasis-text-muted)' }}>→</span>
    </Link>
  );
}

// ---------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------

export function EventsPage() {
  const [events, setEvents]         = useState<SeasonalEvent[]>([]);
  const [featured, setFeatured]     = useState<FeaturedGame[]>([]);
  const [loading, setLoading]       = useState(true);
  const [error, setError]           = useState<string | null>(null);
  const [filter, setFilter]         = useState<'all' | 'active' | 'upcoming' | 'past'>('all');

  const today = new Date().toISOString().slice(0, 10);

  useEffect(() => {
    Promise.all([fetchAllEvents(), fetchFeaturedGames()])
      .then(([eventsData, featuredData]) => {
        setEvents(eventsData.events);
        setFeatured(featuredData);
      })
      .catch(() => setError('Could not load events — is the lobby server running?'))
      .finally(() => setLoading(false));
  }, []);

  const filteredEvents = events.filter((e) => {
    if (filter === 'all') return true;
    return eventStatus(e, today) === filter;
  });

  const activeCount   = events.filter((e) => eventStatus(e, today) === 'active').length;
  const upcomingCount = events.filter((e) => eventStatus(e, today) === 'upcoming').length;

  return (
    <div className="space-y-10 max-w-5xl">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-black" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🎪 Events & Featured Games
        </h1>
        <p className="text-sm mt-1 font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Seasonal events, weekly featured games, and limited-time challenges
        </p>
      </div>

      {/* Featured Games */}
      <section>
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-lg font-black" style={{ color: 'var(--color-oasis-accent-light)' }}>
            ⭐ Featured This Week
          </h2>
          <Link to="/library" className="text-xs font-bold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Browse All →
          </Link>
        </div>
        {loading ? (
          <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>Loading…</p>
        ) : featured.length > 0 ? (
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3">
            {featured.map((g) => <FeaturedGameCard key={g.gameId} game={g} />)}
          </div>
        ) : (
          <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No featured games available.
          </p>
        )}
      </section>

      {/* Seasonal Events */}
      <section>
        <div className="flex items-start justify-between mb-4">
          <div>
            <h2 className="text-lg font-black" style={{ color: 'var(--color-oasis-accent-light)' }}>
              🗓️ Seasonal Events
            </h2>
            {activeCount > 0 && (
              <p className="text-xs mt-0.5 font-semibold" style={{ color: 'var(--color-oasis-green)' }}>
                {activeCount} event{activeCount !== 1 ? 's' : ''} active right now
              </p>
            )}
          </div>
          {/* Filter tabs */}
          <div className="flex gap-1.5">
            {(['all', 'active', 'upcoming', 'past'] as const).map((f) => (
              <button
                key={f}
                onClick={() => setFilter(f)}
                className="text-xs font-black px-3 py-1.5 rounded-full capitalize transition-all"
                style={
                  filter === f
                    ? { backgroundColor: 'var(--color-oasis-accent)', color: '#fff' }
                    : { backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)', border: '1px solid var(--n-border)' }
                }
              >
                {f}{f === 'upcoming' && upcomingCount > 0 ? ` (${upcomingCount})` : ''}
              </button>
            ))}
          </div>
        </div>

        {error && (
          <div
            className="rounded-2xl px-4 py-3 text-sm font-semibold"
            style={{ backgroundColor: 'rgba(230,0,18,0.1)', color: 'var(--color-oasis-accent)', border: '1px solid rgba(230,0,18,0.2)' }}
          >
            ⚠️ {error}
          </div>
        )}

        {!loading && !error && filteredEvents.length === 0 && (
          <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
        No {filter !== 'all' ? `${filter} ` : ''}events found.
          </p>
        )}

        {!loading && !error && filteredEvents.length > 0 && (
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {filteredEvents.map((e) => (
              <EventCard key={e.id} event={e} today={today} />
            ))}
          </div>
        )}
      </section>

      {/* Room Themes hint */}
      <section>
        <div
          className="rounded-2xl p-5 flex items-start gap-4"
          style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
        >
          <span className="text-2xl flex-shrink-0">🎨</span>
          <div>
            <h3 className="font-black text-sm mb-1" style={{ color: 'var(--color-oasis-text)' }}>
              Custom Room Themes
            </h3>
            <p className="text-sm leading-relaxed" style={{ color: 'var(--color-oasis-text-muted)' }}>
              When hosting a room you can now pick a seasonal or custom theme —{' '}
              <span style={{ color: 'var(--color-oasis-text)' }}>Spring</span>,{' '}
              <span style={{ color: 'var(--color-oasis-text)' }}>Summer</span>,{' '}
              <span style={{ color: 'var(--color-oasis-text)' }}>Fall</span>,{' '}
              <span style={{ color: 'var(--color-oasis-text)' }}>Winter</span>, or{' '}
              <span style={{ color: 'var(--color-oasis-text)' }}>Neon</span>.{' '}
              The theme colours the lobby card and room page so your friends can find your vibe instantly.
            </p>
            <Link
              to="/"
              className="mt-2 inline-block text-xs font-black"
              style={{ color: 'var(--color-oasis-accent)' }}
            >
              Host a themed room →
            </Link>
          </div>
        </div>
      </section>
    </div>
  );
}
