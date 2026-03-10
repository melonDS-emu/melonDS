# Current State Audit (Phase 0)

Repository review performed on 2026-03-10. Baseline commands verified locally:
- `npm run typecheck` (pass)
- `timeout 5 npm run start -w @retro-oasis/lobby-server` (boots on ws://localhost:8080, relay ports 9000-9200)
- `timeout 5 npm run dev:desktop` (Vite ready on http://localhost:5173)

## Pages That Render
- [x] Home (`/`): Renders hero + lobby list; lobby cards sourced from live WebSocket `list-rooms` when connected, game tiles from `apps/desktop/src/data/mock-games.ts`.
- [x] Library (`/library`): Filterable grid over `mock-games`; no backend or game-db integration.
- [x] Game Details (`/game/:gameId`): Reads `mock-games`, hosts lobbies via WebSocket `create-room`; invite button is a no-op.
- [x] Lobby (`/lobby/:lobbyId`): Real-time room/ready/chat via WebSocket; requires running lobby server; shows connection diagnostics + relay banner on `game-starting`; does not launch emulators.

## Working vs Mocked Features (checkbox = implemented today)
- [x] Lobby WebSocket protocol: create/join/spectate/leave/list/toggle-ready/chat/ping; in-memory rooms; spectator support.
- [x] Connection diagnostics: clients ping every 10s; server persists `connectionQuality` + `latencyMs` per player and broadcasts `room-updated`.
- [x] Netplay relay allocation: `start-game` broadcasts `game-starting` with relay port (TCP 9000-9200).
- [ ] Game library data: UI uses hard-coded `mock-games`; not wired to `packages/game-db` or ROM scanning.
- [ ] Presence/friends: Sidebar is static; `packages/presence-client` is in-memory stub with no transport.
- [ ] Save/Cloud flows: `packages/save-system` can read/write local disk, but UI/server never call it; cloud sync service is scaffold-only.
- [ ] Emulator launch orchestration: `packages/emulator-bridge` builds/spawns processes but UI/server never invoke it; no automatic launch on `start-game`.
- [ ] Multiplayer metadata: `packages/multiplayer-profiles` and `packages/session-engine` store templates/profiles but are unused by apps or lobby server.
- [ ] UI invites/social: "Invite Friends" button has no behavior; no party/invite links; no auth/identity beyond localStorage display name.

## Dependency Map (apps & packages)
- apps/desktop: React + Vite frontend; depends only on local components/context and mock data. No imports from packages/*.
- apps/lobby-server: WS server using in-memory `LobbyManager` + `NetplayRelay`; no shared package dependencies.
- packages/shared-types: Domain type definitions; dependency for most packages.
- packages/emulator-bridge → shared-types; spawns emulator processes using adapter-built args; includes ROM scanner.
- packages/game-db → shared-types; 30-game seed catalog + query helpers; not consumed by apps.
- packages/session-engine → shared-types; session lifecycle + template store (Phase 1/2 presets); not wired to UI/server.
- packages/multiplayer-profiles → shared-types; per-game multiplayer metadata store; unused.
- packages/save-system → shared-types; local save manager (fs) + cloud-sync scaffold; unused by apps.
- packages/presence-client → shared-types; in-memory presence/friends client; unused.
- packages/ui: placeholder export only.

## Server Events Implemented (apps/lobby-server/src/handler.ts)
- From client: `create-room`, `join-room`, `join-as-spectator`, `leave-room`, `toggle-ready`, `list-rooms`, `start-game`, `chat`, `ping`.
- From server: `welcome`, `room-created`, `room-joined`, `room-left`, `room-updated`, `room-list`, `game-starting` (relayPort/host), `chat-broadcast`, `pong`, `error`.

## Emulator Adapters Present (packages/emulator-bridge)
- NES → FCEUX (`--net` support), SNES → Snes9x (netplay args), GB/GBC/GBA → mGBA (default), SameBoy, VBA-M link cable flags; N64 → Mupen64Plus netplay flags; NDS → melonDS Wi-Fi relay flags. Backends catalog lists bsnes, Gambatte, Project64, DeSmuME as alternates; launch uses `backendId` as executable name.

## Top 10 Broken / Partial Flows (highest impact first)
1) Game launch pipeline stops at relay banner: `start-game` only emits relay port; no emulator process launch, no session token distribution, no room status update to `in-game`.
2) Library + hosting use `mock-games` instead of `packages/game-db` or real ROM scans; metadata/compatibility badges can drift from source of truth.
3) Presence/social is nonfunctional: sidebar friends are hard-coded, `presence-client` has no networking, "Invite Friends" does nothing.
4) Save/Cloud flows are not exposed anywhere; cloud sync is stubbed, and front-end cannot manage saves or conflicts.
5) Netplay relay usability gaps: session token expected on TCP but never sent to clients; relay sessions are never deallocated when games end.
6) No auth/identity: player IDs are random per connection; display names unvalidated; rooms evaporate on server restart (in-memory only).
7) Emulator launch uses `backendId` as executable path and ignores `executableName` from the backend catalog; likely fails unless binaries are named identically and on PATH.
8) Session templates + multiplayer profiles are defined but unused, so lobby/start-game ignores recommended backend/mappings/save rules/latency targets.
9) Lobby readiness UX blocks single-player testing: `allReady` requires >1 player before Start enables, preventing host-only smoke tests.
10) Relay/game-start flow never emits `room-updated` with `relayPort`/status, so clients rely solely on transient banner state and cannot rehydrate after reload.
