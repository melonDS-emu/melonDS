# RetroOasis Development Roadmap

> **Honest progress snapshot (2026-03-12)**
>
> - The current working stack is **React + Vite desktop UI + WebSocket lobby server + TCP relay + local HTTP launch API (`/api/launch`)**.
> - **Tauri/native packaging, a C++ IPC bridge into the melonDS core in `/src`, and a broader REST backend for catalog/save/presence data are still not complete.**
> - The lobby now triggers **local emulator launch requests** through the server, but **real per-game ROM discovery/selection and relay token handoff into emulator processes are still incomplete**.
> - Presence, activity, saves, and cloud flows have useful UI scaffolding, but the **live/server-backed versions remain future work unless explicitly marked “mock/dev/demo.”**
> - Phase 6 DSiWare support and interactive touch calibration panel are now complete. Phase 7 backend hardening has started with per-IP rate limiting on the lobby server.
> - Phase 8 SQLite persistence, friend system, matchmaking queue, player identity, and enhanced session stats are now complete. Set `DB_PATH` env var to enable persistence across restarts.
> - Phase 9 achievements (20 definitions + 3 tournament achievements), player stats aggregation, global leaderboard, Profile page, SQLite achievement persistence, WebSocket achievement push + desktop toast notifications, and the `/api/achievements/:playerId/refresh` endpoint are now complete.
> - Phase 10 tournaments (single-elimination bracket engine, REST + WS push, desktop Tournament page, winner achievements) and clip sharing (MediaRecorder service, IndexedDB storage, export, home page widget) are now complete.
> - Phase 11 clip library (`/clips`), live friend presence, live friend request UI, notification badges, `SqliteTournamentStore`, and tournament history in Profile page are now complete.
> - Phase 12 WFC auto-config (Wiimmfi + AltWFC, `GET /api/wfc/providers`), Pokémon Online page (`/pokemon` — Trade Lobby, Battle Lobby, Friend Codes), and Mario Kart DS matchmaking page (`/mario-kart` — Quick Race, Public Rooms, Ranked Races) are now complete.
> - **Next: further phases — see Future Ideas section below.**

## Phase 1 — Foundation (Complete)

**Goal:** Polished multiplayer-first retro Nintendo app for simpler systems.

### Systems
- NES, SNES, GB, GBC, GBA

### Milestones
- [x] Monorepo structure
- [x] Core TypeScript domain types
- [x] Game catalog with multiplayer seed data (30 games)
- [x] Emulator bridge interfaces and backend definitions
- [x] Save system scaffold (local + cloud models)
- [x] Session engine scaffold with Phase 1 default templates
- [x] Presence client scaffold
- [x] Multiplayer profiles + input management
- [x] React frontend scaffold with mocked data
- [x] WebSocket lobby server prototype
- [x] Architecture and UX documentation
- [x] WebSocket-connected lobby in frontend (host/join rooms, ready state, chat)
- [x] Host a Room modal with game selection
- [x] Join by Room Code modal (with Spectate option)
- [x] Lobby page with real-time player list, ready-up, host controls, and chat
- [x] Disconnect cleanup (rooms closed when host disconnects)
- [x] Host migration (host transferred on disconnect)
- [x] Room code one-click copy with visual confirmation
- [x] Spectator mode (join-as-spectator, spectator list in lobby)
- [x] Ping/pong heartbeat with live latency display
- [x] Connection quality indicators per player
- [x] Netplay TCP relay server (ports 9000–9200, room-scoped sessions)
- [x] Local child_process emulator launching via the server `/api/launch` endpoint (SIGTERM stop)
- [x] Emulator netplay args per backend (FCEUX, Snes9x, mGBA, Mupen64Plus, melonDS)
- [x] ROM file scanner (by extension, recursive, grouped by system)
- [x] Save file real I/O (fs/promises create, delete, import, export)
- [x] Relay port surfaced to frontend on game-starting event
- [x] Session token surfaced to players on `game-starting`
- [ ] Tauri integration for native desktop app
- [x] Settings page for ROM/save directories and display name
- [x] Library refresh affordance tied to the configured ROM path (currently simulated until native filesystem IPC is added)
- [x] Read-only controller profile reference UI in Settings
- [ ] Native ROM scan + per-game ROM file selection wired into the desktop UI
- [x] Relay session token handoff from `game-starting` into emulator processes (`RETRO_OASIS_SESSION_TOKEN` env var)
- [ ] Editable controller remapping UI
- [x] Game catalog REST backend (`/api/games`, `/api/systems`) now live on the lobby server; backed by `@retro-oasis/game-db` seed data
- [ ] Cloud save sync implementation
- [ ] Friend invite flow (room code sharing via external messaging)

