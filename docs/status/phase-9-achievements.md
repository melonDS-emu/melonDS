# Phase 9 Status — Achievements & Player Stats

> **Current state (2026-03-11):** Phase 9 is **fully complete**. All three remaining items are now implemented: SQLite-backed achievement persistence, achievement unlock WebSocket push with in-app toast notifications, and the `/api/achievements/:playerId/refresh` on-demand endpoint.

---

## What was completed this phase

### Achievement engine (`apps/lobby-server/src/achievement-store.ts`)

- **20 achievement definitions** across 5 categories: Sessions, Social, Systems, Games, Streaks.
- `AchievementStore` — in-memory per-player tracker; `checkAndUnlock()` evaluates all definitions against a player's session history and returns newly unlocked achievements.
- Definitions are intentionally deterministic and threshold-based so they can be re-computed at any time without persisting intermediate state.

### SQLite achievement persistence (`apps/lobby-server/src/sqlite-achievement-store.ts`)

- `SqliteAchievementStore` — drop-in SQLite-backed replacement for `AchievementStore`; follows the same pattern as `SqliteSessionHistory`.
- Two new tables added to `db.ts`: `player_achievements` (per-player unlock rows) and `player_achievement_meta` (display name + last-checked timestamp).
- Enabled automatically when `DB_PATH` is set; falls back to in-memory otherwise.
- 8 new tests added to `sqlite-stores.test.ts` covering persistence, idempotency, and playerCount.

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
| `POST` | `/api/achievements/:playerId/refresh` | Re-evaluate achievements against live session history; returns earned list + newly unlocked IDs |
| `GET` | `/api/stats/:displayName` | Aggregate stats for a player |
| `GET` | `/api/leaderboard` | Global leaderboard (`?metric=sessions\|playtime\|games\|systems&limit=N`) |

### Achievement WebSocket push + desktop toast (`handler.ts`, `LobbyContext.tsx`)

- After a session ends (room deleted on last player leave or disconnect), the server calls `checkAndUnlock()` for each player in the session.
- Any newly earned achievements are pushed to the player's active WebSocket connection as `achievement-unlocked` messages.
- The desktop `LobbyContext` handles `achievement-unlocked` messages by calling `addToast()` — a new minimal toast notification system (`ToastContext.tsx` + `ToastContainer.tsx`).
- Toasts appear in the bottom-right corner with the achievement icon, name, and description; auto-dismiss after 6 s.
- `displayNameToPlayerId` module-level map in `handler.ts` enables the server to locate a player's WS by display name at session-end time.

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
- `player-stats.test.ts` — 15 tests covering aggregation, system grouping, top games, leaderboard ranking.
- `sqlite-stores.test.ts` — 8 new tests for `SqliteAchievementStore` (persistence, idempotency, playerCount).
- All 146 lobby-server tests pass; all 40 desktop tests pass.

---

## Known limitations / future work

- **Achievement unlock WS push on disconnect** — when the last player *disconnects* (rather than explicitly leaving), `connections.delete(playerId)` is called before `endSession`, so the disconnecting player cannot receive the WS push. Achievements are still persisted; the player will see them on the Profile page. The `POST /api/achievements/:playerId/refresh` endpoint provides on-demand re-evaluation.
- **Player ID vs. display name** — the current implementation uses display name as a proxy for player identity in the stats and leaderboard endpoints (since that's what session records store). Phase 8 player identity tokens would give a more stable player key.
