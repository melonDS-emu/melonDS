# RetroOasis Agent Guide

## Current Phase
- Focus: **Phase 9 — Achievements & Player Stats**. Emphasis on rewarding engagement with an achievement system and giving players visibility into their playtime history through stats and leaderboards.
- Latest work: Achievement engine (20 definitions, 5 categories), player stats aggregation, global leaderboard, REST endpoints, Profile page (`/profile`), and a home-page leaderboard teaser widget.

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

## Social/Diagnostics Behavior
- Client pings every 10s; server now updates player `connectionQuality` and `latencyMs`, then broadcasts `room-updated` so the lobby UI can show per-player dots and latency.
- Lobby UI banner exposes relay host/port when a game starts; Connection Diagnostics shows your measured ping and quality.

## Agent Expectations
- Keep Phase 9 roadmap in sync (docs/mvp/roadmap.md) when marking milestones.
- Run `npm run typecheck` after changes touching TS packages.
- Prefer existing packages over new dependencies; respect emulator adapter conventions when adding backends or netplay flags.