## Phase 2 — N64 + Enhanced Social

**Goal:** Add N64 support with party game focus, plus richer social features.

### Systems
- Add: Nintendo 64

### Milestones
- [x] N64 session templates (Mupen64Plus, 2P + 4P defaults, MK64, Smash Bros presets)
- [x] Netplay relay supports N64 packet forwarding
- [x] SameBoy backend added (GB/GBC — best-in-class accuracy + link cable via TCP)
- [x] VisualBoyAdvance-M (VBA-M) backend added (GB/GBC/GBA — link cable via TCP)
- [x] System adapters updated with backend-specific link cable args (SameBoy, VBA-M)
- [x] GB Pokémon Gen I catalog entries (Red, Blue, Yellow — link/trade)
- [x] GBC link game catalog entries (Zelda Oracle of Ages, Oracle of Seasons)
- [x] GBA Pokémon Gen III catalog entries (Ruby, Sapphire, LeafGreen — link/trade)
- [x] GBA Four Swords catalog entry (4-player link co-op)
- [x] Session templates for all new Pokémon link/trade games
- [x] Session template for Zelda Four Swords 4P
- [x] N64 backend integration: Mupen64Plus install/detection notes + analog calibration guidance
- [x] N64 playerSlot support in adapter (--netplay-player arg)
- [x] N64 default input profiles: Xbox, PlayStation, keyboard (analog stick mappings)
- [x] Session templates: Mario Party 2, GoldenEye 007, Diddy Kong Racing (4P)
- [x] Mock game catalog expanded with Mario Party 2, GoldenEye 007, Diddy Kong Racing
- [x] "Best 4P" badge on party game cards (N64 4-player games)
- [x] Party hint banners on game detail pages (N64-specific messaging)
- [x] N64 party spotlight section on home page
- [x] Game summary card in lobby with party context and system badge
- [x] Open slots invite nudge in lobby (share room code prompt)
- [x] Connection diagnostics UI
- [x] Spectator mode (Phase 1 carry-over) ✅
- [ ] Tauri integration for native desktop app (Phase 1 carry-over)
- [x] Read-only controller profile UI shows N64 default mappings
- [x] Editable N64 controller remapping UI (all systems; bindings saved to localStorage)
- [x] Voice chat hooks (WebRTC mesh; mute/unmute + per-peer talking indicators in lobby)
- [x] Party activity feed (mock/dev presence data)

## Phase 3 — Nintendo DS + Premium Features

**Goal:** Add DS support as a premium differentiator with unique UX.

### Systems
- Add: Nintendo DS

