# Phase 13 — Seasonal Events, Featured Games & Custom Room Themes

**Status: Complete**

## What was delivered

### Seasonal Events System
- `apps/lobby-server/src/seasonal-events.ts` — event definitions (5 seeded events for
  Spring 2026, Summer 2026, Fall 2026, Winter 2026, Spring 2027)
- `getActiveEvents(date?)` — returns events whose window includes the given date (defaults to now)
- `getNextEvent(date?)` — returns the first upcoming event
- REST: `GET /api/events` — all events + nextEvent pointer
- REST: `GET /api/events/current` — currently active events + nextEvent

### Weekly Featured Games
- `apps/lobby-server/src/featured-games.ts` — static pool of 20 curated game entries
  (Racing, Fighting, Party, Pokémon, Puzzle, Adventure, Action)
- `getFeaturedGames(date?)` — deterministic rotation based on ISO week number;
  returns 6 different games each week from the pool
- REST: `GET /api/games/featured` — current week's featured games

### Custom Room Themes
- `theme?: string` added to `Room` and `CreateRoomPayload` in both server and desktop types
- SQLite migration: `ALTER TABLE rooms ADD COLUMN theme TEXT` runs on startup (idempotent)
- `LobbyManager.createRoom` and `SqliteLobbyManager.createRoom` accept optional `theme`
- `handler.ts` `create-room` case passes `theme` from payload
- 6 themes available: `classic`, `spring`, `summer`, `fall`, `winter`, `neon`
- HostRoomModal: theme picker (3-column grid) lets host choose before creating
- LobbyPage: room card uses themed gradient background + accent border when `theme` is set

### Client-Side Events Service
- `apps/desktop/src/lib/events-service.ts` — typed fetch helpers:
  - `fetchAllEvents()`, `fetchCurrentEvents()`, `fetchFeaturedGames()`
  - `themeGradient(theme)`, `themeAccent(theme)`, `themeLabel(theme)` — UI helpers
  - `ROOM_THEMES` constant — all available themes with id + label

### Events Page (`/events`)
- Full page showing Featured This Week grid + all Seasonal Events
- Event cards with theme-tinted gradient, status badge (● Live / ◆ Upcoming / ✓ Ended)
- Filter tabs: All / Active / Upcoming / Past
- Xp multiplier badge shown for active events
- Room Themes explainer section

### Home Page Enhancements
- **Seasonal Event Banner** (shown when ≥ 1 event is active): themed gradient card with
  emoji, name, description, stat-multiplier badge, and a "Details →" link to `/events`
- **⭐ Featured This Week** widget: 6-column grid of featured game cards with emoji + reason,
  links directly to the game detail page

### Navigation
- `🎪 Events` nav item added to the sidebar in `Layout.tsx`

## API Reference

```
GET /api/events
  → { events: SeasonalEvent[], nextEvent: SeasonalEvent | null }

GET /api/events/current
  → { active: SeasonalEvent[], nextEvent: SeasonalEvent | null }

GET /api/games/featured
  → { featured: FeaturedGame[] }
```

## Known Limitations / Future Work
- Events and featured games are statically seeded; a future phase could move them to a
  database table to allow live editing without code deploys.
- `xpMultiplier` is cosmetic only — no stat-weighting logic is connected to it yet.
- Room themes are visual only; voice/chat UI doesn't change colour to match the theme.
- Featured game rotation ignores real-time play data (no "Trending" algorithm yet).
