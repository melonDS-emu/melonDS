# Current State Audit (Phase 0)

Repository review performed on 2026-03-10. Baseline commands verified locally after `npm install`:
- `npm run typecheck` (pass)
- `timeout 5 npm run start -w @retro-oasis/lobby-server` (boots on ws://localhost:8080, relay ports 9000-9200)
- `timeout 5 npm run dev:desktop` (Vite ready on http://localhost:5173)

## Pages That Render
- [x] Home (`/`): Renders hero + lobby list; lobby cards sourced from live WebSocket `list-rooms` when connected, game tiles from `apps/desktop/src/data/mock-games.ts`.
- [x] Library (`/library`): Filterable grid over `mock-games`; no backend or game-db integration.
- [x] Game Details (`/game/:gameId`): Reads `mock-games`, hosts lobbies via WebSocket `create-room`; invite button is a no-op.
- [x] Lobby (`/lobby/:lobbyId`): Real-time room/ready/chat via WebSocket; requires running lobby server; shows connection diagnostics + relay banner on `game-starting`; can trigger the local launch API when ROM settings are configured, but the full emulator handoff is still partial.

## Working vs Mocked Features (checkbox = implemented today)
- [x] Lobby WebSocket protocol: create/join/spectate/leave/list/toggle-ready/chat/ping; in-memory rooms; spectator support.
- [x] Connection diagnostics: clients ping every 10s; server persists `connectionQuality` + `latencyMs` per player and broadcasts `room-updated`.
- [x] Netplay relay allocation: `start-game` broadcasts `game-starting` with relay port (TCP 9000-9200).
- [x] Emulator launch plumbing: the lobby server exposes `POST /api/launch`, and the desktop client posts to it automatically when a game starts if ROM settings are configured.
- [ ] Game library data: UI uses hard-coded `mock-games`; not wired to `packages/game-db` or ROM scanning.
- [ ] Real ROM ownership flow: the desktop app stores a ROM directory path, but it still does not scan real files into the library or associate a specific ROM file with each game card.
- [ ] Presence/friends: the UI is powered by `packages/presence-client`, but it is still an in-memory mock with no transport or persistence.
- [ ] Save/Cloud flows: `packages/save-system` can read/write local disk, but UI/server never call it; cloud sync service is scaffold-only.
- [ ] Multiplayer metadata: `packages/session-engine` is still unused at runtime, and `packages/multiplayer-profiles` are only surfaced as read-only Settings data rather than launch-time/runtime configuration.
- [ ] UI invites/social: "Invite Friends" button has no behavior; no party/invite links; no auth/identity beyond localStorage display name.

## Dependency Map (apps & packages)
- apps/desktop: React + Vite frontend; uses `packages/presence-client` for mock social data and `packages/multiplayer-profiles` for the Settings controller-profile reference UI, while game/library data still defaults to bundled mock data.
- apps/lobby-server: WebSocket server using in-memory `LobbyManager` + `NetplayRelay`; also hosts the local `/api/launch` bridge on the same port and depends on `@retro-oasis/emulator-bridge` for process spawning.
- packages/shared-types: Domain type definitions; dependency for most packages.
- packages/emulator-bridge → shared-types; spawns emulator processes using adapter-built args; includes ROM scanner.
- packages/game-db → shared-types; 30-game seed catalog + query helpers; not consumed by apps.
- packages/session-engine → shared-types; session lifecycle + template store (Phase 1/2 presets); not wired to UI/server.
- packages/multiplayer-profiles → shared-types; default controller profiles plus input metadata; consumed by the desktop Settings page.
- packages/save-system → shared-types; local save manager (fs) + cloud-sync scaffold; unused by apps.
- packages/presence-client → shared-types; in-memory presence/friends client; consumed by the desktop app in mock/dev mode.
- packages/ui: placeholder export only.

## Server Events Implemented (apps/lobby-server/src/handler.ts)
- From client: `create-room`, `join-room`, `join-as-spectator`, `leave-room`, `toggle-ready`, `list-rooms`, `start-game`, `chat`, `ping`.
- From server: `welcome`, `room-created`, `room-joined`, `room-left`, `room-updated`, `room-list`, `game-starting` (relayPort/host), `chat-broadcast`, `pong`, `error`.

## Emulator Adapters Present (packages/emulator-bridge)
- NES → FCEUX (`--net` support), SNES → Snes9x (netplay args), GB/GBC/GBA → mGBA (default), SameBoy, VBA-M link cable flags; N64 → Mupen64Plus netplay flags; NDS → melonDS Wi-Fi relay flags. Backends catalog lists bsnes, Gambatte, Project64, DeSmuME as alternates; launch uses `backendId` as executable name.

## Top 10 Broken / Partial Flows (highest impact first)
1) Game launch pipeline is only partially real: `start-game` now emits relay port + per-player session tokens and triggers the local launch API, but there is still no true per-game ROM file selection — the desktop only knows a configured ROM path, not which ROM file matches a room.
2) Library + hosting use `mock-games` instead of `packages/game-db` or real ROM scans; metadata/compatibility badges can drift from source of truth.
3) Presence/social is still mock-driven: `presence-client` seeds realistic demo friends/activity, but it has no networking, persistence, or friend-management flow, and "Invite Friends" still does nothing.
4) Save/Cloud flows are UI-only today: the Saves page can manage seeded local records and conflicts in-browser, but cloud sync is still stubbed and the front-end never reaches the real filesystem-backed `packages/save-system` layer.
5) Netplay relay usability gaps: relay sessions are never deallocated when games end naturally, `relayHost` is still hardcoded to `localhost`, and the per-player session token is not yet handed off to the launched emulator process.
6) No auth/identity: player IDs are random per connection; display names unvalidated; rooms evaporate on server restart (in-memory only).
7) Emulator launch still assumes binaries are available on PATH and that the configured ROM path is launchable as-is, so local setup and ROM association errors are still likely.
8) Session templates are still unused at runtime, and multiplayer profiles are only surfaced as read-only reference data, so lobby/start-game still ignore recommended backend/save-rule/latency settings.
9) Lobby readiness UX blocks single-player testing: `allReady` requires >1 player before Start enables, preventing host-only smoke tests.
10) Reload/reconnect during an active game still loses player-specific launch context: `room-updated` now persists `relayPort`/status, but the one-time `sessionToken` is not rehydrated after reload, so the client cannot automatically reconnect a launched emulator.