### Milestones
- [x] melonDS session templates (2P + 4P defaults, WFC/Pokémon presets)
- [x] WFC config in session templates (Wiimmfi DNS server)
- [x] NDS Pokémon WFC presets (Diamond, Pearl, Platinum, HGSS, Mario Kart DS)
- [x] Dual-screen layout options in session templates
- [x] melonDS netplay args in system adapter (--wifi-host / --wifi-port)
- [ ] melonDS integration (this repository's core — C++ build + IPC bridge)
- [x] Dual-screen layout controls UI (stacked, side-by-side, focus modes)
- [x] Touch input mapping (mouse/touchpad)
- [x] DS wireless-inspired room UX (animated signal bars + "Scanning for DS nearby" in open slots)
- [x] Advanced compatibility badges
- [x] Screen swap hotkeys
- [x] Touch input calibration panel (offsetX/Y, scaleX/Y; persisted in localStorage)

## Phase 5 — Presence and Social Discovery

**Goal:** Make RetroOasis feel alive even before a session starts.

**Current reality:** the discovery UI is in place, but today it is powered by seeded mock/dev data rather than live presence transport or persistence.

### Milestones
- [x] `FriendInfo` enriched with `roomCode` for direct join surfacing
- [x] `RecentActivity` type added to presence-client (join/start/finish/online events)
- [x] `OnlineStatus` and `ActivityEventType` exported as standalone types
- [x] `PresenceClient.seedMockFriends()` — realistic mock friends for dev/demo
- [x] `PresenceClient.getMockRecentActivity()` — mock activity feed seeded from friends
- [x] `PresenceContext` React context exposing friends, onlineFriends, joinableSessions, recentActivity
- [x] `PresenceProvider` seeds mock friends + activity on mount
- [x] `PresenceProvider` mounted in app root alongside LobbyProvider
- [x] Sidebar friends panel — dynamic list from context, hover-reveal Join button, online count
- [x] Friends nav item added to sidebar with green online-count badge
- [x] `JoinRoomModal` accepts optional `initialCode` prop for pre-filled join from friend
- [x] Home page "Friends Playing Now" section — friend session cards with ▶ Join button
- [x] Home page "Recent Activity" section — timestamped feed with inline join actions
- [x] `/friends` route + `FriendsPage` — full social discovery (spotlight, filter tabs, roster, activity feed)
- [ ] Connect PresenceClient to lobby WebSocket for live friend presence
- [ ] Friend request / add-friend flow
- [ ] Server-side friend list and presence persistence
- [ ] Push notifications for joinable sessions

## Phase 6 — Nintendo DS Premium Experience

**Goal:** Make Nintendo DS feel like a standout feature, not an afterthought.

### Systems
- Focus: Nintendo DS (melonDS core already embedded in /src)

### Milestones
- [x] NDS default input profiles: Xbox, PlayStation, keyboard (DS button + touchscreen mappings)
- [x] Enhanced NDS system adapter: DeSmuME fallback, touchEnabled arg, WFC DNS override arg
- [x] melonDS backend enriched with full install/setup notes (all platforms + BIOS guide)
- [x] DeSmuME backend enriched with install notes and netplay caveats
- [x] Additional DS session templates: New Super Mario Bros., Metroid Prime Hunters, Tetris DS, Mario Party DS, Zelda Phantom Hourglass, Pokémon Black/White
- [x] DS showcase games expanded in mock catalog (NDS system color #E87722)
- [x] Pokémon Black and White WFC presets added
- [x] GameDetailsPage: DS dual-screen tip, WFC Online badge, WFC explanation callout
- [x] LobbyPage: DS session hints (Pokémon, kart, hunters, Tetris, party context)
- [x] LobbyPage: collapsible DS Controls & Setup Guide panel (compact mode)
- [x] DSControlsGuide component: button mapping table, touchscreen explanation, screen layout reference, WFC info
- [x] HomePage: Nintendo DS Spotlight section with setup callout
- [x] docs/status/phase-6-ds.md: phase status, known-good showcase games list
- [x] roadmap.md updated with Phase 6 milestones
- [ ] melonDS integration (this repository's C++ core → IPC bridge for launch/control)
- [x] Dual-screen layout picker in HostRoomModal (stacked / side-by-side / focus modes)
- [x] Screen swap hotkey displayed in lobby (F11 reminder)
- [x] Touch input calibration panel — interactive visual test area + offsetX/Y/scaleX/Y controls
- [x] DS-specific compatibility badges (WFC Online, Touch Controls, Download Play)
- [x] DSi mode detection and DSiWare support — dsiMode adapter option, DSiWare mock games, DSi badge/tips in UI, DSiWare session templates

## Cross-Cutting Quality Work

**Goal:** Keep the current prototype honest, debuggable, and safe to extend while native packaging and deeper backend work are still in progress.

### Milestones
- [x] `docs/status/debug-pass.md` — targeted bug-fix pass across lobby/frontend/server flows
- [x] `docs/status/current-state-audit.md` — honest working-vs-mocked inventory
- [x] `docs/status/full-audit.md` — full product/code audit with gap analysis
- [x] Phase status notes for N64, saves, presence, and DS work
- [x] Desktop API smoke tests (`apps/desktop/src/lib/__tests__/api-client.test.ts`, 14 tests)
- [x] Workspace validation documented and re-verified after `npm install` (`npm run typecheck`, `npm run test -w @retro-oasis/desktop`)
- [x] Custom controller profile persistence tests (`custom-profiles.test.ts`, 8 tests)
- [x] DS touch calibration service tests (`touch-calibration.test.ts`, 8 tests)
- [x] Rate limiter unit tests (rate-limiter.test.ts, 8 tests)
- [ ] Integration coverage for emulator launch, relay token flow, ROM scanning, save I/O, and live presence
- [ ] Native desktop verification checklist for Tauri / IPC flows
- [ ] Backend hardening: auth/identity, rate limiting, persistence, and deployable relay host configuration

## Current Carry-Over / Blockers

- [ ] **Tauri native shell and IPC command surface for desktop-only capabilities**
  - `apps/desktop/src/lib/tauri-ipc.ts` defines the full typed command surface
    (`tauriScanRoms`, `tauriPickFile`, `tauriPickDirectory`, `tauriLaunchEmulator`)
    with HTTP-API fallbacks so the app runs in both native and browser/dev mode.
  - Remaining: scaffold `src-tauri/` (Rust, `tauri.conf.json`, `Cargo.toml`),
    implement matching Rust command handlers, wire into the npm build.

- [ ] **C++ IPC bridge between the TypeScript launcher and the melonDS core in `/src`**
  - `apps/lobby-server/src/melonds-ipc.ts` defines the full `MelonDsIpcBridge`
    interface + protocol message types (`MelonDsCommand` / `MelonDsResponse`).
    A `MelonDsIpcStub` provides no-op responses for dev/test use.
  - Remaining: compile the C++ core via the existing CMake build, implement the
    Unix socket / named-pipe server in C++, replace the stub with a real socket
    client, wire the sidecar launch into Tauri or Node.js.

- [ ] **Backend expansion beyond WebSocket rooms + `/api/launch`**
  - Catalog endpoints (`GET /api/games`, `GET /api/games/:id`,
    `GET /api/games/search`, `GET /api/systems`) are now live on the lobby
    server, backed by `@retro-oasis/game-db`. `VITE_API_MODE=backend` is
    functional for catalog queries.
  - Remaining: auth/identity layer, rate limiting, save-sync endpoints, presence
    persistence, deployable relay-host configuration.

- [ ] **Real ROM ownership/discovery flow: filesystem scan, file association, and per-room ROM selection**
  - `GET /api/roms/scan?dir=<path>&recursive=<bool>` endpoint added to the lobby
    server (delegates to the existing `scanRomDirectory` scanner).
  - `apps/desktop/src/lib/rom-library.ts` added: per-game ROM file associations
    stored in localStorage (`setRomAssociation`, `getRomAssociation`,
    `resolveGameRomPath`).
  - `LobbyContext` now resolves the game-specific ROM path via
    `resolveGameRomPath(gameId)` instead of using the raw ROM directory.
  - Remaining: native file-picker UI in Settings / game detail page (requires
    Tauri `tauriPickFile`), visible association status per game in Library page,
    server-side ROM catalog persistence.

- [x] **Relay authentication handoff: feed per-player session tokens into launched emulator processes**
  - `LaunchOptions.sessionToken` added to `@retro-oasis/emulator-bridge`.
  - Token is injected as `RETRO_OASIS_SESSION_TOKEN` env var on the spawned
    emulator child process.
  - `/api/launch` now accepts and forwards `sessionToken`.
  - `LobbyContext` includes `sessionToken` in the `/api/launch` request body.

## Phase 7 — Backend Hardening + Advanced Integration

**Goal:** Make the server production-ready and wire together the remaining loose ends.

### Milestones
- [x] Per-IP token-bucket rate limiter on lobby server (WebSocket connections + HTTP requests)
- [x] Rate limiter unit tests (8 tests)
- [x] Auth/identity layer: player display name validation (length, control-char filtering, whitespace normalisation) + HMAC-signed room ownership tokens (kick-player action, owner token returned on room-created)
- [x] Auth unit tests (13 tests)
- [x] Save-sync REST endpoints: `POST/GET /api/saves/:gameId`, `GET/DELETE /api/saves/:gameId/:saveId`; optimistic-concurrency conflict detection via version counter (in-memory SaveStore)
- [x] Save-store unit tests (9 tests)
- [x] Session history tracking: in-memory `SessionHistory`; `GET /api/sessions` and `GET /api/sessions/:roomId`; sessions recorded on game-start and ended on room close
- [x] Session history unit tests (7 tests)
- [x] Live presence transport: server broadcasts `presence-update` on join/leave/start; `LobbyContext` exposes `onlinePlayers` state updated from WS events
- [x] Deployable relay host configuration: `RELAY_HOST`, `RELAY_PORT_MIN`, `RELAY_PORT_MAX`, `RELAY_BIND` env-variable overrides
- [x] Room ownership tokens surfaced in `LobbyContext` (`ownerToken` state) for use in future kick/reclaim-host UI
- [x] Integration test suite for lobby server (WebSocket connect/create/join/start/kick/ping flows — 12 tests)
- [x] Push notification service (`NotificationService` + `notificationService` singleton) with browser Notification API, permission management, and convenience helpers (friendOnline, friendInLobby, gameStarting, roomReady)
- [x] Push notification service unit tests (10 tests)
- [ ] Friend request / add-friend flow with server-side friend list
- [ ] Backend persistence: SQLite or flat-file store for rooms, players, and session history
- [ ] Server-side friend list and presence persistence


## Future Ideas
- Tournament-style rooms
- Seasonal featured games
- Ranked/casual matchmaking tags
- Custom room themes
- Game clip sharing
- Mobile companion app

## Phase 8 — Persistence, Matchmaking, and Social Graph

**Goal:** Persist data across server restarts, add a real matchmaking queue, and build a server-side social graph for friends.

### Milestones
- [x] SQLite persistence layer: schema for rooms, players, session history, friends
- [x] Migrate in-memory `LobbyManager`, `SessionHistory`, and `SaveStore` to SQLite-backed implementations
- [x] Server-side friend list: `POST /api/friends/add`, `GET /api/friends`, `DELETE /api/friends/:userId`
- [x] Live friend presence via WebSocket: broadcast friend status changes to subscribed clients
- [x] Friend request / accept / decline flow
- [x] Push notifications for joinable friend sessions (wired to live presence + browser Notification API)
- [x] Matchmaking queue: `POST /api/matchmaking/join`, `DELETE /api/matchmaking/leave`, auto-create room when enough players matched
- [x] Player identity / display name persistence (server-assigned or user-chosen, stored per connection)
- [x] Session history REST API enhancements: per-player stats, most-played games, total session time
- [x] Relay host deployment guide: Docker Compose config, env-variable reference, reverse-proxy notes

## Phase 9 — Achievements & Player Stats

**Goal:** Reward engagement with a persistent achievement system and give players visibility into their playtime history through stats and leaderboards.

### Milestones
- [x] Achievement engine: 20 definitions across 5 categories (Sessions, Social, Systems, Games, Streaks)
- [x] `AchievementStore` — in-memory per-player tracker with `checkAndUnlock()` predicate engine
- [x] Player stats aggregation: `computePlayerStats()` — sessions, playtime, top games, systems, active days
- [x] Global leaderboard: `computeLeaderboard()` — rank by sessions, playtime, unique games, or systems
- [x] REST endpoints: `GET /api/achievements`, `GET /api/achievements/:playerId`, `GET /api/stats/:displayName`, `GET /api/leaderboard`
- [x] Desktop Profile page (`/profile`): name lookup, stats cards, top-games list, achievement grid with progress bar, category filter tabs, global leaderboard
- [x] Desktop Home page: "🏆 Top Players" leaderboard teaser widget (top 5 by sessions)
- [x] Stats service (`stats-service.ts`): typed fetch wrappers + `formatDuration` helper
- [x] Achievement unit tests (21 tests)
- [x] Player stats unit tests (13 tests)
- [x] `docs/status/phase-9-achievements.md` — phase status and known limitations
- [x] SQLite-backed achievement persistence (follow `SqliteSessionHistory` pattern)
- [x] Achievement unlock WebSocket push (`achievement-unlocked` server message + toast in UI)
- [x] `POST /api/achievements/:playerId/refresh` — re-evaluate achievements against live history on demand

## Phase 10 — Tournaments & Clip Sharing

**Goal:** Add competitive structure through bracket-style tournaments and let players capture / share short session clips.

### Milestones
- [x] Tournament engine: `TournamentStore` — bracket creation (single-elimination), match scheduling, result recording
- [x] REST endpoints: `POST /api/tournaments`, `GET /api/tournaments`, `GET /api/tournaments/:id`, `POST /api/tournaments/:id/matches/:matchId/result`
- [x] WebSocket push: `tournament-updated` message broadcast to all connected clients on bracket change
- [x] Desktop Tournament page (`/tournaments`): create bracket, visualise single-elimination draw, report results, view standings
- [x] Game clip recorder hook: client-side `MediaRecorder` API, IndexedDB storage (`clip-service.ts`)
- [x] Clip viewer: list saved clips per game, play back in-app (via blob URL), delete
- [x] Clip sharing: export clip as `.webm` / copy shareable link (local blob URL)
- [x] Home page “Recent Clips” widget: last 3 clips with game label and duration
- [x] Tournament winner achievements (`First-Blood`, `Champion`, `Dynasty`) — 3 new defs in `achievement-store.ts`
- [x] docs/status/phase-10-tournaments.md — phase status and feature notes

## Phase 11 — Clip Library, Social Sharing & Live Presence

**Goal:** Build a full in-app clip library with playback, management and social features; upgrade presence to use live server-side data rather than mock/demo data.

### Milestones
- [x] Clip library page (`/clips`): grid of all recorded clips, grouped by game, with play/delete/export/share actions
- [x] In-page clip player: `<video>` element loaded from IndexedDB blob URL, playback controls, fullscreen
- [x] Clip thumbnail generation: capture a still frame from the recorded blob at 50% duration via `<video>` + canvas
- [x] Clip tagging: attach a tournament match ID or free-text label to a clip
- [x] Live friend presence: connect `PresenceContext` to the lobby WebSocket `presence-update` feed instead of mock data
- [x] Live friend request UI: send/receive/accept/decline from the Friends page wired to Phase 8 WebSocket events
- [x] Notification badge: unread friend requests count shown on Friends nav item
- [x] Tournament SQLite persistence: `SqliteTournamentStore` following the `SqliteSessionHistory` pattern
- [x] Tournament history in Profile page: tournaments entered, wins, runner-up finishes
- [x] docs/status/phase-11-clips-presence.md — phase status and feature notes

## Phase 12 — WFC Auto-Config, Pokémon Lobbies & Mario Kart Matchmaking

**Goal:** Make Nintendo DS online play zero-friction — players go from "I have a ROM" to "I'm playing online" with one click, no DNS setup required.

### Phase 12a — WFC Auto-Config
- [x] `WFC_PROVIDERS` constant (`wfc-config.ts`): Wiimmfi (178.62.43.212) + AltWFC (172.104.88.237) definitions with description, URL, and stable ID
- [x] `GET /api/wfc/providers` — list all WFC providers; `GET /api/wfc/providers/:id` — single provider lookup
- [x] `wfcConfig.providerId` field added to `SessionTemplateConfig` — all existing Pokémon/MK DS templates carry `providerId: 'wiimmfi'`
- [x] Three Mario Kart DS session templates: `nds-mario-kart-ds-4p` (public), `nds-mario-kart-ds-quick-2p` (quick), `nds-mario-kart-ds-ranked-4p` (ranked)
- [x] `kartMode` field on `SessionTemplateConfig` (`'quick' | 'public' | 'ranked'`) tags MK rooms for UI bucketing

### Phase 12b — Pokémon Online Page (`/pokemon`)
- [x] `FriendCodeStore` — in-memory DS friend code registry (12-digit, per player per game)
- [x] `validateFriendCode()` — format validation helper (XXXX-XXXX-XXXX)
- [x] REST: `GET /api/friend-codes?gameId=<id>`, `GET /api/friend-codes?player=<name>`, `POST /api/friend-codes`, `DELETE /api/friend-codes?player=<n>&gameId=<g>`
- [x] `PokemonPage` (`/pokemon`): three-tab UI — Trade Lobby, Battle Lobby, Friend Codes
  - Trade Lobby: WFC auto-config banner, Wiimmfi/AltWFC provider switcher, host-a-trade-room flow, live open-rooms list
  - Battle Lobby: battle-room host flow, live open-rooms list
  - Friend Codes: register/display/copy your 12-digit DS codes per game; community codes directory
- [x] Pokémon nav item added to sidebar (🔴 Pokémon)

### Phase 12c — Mario Kart DS Matchmaking Page (`/mario-kart`)
- [x] `MarioKartPage` (`/mario-kart`): three-mode UI — Quick Race, Public Rooms, Ranked Races
  - Quick Race: one-click match via existing open rooms or auto-host + matchmaking queue join
  - Public Rooms: browse/join all open MK DS rooms; host public race
  - Ranked Races: join 4-player ranked queue, leaderboard from session history
- [x] Mario Kart nav item added to sidebar (🏎️ Mario Kart)
- [x] WFC auto-config badge in MK page explains zero-setup DNS
