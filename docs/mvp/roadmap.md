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
> - Phase 13 Seasonal Events, Featured Games & Custom Room Themes are now complete. REST: `GET /api/events`, `GET /api/events/current`, `GET /api/games/featured`. Events page (`/events`), home page seasonal banner + featured games widget, theme picker in HostRoomModal, themed lobby cards. Mario Sports page (`/mario-sports`) also added.
> - Phase 14 Zelda & Metroid Online + Direct Messaging are now complete. Zelda page (`/zelda` — Four Swords co-op, Phantom Hourglass battle mode), Metroid page (`/metroid` — Prime Hunters 4P deathmatch, quick match), in-app DMs between friends with WS push delivery, unread badge in nav.
> - Phase 15 Community Hub, Game Ratings & Ranked Play are now complete. Game reviews/ratings with 1-5 stars + text, top-rated games, per-game summaries. Community activity feed (session-started, achievement-unlocked, tournament-won, review-submitted, friend-added). ELO-based ranked matchmaking (casual/ranked room toggle, global + per-game leaderboards, Bronze→Diamond tiers). Community Hub page (`/community` — Activity Feed, Game Ratings, Rankings tabs). Player rank badge in Profile page. SQLite-backed stores for reviews and rankings.
> - Phase 18 GameCube (Dolphin) & Nintendo 3DS (Citra/Lime3DS) support are now complete. 10 GC games (MK:DD, Melee, MP4-7, F-Zero GX, Luigi's Mansion, Pikmin 2), 10 3DS games (MK7, SSB4-3DS, Pokémon X/Y/OR/AS/Sun/Moon, AC:NL, MH4U). 22 new templates. Dolphin backend, Citra backend. 235 tests total.
> - Phase 19 Nintendo Wii (Dolphin) support is now complete: `wii` system type, Wii adapter with MotionPlus flag, 10 Wii game templates, 9 Wii mock games, `WiiPage` (`/wii`), 🐬 Wii nav item. 270 tests total.
> - Phase 20 Debug & Polish is now complete: TypeScript `rootDir` fix (phase-18/19 tests now type-check cleanly), duplicate `n64-mario-party-2` mock-game entry removed, try/catch error isolation added to `send-dm`/`mark-dm-read`/`send-global-chat` handlers, `GameCubePage` (`/gc`) added with 9 GC games + Leaderboard tab, 🟣 GameCube nav item. 382 tests total.
> - Phase 21 SEGA Genesis / Mega Drive is now complete: `genesis` system type, RetroArch + Genesis Plus GX adapter, 10 Genesis session templates, 9 Genesis mock games, `GenesisPage` (`/genesis`), 🔵 Genesis nav item. 427 tests total.
> - Phase 23 Retro Achievement Support is now complete: `RetroAchievementStore` with 40 per-game achievements across 10 games, 6 REST endpoints (`/api/retro-achievements/*`), `RetroAchievementsPage` (`/retro-achievements`) with summary bar + game sidebar + badge grid, `🏅 Retro Achievements` nav item. 48 new tests (660 total).
 - Phase 24 Further Retro Achievement Support is now complete: catalog expanded to 60 achievements across 15 games (added GC/Wii/3DS/DC/PS2), `SqliteRetroAchievementStore` with Phase 24 DB migration, `getLeaderboard()` + `GET /api/retro-achievements/leaderboard`, `retro-achievement-unlocked` WS push on unlock, Leaderboard tab in `RetroAchievementsPage`. 22 new tests.

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


## Phase 15 — Community Hub, Game Ratings & Ranked Play

**Goal:** Give RetroOasis a community layer — players can rate games, see what others are doing in real time, and compete for ELO rankings in ranked rooms.

### Milestones
- [x] `GameRatingsStore` + `SqliteGameRatingsStore` — 1-5 star reviews, per-player upsert, delete, top-rated games, per-game summary
- [x] REST: `GET /api/reviews/top`, `GET /api/reviews/:gameId`, `GET /api/reviews/:gameId/summary`, `POST /api/reviews/:gameId`, `DELETE /api/reviews/:reviewId`, `GET /api/reviews/player/:playerId`
- [x] `ActivityFeedStore` — ring-buffer (200 events), records: session-started/ended, achievement-unlocked, tournament-created/won, review-submitted, friend-added
- [x] REST: `GET /api/activity` with `?type=`, `?player=`, `?limit=` filters
- [x] `RankingStore` + `SqliteRankingStore` — ELO (K=32, start=1000), global + per-game, tiers: Bronze/Silver/Gold/Platinum/Diamond
- [x] REST: `GET /api/rankings`, `GET /api/rankings/:gameId`, `GET /api/rankings/player/:playerId`, `GET /api/rankings/player/:playerId/:gameId`, `POST /api/rankings/match`
- [x] `rankMode: 'casual' | 'ranked'` added to `Room`, `CreateRoomPayload`, `LobbyManager`, `SqliteLobbyManager`, `handler.ts`
- [x] Phase 15 SQLite migrations: `game_reviews`, `global_rankings`, `game_rankings` tables; `rank_mode` column on `rooms`
- [x] `CommunityPage` (`/community`) — Activity Feed, Game Ratings, Rankings tabs
- [x] Community Hub nav item added to sidebar (🌐 Community)
- [x] Player rank badge in Profile page — tier icon, ELO, W/L, earned title
- [x] `community-service.ts` — typed fetch wrappers + `TIER_COLORS`, `TIER_ICONS`, `playerTitle()` helper
- [x] Ranked/Casual mode toggle in `HostRoomModal` (affects ELO on session end)
- [x] Phase 15 unit tests (39 tests across GameRatingsStore, ActivityFeedStore, RankingStore + ELO helpers)
- [x] `docs/mvp/roadmap.md` updated with Phase 15 milestones

## Phase 16 — Desktop Runtime & Core Integration

**Goal:** Convert RetroOasis from a browser-first prototype into a native desktop runtime with tighter emulator integration and less manual setup.

### Milestones
- [x] Tauri desktop shell integration for shipping native builds (Phase 1 carry-over)
- [x] Native ROM discovery wired into desktop settings/library (replace simulated refresh-only flow)
- [x] Per-game ROM selection persisted and used by launch flow
- [x] melonDS core IPC bridge path documented + initial integration spike in desktop launch pipeline
- [x] Network/runtime config hardening: environment-driven lobby URL + relay host defaults for non-local deployments
- [x] End-to-end launch validation for one title per system family (NES/SNES, GB/GBC/GBA, N64, NDS) with docs/status report

## Phase 17 — ROM Intelligence, Global Chat & Notification Center

**Goal:** Add Levenshtein-based fuzzy ROM title matching to improve auto-association accuracy, create a global lobby chat channel, and build an in-app notification center.

### Milestones
- [x] `rom-fuzzy-match.ts` — `fuzzyMatchGameId()` with Levenshtein edit distance, normalisation, 40 % confidence threshold
- [x] `LibraryPage` updated to use `fuzzyMatchGameId` against the full game catalog instead of the slug heuristic
- [x] `GlobalChatStore` — 500-message ring buffer with `post`, `getRecent`, `clear`
- [x] WS: `send-global-chat` → broadcast `global-chat-message` to all connected clients
- [x] REST: `GET /api/chat?limit=N` — fetch recent global chat messages
- [x] `GlobalChatPage` (`/chat`) — live global chat UI with WS subscription and Send input
- [x] `NotificationStore` — per-player notification store with `add`, `list`, `markRead`, `markAllRead`, `unreadCount`
- [x] Notifications auto-generated for: achievement unlocked, friend request received, DM received
- [x] REST: `GET /api/notifications/:playerId`, `POST /api/notifications/:playerId/read`, `POST /api/notifications/:playerId/read/:notifId`
- [x] `NotificationsPage` (`/notifications`) — groups unread/read, mark-all-read button, type icons
- [x] Layout sidebar: 💬 Chat and 🔔 Notifications nav items; unread badge on Notifications
- [x] 18 unit tests in `phase-17.test.ts` (GlobalChatStore + NotificationStore)
- [x] `docs/status/phase-17-rom-intelligence.md` — phase status doc
- [x] `roadmap.md` updated with Phase 17 milestones

## Phase 18 — GameCube & Nintendo 3DS Emulation

**Goal:** Extend RetroOasis to support GameCube (via Dolphin) and Nintendo 3DS (via Citra/Lime3DS) with relay-based online netplay, game-specific session templates, and dedicated mock catalog entries for both systems.

### Milestones
- [x] `gc` and `3ds` added to `SupportedSystem` type and `SYSTEM_INFO`
- [x] `dolphin` backend added (GC support, relay-only netplay, batch launch `-b -e`)
- [x] `citra` backend added (3DS support, relay + `--multiplayer-server/--port`)
- [x] `RetroArch` backend updated to include `gc` and `3ds` systems
- [x] GC system adapter: Dolphin `-b -e <rom>`, `--no-gui`, `--debugger` flags
- [x] 3DS system adapter: `citra <rom>`, `--fullscreen`, `--multiplayer-server/--port` flags
- [x] 10 GC session templates (default 2P/4P, MK:DD, Melee, MP4/5/6/7, F-Zero GX, Luigi's Mansion, Pikmin 2)
- [x] 10 3DS session templates (default 2P/4P, MK7 8P, SSB4-3DS 4P, Pokémon X/Y/OR/AS/Sun/Moon, AC:NL, MH4U)
- [x] 9 GC mock games + 10 3DS mock games in desktop catalog
- [x] `--gradient-gc` and `--gradient-3ds` CSS variables added
- [x] 22 new session templates; `phase-18.test.ts` — 22 tests
- [x] `roadmap.md` updated with Phase 18 milestones

## Phase 19 — Nintendo Wii (Dolphin)

**Goal:** Extend Dolphin emulator integration to cover the Nintendo Wii, adding Wii-specific adapter flags (MotionPlus emulation), 10 Wii game templates, 9 mock catalog entries, and a dedicated Wii lobby page.

### Milestones
- [x] `wii` added to `SupportedSystem` type and `SYSTEM_INFO` (generation 7, 4 local players)
- [x] `dolphin` backend updated to cover both `gc` and `wii` systems
- [x] `RetroArch` backend updated to include `wii`
- [x] Wii system adapter: Dolphin `-b -e <rom>`, `--no-gui`, `--wii-motionplus`, `--debugger` flags
- [x] `wiiMotionPlus?: boolean` added to `AdapterOptions` (Wii Sports Resort, etc.)
- [x] 10 Wii session templates: `wii-default-2p/4p`, `wii-mario-kart-wii-4p`, `wii-super-smash-bros-brawl-4p`, `wii-new-super-mario-bros-wii-4p`, `wii-wii-sports-4p`, `wii-wii-sports-resort-4p`, `wii-mario-party-8/9-4p`, `wii-donkey-kong-country-returns-2p`, `wii-kirby-return-to-dream-land-4p`
- [x] 9 Wii mock games (MKWii, Brawl, NSMBW, Wii Sports, Wii Sports Resort, MP8, MP9, DKC Returns, Kirby RTD)
- [x] `--gradient-wii` CSS variable added (`#c0c0c0 → #606060`)
- [x] `WiiPage` (`/wii`) — Lobby tab (game grid with Quick Match / Host Room) + Leaderboard tab
- [x] 🐬 Wii nav item added to sidebar in `Layout.tsx`
- [x] `/wii` route added in `App.tsx`
- [x] 35 unit tests in `phase-19.test.ts` (system type, Dolphin backend, adapter launch args, session templates, mock catalog)
- [x] `roadmap.md` updated with Phase 19 milestones

## Phase 20 — Debug & Polish

**Goal:** Fix accumulated bugs, eliminate type-check failures, harden WebSocket error handling, and surface the GameCube library that was added in Phase 18 with a dedicated lobby page.

### Milestones
- [x] `rootDir` removed from `apps/lobby-server/tsconfig.json` — phase-18 and phase-19 test files that import cross-package symbols now type-check cleanly
- [x] Duplicate `n64-mario-party-2` entry removed from `apps/desktop/src/data/mock-games.ts`
- [x] try/catch error isolation added to `send-dm`, `mark-dm-read`, and `send-global-chat` cases in `handler.ts` — store failures now return a graceful error response instead of crashing the WS handler
- [x] `GameCubePage` (`/gc`) added — 9 GC games (MK:DD, Melee, Mario Party 4-7, F-Zero GX, Luigi's Mansion, Pikmin 2), Quick Match / Host Room per card, Leaderboard tab, purple GC colour scheme
- [x] 🟣 GameCube nav item added to sidebar in `Layout.tsx`
- [x] `/gc` route added in `App.tsx`
- [x] 41 unit tests in `phase-20.test.ts` (duplicate-ID regression, GC system type, Dolphin backend, adapter launch args, session templates, mock catalog, NotificationStore API, GlobalChatStore ring-buffer)
- [x] `roadmap.md` updated with Phase 20 milestones

## Phase 21 — SEGA Genesis / Mega Drive

**Goal:** Extend RetroArch integration to cover the SEGA Genesis / Mega Drive — RetroOasis's first non-Nintendo platform — adding the Genesis Plus GX adapter, 10 Genesis session templates, 9 mock catalog entries, and a dedicated Genesis lobby page.

### Milestones
- [x] `genesis` added to `SupportedSystem` type and `SYSTEM_INFO` (generation 4, 2 local players, SEGA blue `#0066CC`)
- [x] `retroarch` backend updated to include `genesis` in its systems list; Genesis Plus GX core noted in backend notes
- [x] Genesis system adapter: RetroArch with Genesis Plus GX core, standard `--host`/`--connect` relay flags, saves to `genesis/` subdirectory
- [x] 10 Genesis session templates: `genesis-default-2p`, `genesis-sonic-the-hedgehog-2-2p`, `genesis-streets-of-rage-2-2p`, `genesis-mortal-kombat-3-2p`, `genesis-nba-jam-2p`, `genesis-contra-hard-corps-2p`, `genesis-gunstar-heroes-2p`, `genesis-golden-axe-2p`, `genesis-toejam-and-earl-2p`, `genesis-earthworm-jim-2-2p`
- [x] 9 Genesis mock games: Sonic 2, Streets of Rage 2, Mortal Kombat 3, NBA Jam, Contra: Hard Corps, Gunstar Heroes, Golden Axe, ToeJam & Earl, Earthworm Jim 2
- [x] `--gradient-genesis` CSS variable added (`#0066cc → #003366`)
- [x] `GenesisPage` (`/genesis`) — Game grid with Quick Match / Host Room per card, Genesis Plus GX info banner, Leaderboard tab, SEGA blue colour scheme
- [x] 🔵 Genesis nav item added to sidebar in `Layout.tsx`
- [x] `/genesis` route added in `App.tsx`
- [x] 45 unit tests in `phase-21.test.ts` (system type, RetroArch backend, adapter launch args, session templates, mock catalog, no-duplicate regression)
- [x] `roadmap.md` updated with Phase 21 milestones

## Phase 23 — Retro Achievement Support

**Goal:** Add per-game retro achievement support inspired by RetroAchievements.org. Each supported game has a curated set of in-game milestones that players can unlock during netplay sessions.

### Phase 23a — RetroAchievementStore
- [x] `RetroAchievementStore` — in-memory store tracking per-game achievement definitions and per-player progress
- [x] 40 achievement definitions across 10 games (4 per game): Mario Kart 64, Super Smash Bros. (N64), Mario Kart DS, Pokémon Diamond, Pokémon Emerald (GBA), Tetris (GB), Contra (NES), Sonic the Hedgehog 2 (Genesis), Crash Bandicoot (PSX), Super Bomberman (SNES)
- [x] `unlock(playerId, achievementId, sessionId?)` — idempotent; returns def on first unlock or null if already earned / unknown
- [x] `unlockMany(playerId, achievementIds[], sessionId?)` — batch unlock helper
- [x] `getProgress(playerId)` — returns (or creates) `RetroPlayerProgress` with `earned[]` and `totalPoints`
- [x] `getEarned(playerId, gameId?)` — earned achievements, optionally filtered to one game
- [x] `getGameSummaries()` — per-game achievement count + total points

### Phase 23b — REST API
- [x] `GET  /api/retro-achievements` — all 40 definitions
- [x] `GET  /api/retro-achievements/games` — per-game summaries
- [x] `GET  /api/retro-achievements/games/:gameId` — definitions for a specific game (404 if unknown)
- [x] `GET  /api/retro-achievements/player/:playerId` — player progress across all games
- [x] `GET  /api/retro-achievements/player/:playerId/game/:gameId` — per-game progress
- [x] `POST /api/retro-achievements/player/:playerId/game/:gameId/unlock` — unlock achievement; body `{ achievementId, sessionId? }`

### Phase 23c — Frontend
- [x] `retro-achievement-service.ts` — typed fetch helpers (`fetchRetroAchievementDefs`, `fetchRetroPlayerProgress`, `unlockRetroAchievement`, etc.)
- [x] `RetroAchievementsPage` (`/retro-achievements`) — summary bar (earned/available, points, progress bar), game-list sidebar, achievement grid with locked/unlocked badges
- [x] `🏅 Retro Achievements` nav item added to sidebar (More group)
- [x] `/retro-achievements` route added in `App.tsx`

### Phase 23d — Tests & Docs
- [x] 48 unit tests in `phase-23.test.ts` covering all store methods, catalog integrity, cross-player isolation, edge cases
- [x] `docs/status/phase-23-retro-achievements.md` — phase status and achievement catalog
- [x] `roadmap.md` updated with Phase 23 milestones

## Phase 24 — Further Retro Achievement Support

**Goal:** Expand retro achievement coverage to newer systems added in Phases 18–22, add SQLite persistence, a leaderboard endpoint, WebSocket push for unlocks, and a Leaderboard tab in the frontend.

### Phase 24a — Expanded Achievement Catalog
- [x] 20 new achievement definitions across 5 games (4 each):
  - `gc-mario-kart-double-dash` — Mario Kart: Double Dash!! (GameCube): First Double Dash, Perfect Partnership, Grand Prix Master, LAN Party Champion
  - `wii-mario-kart-wii` — Mario Kart Wii: First Race Online, Big Air, Star Ranked, Worldwide Winner
  - `3ds-mario-kart-7` — Mario Kart 7: First Race, Hang Glider, Community Racer, Perfect Cup
  - `dc-sonic-adventure-2` — Sonic Adventure 2 (Dreamcast): City Escape, A-Rank Hero, Chao Keeper, Space Colony ARK
  - `ps2-gta-san-andreas` — GTA: San Andreas (PS2): Welcome to San Andreas, High Roller, Grove Street 4 Life, 100% Complete
- [x] Catalog now totals 60 definitions across 15 games

### Phase 24b — SQLite Persistence
- [x] `SqliteRetroAchievementStore` — SQLite-backed store; same public API as `RetroAchievementStore`
- [x] Phase 24 DB migration: `retro_achievement_progress` table with `PRIMARY KEY (player_id, achievement_id)` and two indexes
- [x] `index.ts` wires `SqliteRetroAchievementStore` when `DB_PATH` is set

### Phase 24c — Leaderboard & WS Push
- [x] `getLeaderboard(limit?)` — top-N players sorted by `totalPoints` (tie-break: earnedCount then playerId)
- [x] `GET /api/retro-achievements/leaderboard?limit=<n>` — new REST endpoint (max 100, default 10)
- [x] `pushRetroAchievementUnlocked()` exported from `handler.ts` — sends `retro-achievement-unlocked` WS message to the player's connection and creates a notification on unlock
- [x] `retro-achievement-unlocked` WS message type added to `ServerMessage` union

### Phase 24d — Frontend
- [x] `RetroLeaderboardEntry` type + `fetchRetroLeaderboard()` added to `retro-achievement-service.ts`
- [x] `gameIdToTitle` updated with 5 new game titles (GC, Wii, 3DS, DC, PS2)
- [x] `RetroAchievementsPage` gains Achievements / Leaderboard tab bar; Leaderboard tab shows ranked table with gold/silver/bronze medals and "You" badge for the current player

### Phase 24e — Tests & Docs
- [x] 22 unit tests in `phase-24.test.ts` — expanded catalog integrity, `getLeaderboard()` sorting + limit, cross-player isolation, new Phase 24 achievement IDs
- [x] Phase 23 tests updated to use `toBeGreaterThanOrEqual` instead of exact counts (catalog is now 60 defs / 15 games)
- [x] `roadmap.md` updated with Phase 24 milestones

## Phase 25 — More Retro Achievements

**Goal:** Expand retro achievement coverage to additional systems and classic titles supported by RetroAchievements.org, growing the catalog from 60 → 80 definitions across 20 games.

### Phase 25a — Expanded Achievement Catalog
- [x] 20 new achievement definitions across 5 games (4 each):
  - `gbc-pokemon-crystal` — Pokémon Crystal (GBC): First Badge, Link Trade, Legendary Chase, Johto Champion
  - `psp-monster-hunter-fu` — Monster Hunter Freedom Unite (PSP): First Hunt, Hunting Party, Rathalos Slayer, G-Rank Hunter
  - `psx-castlevania-sotn` — Castlevania: Symphony of the Night (PSX): Dracula's Castle, Familiar Bond, Inverted Castle, True Ending
  - `snes-secret-of-mana` — Secret of Mana (SNES): First Weapon, Multiplayer Adventure, Master of Arms, Mana Beast
  - `psx-metal-gear-solid` — Metal Gear Solid (PSX): Sneaking Mission, Mind Reader, Pacifist, Big Boss Rank
- [x] `gameIdToTitle` updated with 5 new game titles

### Phase 25b — Tests & Docs
- [x] Phase 24 catalog count tests updated to `toBeGreaterThanOrEqual` (catalog now 80 defs / 20 games)
- [x] 20 unit tests in `phase-25.test.ts` — catalog integrity, new-game coverage, store behaviour, leaderboard
- [x] `roadmap.md` updated with Phase 25 milestones

## Phase 26 — Debug, Audit & Polish

**Goal:** Fix latent bugs discovered during audit of Phases 15–25, harden REST error handling, and polish the UI.

### Phase 26a — Bug Fixes

- [x] `activity-feed.ts` — Ring buffer eviction: replaced `O(n)` `slice(0, MAX_EVENTS)` allocation with `O(1)` `pop()` call; oldest entry is removed without re-allocating the full array
- [x] `ranking-store.ts` — Added explicit comment clarifying per-game draw semantics: `gamesPlayed` increments on draws; no separate wins/losses counter is mutated (consistent with `GameRank` interface and `game_rankings` schema)
- [x] `handler.ts` — `displayNameToPlayerId` cleanup on disconnect: removed `break` so **all** stale entries for the disconnected `playerId` are deleted (prevents ghost entries / memory leak)
- [x] `handler.ts` — `send-dm` handler: added 2 000-character maximum length guard; oversized messages return `{ type: 'error', message: 'Message too long (max 2000 characters)' }` instead of being stored
- [x] `index.ts` — `POST /api/messages/send`: wrapped `JSON.parse` in `try/catch`; malformed body now returns `400 { error: 'Invalid JSON body' }` instead of an unhandled crash
- [x] `index.ts` — `POST /api/messages/read`: same JSON parse guard added
- [x] `index.ts` — `POST /api/rankings/match`: same JSON parse guard added

### Phase 26b — UI Polish

- [x] `RetroAchievementsPage.tsx` — Added `loadError` state + `retryKey` counter; if any fetch throws, an error message with a **Retry** button is shown instead of the spinner hanging forever

### Phase 26c — Tests

- [x] 14 new unit tests in `phase-26.test.ts` — ring buffer eviction, draw outcome (global + per-game), GlobalChatStore ordering, NotificationStore unread count
- [x] `roadmap.md` updated with Phase 26 milestones

## Phase 27 — Nintendo Wii U Support & README Multi-Platform Update

**Goal:** Add full Nintendo Wii U (Cemu) support and update the README to reflect that RetroOasis is a multi-platform retro gaming emulator — not just Nintendo — spanning the SMS era through the Wii U and 3DS.

### Phase 27a — Wii U System

- [x] `wiiu` added to `SupportedSystem` type in `packages/shared-types/src/systems.ts`
- [x] `SYSTEM_INFO['wiiu']` — generation 8, 5 max local players, `#009AC7` color
- [x] `cemu` backend added to `KNOWN_BACKENDS` in `packages/emulator-bridge/src/backends.ts`
- [x] RetroArch `systems` list updated to include `wiiu`
- [x] `wiiu` adapter added to `createSystemAdapter()` — Cemu `-g <rom>` launch args, relay-based netplay, RetroArch fallback
- [x] 10 session templates: `wiiu-default-2p/4p`, `wiiu-mario-kart-8-4p`, `wiiu-super-smash-bros-wiiu-4p/8p`, `wiiu-new-super-mario-bros-u-4p`, `wiiu-nintendo-land-4p`, `wiiu-splatoon-4p`, `wiiu-pikmin-3-2p`, `wiiu-mario-party-10-4p`
- [x] 9 mock games: Mario Kart 8, Super Smash Bros. for Wii U, New Super Mario Bros. U, Nintendo Land, Splatoon, Pikmin 3, Mario Party 10, DKC: Tropical Freeze, Bayonetta 2

### Phase 27b — Desktop UI

- [x] `WiiUPage.tsx` — lobby + leaderboard tabs, game grid with Quick Match / Host Room, asymmetric/co-op badges
- [x] `/wiiu` route added to `App.tsx`
- [x] `Wii U` nav item (⊞) added to `Layout.tsx` under Systems group

### Phase 27c — README

- [x] README tagline updated: "multiplayer-first retro gaming emulator platform — from the SMS to the Wii U"
- [x] Description updated to mention Nintendo, SEGA, and Sony platforms
- [x] Features list updated: mentions all 16 supported systems
- [x] Supported Systems table expanded: all 16 systems with Manufacturer and Generation columns

### Phase 27d — Tests

- [x] 40 new unit tests in `phase-27.test.ts` — system type, Cemu backend, adapter launch args, session templates, mock games, no-duplicate regression
- [x] `roadmap.md` updated with Phase 27 milestones


- Tournament-style rooms
- ~~Seasonal featured games~~ ✓ Phase 13
- ~~Ranked/casual matchmaking tags~~ ✓ Phase 15
- ~~Custom room themes~~ ✓ Phase 13
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

## Phase 13 — Seasonal Events, Featured Games & Custom Room Themes

**Goal:** Add a living community layer — weekly featured-game rotations, date-bounded seasonal events, and host-chosen room themes that give each lobby a visual identity.

### Milestones

#### Phase 13a — Seasonal Events
- [x] `seasonal-events.ts` — `SeasonalEvent` interface + 5 seeded events (Spring/Summer/Fall/Winter 2026, Spring 2027)
- [x] `getActiveEvents(date?)` — returns currently active events; `getNextEvent(date?)` — returns the next upcoming event
- [x] `REST GET /api/events` — all events + `nextEvent` pointer
- [x] `REST GET /api/events/current` — active events + `nextEvent`
- [x] `xpMultiplier` field on events (cosmetic multiplier shown in UI; stat weighting is future work)

#### Phase 13b — Weekly Featured Games
- [x] `featured-games.ts` — static pool of 20 curated entries (Racing, Fighting, Party, Pokémon, Puzzle, Adventure, Action)
- [x] `getFeaturedGames(date?)` — deterministic ISO-week rotation; 6 different games shown each week
- [x] `REST GET /api/games/featured` — current week's featured games
- [x] Home page **⭐ Featured This Week** widget — 6-column card grid, links to game detail pages

#### Phase 13c — Custom Room Themes
- [x] `theme?: string` added to `Room` and `CreateRoomPayload` types (server + desktop)
- [x] SQLite migration: `ALTER TABLE rooms ADD COLUMN theme TEXT` (idempotent, Phase 13 upgrade)
- [x] `LobbyManager` and `SqliteLobbyManager` `createRoom` accept optional `theme`
- [x] `handler.ts` `create-room` case passes `theme` from payload
- [x] 6 themes: `classic`, `spring`, `summer`, `fall`, `winter`, `neon`
- [x] `HostRoomModal` theme picker — 3-column grid of styled buttons; `classic` is the default (omitted from payload)
- [x] `LobbyPage` room card uses themed gradient background + accent border

#### Phase 13d — Client-Side Services & Pages
- [x] `events-service.ts` — typed fetch wrappers (`fetchAllEvents`, `fetchCurrentEvents`, `fetchFeaturedGames`)
- [x] Theme helpers: `themeGradient(theme)`, `themeAccent(theme)`, `themeLabel(theme)`, `ROOM_THEMES` constant
- [x] `EventsPage` (`/events`) — Featured This Week grid + all Seasonal Events with filter tabs (All / Active / Upcoming / Past), status badges, xp-multiplier tag, Room Themes explainer
- [x] Home page **Seasonal Event Banner** — appears when ≥ 1 event is active; themed gradient, emoji, description, stat multiplier badge, "Details →" link
- [x] `🎪 Events` nav item added to sidebar in `Layout.tsx`
- [x] `docs/status/phase-13-events.md` — phase status and known limitations
- [x] roadmap.md updated with Phase 13 milestones

## Phase 14 — Zelda & Metroid Online + Direct Messaging

**Goal:** Expand game-specific multiplayer hubs to Zelda and Metroid franchises, and add real-time direct messaging between friends so players can coordinate sessions without leaving the app.

### Phase 14a — Zelda Multiplayer Hub (`/zelda`)
- [x] `ZeldaPage` (`/zelda`) — Co-op Rooms, Battle Mode, Leaderboard tabs
  - Co-op Rooms: GBA Four Swords 4P/2P (online relay) + GBC Oracle Ages/Seasons 2P (link cable) lobby browser and host flow
  - Battle Mode: NDS Phantom Hourglass 2P (Wiimmfi/AltWFC) host/join + WFC auto-config banner
  - Leaderboard: top players by sessions (global)
- [x] `⚔️ Zelda` nav item added to sidebar

### Phase 14b — Metroid Hunters Lobby (`/metroid`)
- [x] `MetroidPage` (`/metroid`) — Online Matches, Quick Match, Rankings tabs
  - Online Matches: 4P deathmatch lobby with WFC zero-setup banner + provider switcher
  - Quick Match: one-click find-or-create a 1v1 match with Wiimmfi auto-config
  - Rankings: global leaderboard + playable hunters roster card (all 7 hunters)
- [x] `🌌 Metroid` nav item added to sidebar

### Phase 14c — Direct Messaging
- [x] `MessageStore` — in-memory DM store with `sendMessage`, `getConversation`, `getUnreadCount`, `markRead`, `getRecentConversations`
- [x] `SqliteMessageStore` — SQLite-backed DM store (follows `SqliteSessionHistory` pattern); `hydrate()` pre-loads rows
- [x] SQLite schema: `direct_messages` table + indexes; Phase 14 migration in `db.ts`
- [x] REST: `GET /api/messages/:player` (recent conversations), `GET /api/messages/:player1/:player2` (thread), `GET /api/messages/:player/unread-count`, `POST /api/messages/send`, `POST /api/messages/read`
- [x] WebSocket: `send-dm` (client→server), `dm-received` (server→client push to recipient), `mark-dm-read` / `dm-read-ack`
- [x] DM thread UI in `FriendsPage`: friend list opens a message thread with chat bubbles and a send input; incoming WS messages merged in real-time
- [x] Unread DM count tracked in `LobbyContext` (`unreadDmCount`, `incomingDms`)
- [x] 💬 DM badge on Friends nav item when unread messages arrive
- [x] Toast notification for incoming DMs

### Phase 14d — Game Catalog & Session Templates
- [x] `nds-metroid-prime-hunters` added to game catalog (`seed-data.ts`) — 4P online, Wiimmfi
- [x] `nds-zelda-phantom-hourglass` added to game catalog — 2P online, Wiimmfi  
- [x] `nds-zelda-spirit-tracks` added to game catalog — 2P local co-op
- [x] `nds-metroid-prime-hunters-2p` session template — quick 1v1 variant
- [x] `gba-zelda-four-swords-2p` session template — 2-player variant of Four Swords

### Phase 14e — Documentation
- [x] `docs/status/phase-14-zelda-metroid-dms.md` — phase status and feature notes
- [x] `roadmap.md` updated with Phase 14 milestones
