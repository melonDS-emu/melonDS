# Phase 9 Status — Achievements & Player Stats

> **Current state (2026-03-11):** Phase 9 is complete. The lobby server now exposes achievement definitions, per-player earned achievements, individual player stats, and a global leaderboard. The desktop app has a new Profile page surfacing all of this data plus a top-players widget on the home page.

---

## What was completed this phase

### Achievement engine (`apps/lobby-server/src/achievement-store.ts`)

- **20 achievement definitions** across 5 categories: Sessions, Social, Systems, Games, Streaks.
- `AchievementStore` — in-memory per-player tracker; `checkAndUnlock()` evaluates all definitions against a player's session history and returns newly unlocked achievements.
- Definitions are intentionally deterministic and threshold-based so they can be re-computed at any time without persisting intermediate state.

**Achievement categories:**
| Category | Achievements |
|----------|-------------|
| Sessions | First Power-On, Getting Warmed Up, Regular, Veteran, Marathon Runner, Endurance Test |
| Social | Party of Four, Social Butterfly, Host Master |
| Systems | N64 Debut, DS Online, Retro Purist, System Collector |
| Games | Mario Kart Fan, Pokémon Trainer, Party Animal |
| Streaks | Daily Player, Comeback King |

### Player stats service (`apps/lobby-server/src/player-stats.ts`)

- `computePlayerStats()` — aggregates session history into a rich stats object:
  - Total / completed sessions, total playtime, avg session, longest session
  - Unique playmates count
  - Per-system breakdown (sorted by session count)
  - Top-5 games (by sessions)
  - Active days (distinct calendar days)
  - First / last session timestamps
- `computeLeaderboard()` — ranks all players by sessions, playtime, unique games, or systems played.

### New REST endpoints (`apps/lobby-server/src/index.ts`)

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/achievements` | All achievement definitions |
| `GET` | `/api/achievements/:playerId` | Earned achievements for a player |
| `GET` | `/api/stats/:displayName` | Aggregate stats for a player |
| `GET` | `/api/leaderboard` | Global leaderboard (`?metric=sessions\|playtime\|games\|systems&limit=N`) |

### Desktop — Profile page (`apps/desktop/src/pages/ProfilePage.tsx`)

- Player name lookup field (pre-filled from `localStorage.displayName`).
- **Stats panel** — 8 stat cards: sessions, playtime, avg session, active days, longest session, playmates, systems, games.
- **Top Games** list with session count and playtime per game.
- **Achievement grid** — all 20 achievements displayed with locked/unlocked state, unlock date tooltip, and progress bar.
- **Category filter tabs** — All / Sessions / Social / Systems / Games / Streaks.
- **Global Leaderboard** — sortable by 4 metrics with medal icons for top 3.
- Route: `/profile`; nav item added to sidebar.

### Desktop — Home page leaderboard widget (`apps/desktop/src/pages/HomePage.tsx`)

- "🏆 Top Players" section showing the top 5 players by session count.
- Loads on mount via `fetchLeaderboard()` (silently skipped if server is unavailable).
- "Full leaderboard →" link navigates to the Profile page.

### Stats service (`apps/desktop/src/lib/stats-service.ts`)

- Typed fetch wrappers: `fetchAchievementDefs`, `fetchPlayerAchievements`, `fetchPlayerStats`, `fetchLeaderboard`.
- All calls are error-tolerant (return empty/null if server is down — dev mode friendly).
- `formatDuration(secs)` helper: converts seconds to human-readable "Xh Ym" / "Ym" / "Xs".

### Tests

- `achievement-store.test.ts` — 21 tests covering all unlock predicates.
- `player-stats.test.ts` — 13 tests covering aggregation, system grouping, top games, leaderboard ranking.
- All 136 lobby-server tests pass; all 40 desktop tests pass.

---

## Known limitations / future work

- **Achievements are not persisted** — they are re-computed on each API call from in-memory session history. If the server restarts, session history and earned achievements are lost unless `DB_PATH` is set. A future iteration should add `achievements` to the SQLite schema (follow the `SqliteSessionHistory` pattern).
- **Achievement checking is on-demand** — the `/api/achievements/:playerId` endpoint returns what was last unlocked but does not re-evaluate against live session data on every request. To get an up-to-date view, callers should also call `/api/stats/:displayName` alongside. A background job or a `POST /api/achievements/:playerId/refresh` endpoint would make this cleaner.
- **Player ID vs. display name** — the current implementation uses display name as a proxy for player identity in the stats and leaderboard endpoints (since that's what session records store). Phase 8 player identity tokens would give a more stable player key.
