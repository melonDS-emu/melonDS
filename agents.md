# RetroOasis Agent Guide

## Current Phase
- Focus: **Phase 2 — N64 + Enhanced Social**. Emphasis on N64 readiness, party-game polish, and richer lobby diagnostics.
- Latest work: Lobby pings now persist connection quality per player and broadcast it to the room; the desktop lobby shows a Connection Diagnostics panel with latency/quality plus relay endpoint details.

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

## Social/Diagnostics Behavior
- Client pings every 10s; server now updates player `connectionQuality` and `latencyMs`, then broadcasts `room-updated` so the lobby UI can show per-player dots and latency.
- Lobby UI banner exposes relay host/port when a game starts; Connection Diagnostics shows your measured ping and quality.

## Agent Expectations
- Keep Phase 2 roadmap in sync (docs/mvp/roadmap.md) when marking milestones.
- Run `npm run typecheck` after changes touching TS packages.
- Prefer existing packages over new dependencies; respect emulator adapter conventions when adding backends or netplay flags.
