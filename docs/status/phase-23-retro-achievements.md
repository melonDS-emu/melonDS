# Phase 23 — Retro Achievement Support

## Status: ✅ Complete

## Goal

Add per-game retro achievement support across the RetroOasis game catalog, inspired by RetroAchievements.org. Each supported game now has a set of in-game milestones players can unlock. Progress is tracked per player and exposed via REST API and a dedicated frontend page.

## Milestones

### Phase 23a — RetroAchievementStore (Backend)

- [x] `RetroAchievementStore` — in-memory store with:
  - 40 achievement definitions across 10 games (4 per game)
  - `getDefinitions()` — all 40 defs
  - `getGameDefinitions(gameId)` — defs for one game
  - `getGameIds()` — list of covered games
  - `getGameSummaries()` — per-game count + point totals
  - `getProgress(playerId)` — create/retrieve player progress
  - `getEarned(playerId, gameId?)` — earned achievements, optionally filtered
  - `unlock(playerId, achievementId, sessionId?)` — idempotent unlock; returns def or null
  - `unlockMany(playerId, achievementIds[], sessionId?)` — batch unlock helper
  - `playerCount()` — number of players tracked

### Phase 23b — REST API (`/api/retro-achievements`)

- [x] `GET  /api/retro-achievements` — all 40 definitions
- [x] `GET  /api/retro-achievements/games` — per-game summaries (count + points)
- [x] `GET  /api/retro-achievements/games/:gameId` — definitions for a specific game
- [x] `GET  /api/retro-achievements/player/:playerId` — player progress (all games)
- [x] `GET  /api/retro-achievements/player/:playerId/game/:gameId` — per-game progress
- [x] `POST /api/retro-achievements/player/:playerId/game/:gameId/unlock` — unlock achievement (body: `{ achievementId, sessionId? }`)

### Phase 23c — Frontend Page (`/retro-achievements`)

- [x] `retro-achievement-service.ts` — typed fetch wrappers for all 6 endpoints
- [x] `RetroAchievementsPage` at `/retro-achievements` featuring:
  - Summary bar: total achievements earned/available, points, games, progress bar
  - Game list sidebar — click to filter by game
  - Achievement grid — locked (faded) / unlocked (highlighted with unlock date)
  - Each badge shows: emoji icon, title, description, point value, unlock date
- [x] `🏅 Retro Achievements` nav item added to Layout sidebar (More group)
- [x] Route registered in `App.tsx`

### Phase 23d — Tests

- [x] `apps/lobby-server/src/__tests__/phase-23.test.ts` — 48 unit tests covering:
  - Catalog structure (40 defs, no duplicate IDs, 10 games, 4 per game)
  - `getDefinitions`, `getGameDefinitions`, `getGameIds`, `getGameSummaries`
  - `getProgress` initialization and idempotency
  - `unlock` — new, duplicate, unknown, sessionId storage
  - `unlockMany` — batch, partial duplicates, unknown IDs, empty list
  - `getEarned` with and without gameId filter
  - `playerCount` tracking
  - Cross-player isolation
  - Specific achievement IDs for all 10 games

## Achievement Catalog

| Game | System | Achievements |
|------|--------|-------------|
| Mario Kart 64 | N64 | First Race, Podium Finish, Blue Shell Survivor, Rainbow Road |
| Super Smash Bros. | N64 | First KO, Untouchable, Last Stock, Comeback Kid |
| Mario Kart DS | NDS | Wi-Fi Winner, Snake Charmer, 150cc Champion, Mission Accomplished |
| Pokémon Diamond | NDS | First Badge, Trade Partner, Elite Four, Pokédex Explorer |
| Pokémon Emerald | GBA | First Catch, Link Battle, Hoenn Champion, Contest Star |
| Tetris | GB | First Clear, Tetris!, Speed Demon, VS Champion |
| Contra | NES | First Contact, Spread Shot, Brothers in Arms, No Man Left Behind |
| Sonic the Hedgehog 2 | Genesis | Emerald Hill Clear, Ring Collector, Super Sonic, Fastest Hedgehog |
| Crash Bandicoot | PSX | Box Smasher, Box Maniac, Gem Hunter, Relentless Bandicoot |
| Super Bomberman | SNES | Boom!, Chain Reaction, Last One Standing, Perfect Bomber |

## Known Limitations

- Achievements are unlocked via explicit API calls (`POST .../unlock`); there is no automatic emulator game-state detection.
- `RetroAchievementStore` is in-memory only; a SQLite-backed variant is future work.
- The `RetroAchievementsPage` displays progress from the connected lobby server; the page shows all achievements as locked when the server is offline.
