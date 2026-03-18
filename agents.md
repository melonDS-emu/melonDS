# RetroOasis Agent Guide

## Current Phase
- Focus: **Phase 15 — Community Hub, Game Ratings & Ranked Play**. Emphasis on community engagement through game reviews, a real-time activity feed, and ELO-based ranked matchmaking.
- Latest work: `GameRatingsStore` (1-5 star reviews + summaries), `ActivityFeedStore` (7 event types, ring buffer), `RankingStore` (ELO K=32, Bronze→Diamond tiers, global + per-game), `CommunityPage` (`/community` — Activity Feed / Game Ratings / Rankings), ranked room toggle in HostRoomModal, player rank badge in Profile page. SQLite-backed stores for reviews and rankings. Phase 15 unit tests (39 tests).

## Build & Test Quickstart
- Install: `npm install` (root workspace installs all packages).
- Type check: `npm run typecheck`.
- Frontend dev server: `npm run dev:desktop` (Vite).
- Lobby server: `npm run dev:server` (WebSocket on ws://localhost:8080).
- Desktop build: `npm run build:desktop`.

## Key Architecture Notes
- Lobby server + relay: WebSocket lobby on port 8080; netplay relay allocates TCP ports 9000–9200 and attaches `relayPort` to `game-starting` events. Session token is the first 36 ASCII bytes on relay connections.
- Emulator bridge: Use `createSystemAdapter(system, backendId?)` for launch args; link cable netplay flags are baked into adapters (mGBA/SameBoy/VBA-M) and N64 uses `--netplay-host/--netplay-port`.
- Session templates: `packages/session-engine/src/templates.ts` contains Phase 1 defaults plus N64 and NDS presets.
- Phase 8 persistence: set `DB_PATH` env var to enable SQLite-backed lobby, session history, saves, friends, matchmaking, and identity. In-memory fallback used otherwise.
- Phase 9 achievements: `apps/lobby-server/src/achievement-store.ts` — 20 definitions; `player-stats.ts` — aggregation + leaderboard. REST: `/api/achievements`, `/api/stats/:displayName`, `/api/leaderboard`.
- Phase 15 community: `game-ratings.ts` (reviews + summaries), `activity-feed.ts` (ring buffer, 7 event types), `ranking-store.ts` (ELO, global + per-game). REST: `/api/reviews/*`, `/api/activity`, `/api/rankings/*`. Frontend: `CommunityPage` at `/community`, rank badge in `ProfilePage`, ranked/casual toggle in `HostRoomModal`.

## Social/Diagnostics Behavior
- Client pings every 10s; server now updates player `connectionQuality` and `latencyMs`, then broadcasts `room-updated` so the lobby UI can show per-player dots and latency.
- Lobby UI banner exposes relay host/port when a game starts; Connection Diagnostics shows your measured ping and quality.

## Agent Expectations
- Keep Phase 15 roadmap in sync (docs/mvp/roadmap.md) when marking milestones.
- Run `npm run typecheck` after changes touching TS packages.
- Prefer existing packages over new dependencies; respect emulator adapter conventions when adding backends or netplay flags.
