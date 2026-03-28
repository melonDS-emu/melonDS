/**
 * Seasonal Events — Phase 13
 *
 * Pre-defined seasonal events that tie together featured games, achievement
 * bonuses, and themed room decorations. Events are date-range bounded; the
 * `getActiveEvents()` helper returns only events whose window includes today.
 */

export interface SeasonalEvent {
  id: string;
  name: string;
  description: string;
  emoji: string;
  theme: string; // CSS gradient hint / room-theme id
  startDate: string; // ISO 8601 date (inclusive)
  endDate: string;   // ISO 8601 date (inclusive)
  featuredGameIds: string[];
  /** Optional stat-multiplier shown in the UI (cosmetic only). */
  xpMultiplier?: number;
}

/** Master list of all seasonal events — kept in ascending startDate order so
 *  `getNextEvent()` (which uses `.find()`) returns the correct upcoming event. */
export const SEASONAL_EVENTS: SeasonalEvent[] = [
  // ── Spring ────────────────────────────────────────────────────────────────
  {
    id: 'spring-showdown-2026',
    name: 'Spring Showdown 2026',
    description:
      'Celebrate spring with fresh competition! Play together during the Spring Showdown and earn bonus stats on every session.',
    emoji: '🌸',
    theme: 'spring',
    startDate: '2026-03-01',
    endDate: '2026-05-31',
    featuredGameIds: [
      'nds-mario-kart-ds',
      'n64-mario-kart-64',
      'gba-mario-kart-super-circuit',
    ],
    xpMultiplier: 2,
  },
  // ── Summer ────────────────────────────────────────────────────────────────
  {
    id: 'summer-retro-slam-2026',
    name: 'Summer Retro Slam',
    description:
      'The hottest retro gaming event of the year — host public rooms, climb the leaderboard, and claim your summer title.',
    emoji: '☀️',
    theme: 'summer',
    startDate: '2026-06-01',
    endDate: '2026-08-31',
    featuredGameIds: [
      'n64-super-smash-bros',
      'gba-advance-wars',
      'nds-tetris-ds',
    ],
    xpMultiplier: 2,
  },
  // ── Fall ──────────────────────────────────────────────────────────────────
  {
    id: 'fall-tournament-classic-2026',
    name: 'Fall Tournament Classic',
    description:
      'Bracket season is here! Create tournaments, challenge rivals, and prove who reigns supreme this autumn.',
    emoji: '🍂',
    theme: 'fall',
    startDate: '2026-09-01',
    endDate: '2026-11-30',
    featuredGameIds: [
      'n64-super-smash-bros',
      'n64-mario-party-3',
      'nds-pokemon-diamond',
    ],
    xpMultiplier: 1,
  },
  // ── Winter ────────────────────────────────────────────────────────────────
  {
    id: 'winter-retro-fest-2026',
    name: 'Winter Retro Fest 2026',
    description:
      'Gather your crew for cozy co-op during the holidays. Share clips, earn achievements, and spread the retro spirit.',
    emoji: '❄️',
    theme: 'winter',
    startDate: '2026-12-01',
    endDate: '2026-12-31',
    featuredGameIds: [
      'n64-mario-party-3',
      'nds-new-super-mario-bros',
      'nds-animal-crossing-wild-world',
    ],
    xpMultiplier: 3,
  },
  // ── 2027 teaser ───────────────────────────────────────────────────────────
  {
    id: 'spring-showdown-2027',
    name: 'Spring Showdown 2027',
    description: 'A new year, a new season of competition. Details coming soon!',
    emoji: '🌷',
    theme: 'spring',
    startDate: '2027-03-01',
    endDate: '2027-05-31',
    featuredGameIds: [],
    xpMultiplier: 2,
  },
];

/** Return the subset of events that are active on `date` (defaults to now). */
export function getActiveEvents(date: Date = new Date()): SeasonalEvent[] {
  const today = date.toISOString().slice(0, 10); // 'YYYY-MM-DD'
  return SEASONAL_EVENTS.filter((e) => e.startDate <= today && today <= e.endDate);
}

/** Return the next upcoming event (first event that hasn't started yet). */
export function getNextEvent(date: Date = new Date()): SeasonalEvent | undefined {
  const today = date.toISOString().slice(0, 10);
  return SEASONAL_EVENTS.find((e) => e.startDate > today);
}
