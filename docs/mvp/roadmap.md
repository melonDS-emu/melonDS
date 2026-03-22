# RetroOasis Development Roadmap

> **Honest progress snapshot (2026-03-12)**
>
> - The current working stack is **React + Vite desktop UI + WebSocket lobby server + TCP relay + local HTTP launch API (`/api/launch`)**.
> - **Tauri/native packaging, a C++ IPC bridge into the melonDS core in `/src`, and a broader REST backend for catalog/save/presence data are still not complete.**
> - The lobby now triggers **local emulator launch requests** through the server, but **real per-game ROM discovery/selection and relay token handoff into emulator processes are still incomplete**.
> - Presence, activity, saves, and cloud flows have useful UI scaffolding, but the **live/server-backed versions remain future work unless explicitly marked “mock/dev/demo.”**
> - Phase 7 (Input & Couch-Friendliness) is now complete: `controller-detection.ts` (Gamepad API wrapper — hot-plug events, `detectGamepadType`, `getConnectedGamepads`), `slot-assignment.ts` (per-player gamepad ↔ slot mapping, `autoAssignSlots`, localStorage persistence), `preflight-validator.ts` (`validateInputConfig` — errors for missing/disconnected gamepads, warning for keyboard sharing). Default input profiles added for 7 new systems: Wii, Wii U, Genesis, Dreamcast, PSX, PS2, PSP (21 new profiles × 3 controller types). `InputProfileManager` now loads all 16 systems (48 default profiles). 32 new tests (172 desktop total).
> - Phase 6 (Social) Presence, Invites & Social Glue are now complete: `RecentPlayersStore` + `SqliteRecentPlayersStore` (track co-players, max 50 per user), `PlayerPrivacyStore` + `SqlitePlayerPrivacyStore` (showOnline/allowInvites/showActivity toggles), WS `send-invite` → `invite-received` with privacy guard, WS `set-privacy` → `privacy-updated`, REST `GET/DELETE /api/players/:id/recent`, REST `GET/PUT /api/players/:id/privacy`, recent-players tracked on game-start, FriendsPage updated with Incoming Invites, Recent Players, Invite buttons, and Privacy Settings panel. 23 new tests (790 total).
> - Phase 8 SQLite persistence, friend system, matchmaking queue, player identity, and enhanced session stats are now complete. Set `DB_PATH` env var to enable persistence across restarts.
> - Phase 8 System Expansion UX is now complete: `performancePreset` ('fast'/'balanced'/'accurate') and `compatibilityFlags` added to `AdapterOptions`; N64 Mupen64Plus maps preset to `--emumode 0/1/2`; 33 golden-list games added (NES×8, SNES×6, GB×5, GBC×3, GBA×7, N64×4); dedicated system pages `NESPage` (`/nes`), `SNESPage` (`/snes`), `GBAPage` (`/gba` — covers GB+GBC+GBA with system filter), `N64Page` (`/n64`), `NDSPage` (`/nds`) — each with lobby grid, quick match, leaderboard tab, and system-specific emulator banner. 82 new tests (790 total).
> - Phase 9 achievements (20 definitions + 3 tournament achievements), player stats aggregation, global leaderboard, Profile page, SQLite achievement persistence, WebSocket achievement push + desktop toast notifications, and the `/api/achievements/:playerId/refresh` endpoint are now complete.
> - Phase 10 tournaments (single-elimination bracket engine, REST + WS push, desktop Tournament page, winner achievements) and clip sharing (MediaRecorder service, IndexedDB storage, export, home page widget) are now complete.
> - Phase 11 clip library (`/clips`), live friend presence, live friend request UI, notification badges, `SqliteTournamentStore`, and tournament history in Profile page are now complete.
> - Phase 11 (Cloud) cloud & account features are now complete: `AccountStore` (email/password, scrypt hashing, session tokens, profile management, identity linking), `ModerationStore` (5-category reports, auto-flag threshold, review/dismiss), 11 REST endpoints for accounts + moderation, SQLite schema migrations. 49 new tests (1053 total).
> - Phase 12 WFC auto-config (Wiimmfi + AltWFC, `GET /api/wfc/providers`), Pokémon Online page (`/pokemon` — Trade Lobby, Battle Lobby, Friend Codes), and Mario Kart DS matchmaking page (`/mario-kart` — Quick Race, Public Rooms, Ranked Races) are now complete.
> - Phase 13 Seasonal Events, Featured Games & Custom Room Themes are now complete. REST: `GET /api/events`, `GET /api/events/current`, `GET /api/games/featured`. Events page (`/events`), home page seasonal banner + featured games widget, theme picker in HostRoomModal, themed lobby cards. Mario Sports page (`/mario-sports`) also added.
> - Phase 14 Zelda & Metroid Online + Direct Messaging are now complete. Zelda page (`/zelda` — Four Swords co-op, Phantom Hourglass battle mode), Metroid page (`/metroid` — Prime Hunters 4P deathmatch, quick match), in-app DMs between friends with WS push delivery, unread badge in nav.
> - Phase 15 Community Hub, Game Ratings & Ranked Play are now complete. Game reviews/ratings with 1-5 stars + text, top-rated games, per-game summaries. Community activity feed (session-started, achievement-unlocked, tournament-won, review-submitted, friend-added). ELO-based ranked matchmaking (casual/ranked room toggle, global + per-game leaderboards, Bronze→Diamond tiers). Community Hub page (`/community` — Activity Feed, Game Ratings, Rankings tabs). Player rank badge in Profile page. SQLite-backed stores for reviews and rankings.
> - Phase 18 GameCube (Dolphin) & Nintendo 3DS (Citra/Lime3DS) support are now complete. 10 GC games (MK:DD, Melee, MP4-7, F-Zero GX, Luigi's Mansion, Pikmin 2), 10 3DS games (MK7, SSB4-3DS, Pokémon X/Y/OR/AS/Sun/Moon, AC:NL, MH4U). 22 new templates. Dolphin backend, Citra backend. 235 tests total.
> - Phase 19 Nintendo Wii (Dolphin) support is now complete: `wii` system type, Wii adapter with MotionPlus flag, 10 Wii game templates, 9 Wii mock games, `WiiPage` (`/wii`), 🐬 Wii nav item. 270 tests total.
> - Phase 20 Debug & Polish is now complete: TypeScript `rootDir` fix (phase-18/19 tests now type-check cleanly), duplicate `n64-mario-party-2` mock-game entry removed, try/catch error isolation added to `send-dm`/`mark-dm-read`/`send-global-chat` handlers, `GameCubePage` (`/gc`) added with 9 GC games + Leaderboard tab, 🟣 GameCube nav item. 382 tests total.
> - Phase 21 SEGA Genesis / Mega Drive is now complete: `genesis` system type, RetroArch + Genesis Plus GX adapter, 10 Genesis session templates, 9 Genesis mock games, `GenesisPage` (`/genesis`), 🔵 Genesis nav item. 427 tests total.
> - Phase 23 Retro Achievement Support is now complete: `RetroAchievementStore` with 40 per-game achievements across 10 games, 6 REST endpoints (`/api/retro-achievements/*`), `RetroAchievementsPage` (`/retro-achievements`) with summary bar + game sidebar + badge grid, `🏅 Retro Achievements` nav item. 48 new tests (660 total).
> - Phase 24 Further Retro Achievement Support is now complete: catalog expanded to 60 achievements across 15 games (added GC/Wii/3DS/DC/PS2), `SqliteRetroAchievementStore` with Phase 24 DB migration, `getLeaderboard()` + `GET /api/retro-achievements/leaderboard`, `retro-achievement-unlocked` WS push on unlock, Leaderboard tab in `RetroAchievementsPage`. 22 new tests.
> - Phase 3 Full Multiplayer Loop is now complete: host/join private+public rooms, room-code flow, ready states, host controls, spectator support, game-start handshake with per-player session tokens, relay allocation (ports 9000–9200), per-backend emulator launch args, host migration on disconnect, clean leave flow, and `rejoinRoom`/`rejoinByCode` for in-progress reconnects. 36 new tests (765 total).
> - Phase 4 Save System is now complete: `SaveBackupStore` + `SqliteSaveBackupStore` (pre-session/post-session/manual/last-known-good backups, per-slot eviction), 8 backup REST routes (`/api/saves/:gameId/backup`, `/api/saves/:gameId/backups`, restore, mark-lkg, last-known-good, session-start, session-end), `discoverSaveFiles()` / `getBackendSaveExtensions()` / `buildBackendSavePath()` / `inferBackendFromExtension()` in `@retro-oasis/save-system`, `save-backup-service.ts` desktop layer, `SavesPage` Backups tab with restore + last-known-good UI. 55 new tests (867 total).
> - Phase 32 Wiimmfi & WiiLink Online Services is now complete: `WfcProvider` interface gains `supportsWii` field; `WiiLink WFC` added as a third WFC provider (DNS `167.235.229.36`); Wiimmfi description updated to highlight its role as the largest WFC revival service since 2014; `WIILINK_CHANNELS` constant (6 active channel definitions — Forecast, News, Everybody Votes, Check Mii Out, Nintendo, Demae); `GET /api/wfc/wiilink-channels` REST endpoint; `OnlineServicesPage` (`/online-services`) with three-tab UI (Overview, Wiimmfi, WiiLink) covering provider switcher, channel list, and zero-setup DNS explainer; 🌐 Online Services nav item in sidebar. 39 new tests.

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

## Phase 3 — Full Multiplayer Loop (Complete)

**Goal:** Users can create rooms and actually get into a session. First get: Host room → friend joins → both ready → session starts → both can leave cleanly.

### Milestones
- [x] Host creates public or private room with auto-generated 6-character room code
- [x] Friend joins by room code (case-insensitive lookup)
- [x] Ready-state management: host defaults to ready, guests toggle with `toggle-ready`
- [x] `startGame` gated on all players being ready and caller being host
- [x] Relay port allocated from NetplayRelay pool (9000–9200) on game start
- [x] Per-player unique session tokens sent in `game-starting` event (used by emulator relay auth)
- [x] Per-backend emulator launch args wired via `createSystemAdapter` (FCEUX, Snes9x, mGBA, Mupen64Plus, melonDS, SameBoy, VBA-M, Dolphin, RetroArch, Citra)
- [x] Spectator join by room code (waiting and in-progress rooms)
- [x] Host migration: host role transfers to next player when host disconnects or leaves
- [x] Player slot renumbering after departure (contiguous 0, 1, 2, …)
- [x] Clean leave: `leaveRoom` / `disconnectPlayer` frees relay port and ends session history record when last player exits
- [x] `rejoinRoom(roomId, playerId, displayName)` — allows a new WebSocket connection to re-enter an in-progress session
- [x] `rejoinByCode(roomCode, playerId, displayName)` — same flow resolved by room code
- [x] `rejoin-room` WebSocket message + `room-rejoined` response with existing relay port and a fresh session token
- [x] 36 new unit tests covering the full multiplayer loop (phase-3.test.ts)

## Phase 3-NDS — Nintendo DS + Premium Features

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

## Phase 4 — Save System (Complete)

**Goal:** No corrupted progress, no mystery saves. A save system users can trust.

### Milestones
- [x] Standardized save discovery per backend (`discoverSaveFiles()` + `getBackendSaveExtensions()` in `@retro-oasis/save-system`)
- [x] `buildBackendSavePath()` — constructs correct save-file path for any emulator backend
- [x] `inferBackendFromExtension()` — maps save-file extensions back to their emulator backend
- [x] Local save registry — `SaveStore` (in-memory) + `SqliteSaveStore` (SQLite) with conflict detection and optimistic concurrency
- [x] Import/export — `importSaveFromFile()` and `exportSave()` in desktop `save-service.ts`
- [x] Pre-session backup — `POST /api/saves/:gameId/session-start` snapshots all save slots before a game launches
- [x] Post-session sync — `POST /api/saves/:gameId/session-end` uploads updated saves and marks last-known-good on clean exit
- [x] Save backup store — `SaveBackupStore` (in-memory) + `SqliteSaveBackupStore` (SQLite) with per-slot eviction (max 20)
- [x] `save_backups` table in SQLite schema
- [x] REST API: `POST /api/saves/:gameId/backup`, `GET /api/saves/:gameId/backups`, `DELETE /api/saves/:gameId/backups/:id`, `POST .../restore`, `POST .../mark-lkg`, `GET .../last-known-good`
- [x] Conflict resolution UI — `ConflictResolutionModal` with side-by-side local/cloud comparison
- [x] Restore from backup — Backups tab in `SavesPage` lists all snapshots; each can be restored with one click
- [x] "Last known good" recovery path — clean-exit sessions auto-mark the saved state as LKG; UI surfaces ⭐ badge and Restore button
- [x] Desktop `save-backup-service.ts` — typed wrapper around all backup REST endpoints
- [x] 55 new tests (867 total)

## Phase 5 — Curated Multiplayer UX

**Goal:** Make the app feel like a social console launcher where a normal user can understand how to play with friends in under 60 seconds.

**Current reality:** the discovery UI is in place, social features powered by seeded mock/dev data, and the curated UX polish is now complete.

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
- [x] **Curated multiplayer UX (Phase 5 polish pass):**
  - [x] `SystemBadge` reusable component with `SYSTEM_COLORS` map (all 16 systems)
  - [x] `session-presets.ts` — `getGamePresets()` generates quick-start presets (Full Party, 4-Player, 1-on-1) from game properties
  - [x] `HostRoomModal` — "Play with Friends" heading, ⚡ Quick Start preset picker per game, advanced options (theme, NDS layout) collapsed behind toggle
  - [x] `GameDetailsPage` — "🎮 Play with Friends" is the primary CTA (full-width, prominent), "Play Solo" and "Invite Friends" demoted to secondary row, ROM/emulator settings collapsed behind "⚙ Advanced Settings" toggle
  - [x] `LibraryPage` — "🤝 Best with Friends" spotlight toggle surfaces games with that badge; multiplayer mode filter replaced with icon-labelled MODES list (Party 🎉, Co-op 🤝, Versus ⚔️, Battle 💥, Link 🔗, Trade 🔄)
  - [x] `HomePage` — "🤝 Best with Friends" game grid section added; "🎮 Browse by Mode" quick-browse card row (Party / Co-op / Versus / Battle) with per-mode colour gradients
  - [x] 30 new tests (979 total)
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

## Phase 6 (Social) — Presence, Invites & Social Glue (Complete)

**Goal:** Make RetroOasis feel alive even before launching a game. Users can discover each other and jump into sessions without coordinating outside the app.

### Milestones
- [x] `recent-players-store.ts` — `RecentPlayersStore` + `SqliteRecentPlayersStore`; tracks up to 50 recent co-players per user, LRU eviction, deduplication by (owner, player, room) pair
- [x] `player-privacy-store.ts` — `PlayerPrivacyStore` + `SqlitePlayerPrivacyStore`; per-player `showOnline`, `allowInvites`, `showActivity` toggles; full defaults on first access
- [x] Handler: `send-invite` WS case — routes invite to recipient with `invite-received` push; privacy guard blocks non-friend invites when `allowInvites=false`; notification added via `NotificationStore`
- [x] Handler: `set-privacy` WS case — updates own privacy settings; confirms with `privacy-updated` message
- [x] Handler: recent-players recorded for every participant pair on game-start (`start-game` case)
- [x] `types.ts` — `send-invite`, `set-privacy` client message types; `invite-received`, `invite-sent`, `privacy-updated` server message types
- [x] `index.ts` — instantiate `RecentPlayersStore`/`PlayerPrivacyStore` (SQLite when `DB_PATH` set); wire to `handleConnection`
- [x] REST: `GET /api/players/:playerId/recent` — list recent players (limit 20)
- [x] REST: `DELETE /api/players/:playerId/recent` — clear all recent players
- [x] REST: `DELETE /api/players/:playerId/recent/:entryId` — remove one entry
- [x] REST: `GET /api/players/:playerId/privacy` — get privacy settings
- [x] REST: `PUT /api/players/:playerId/privacy` — update privacy settings (partial)
- [x] `LobbyContext.tsx` — `sendInvite`, `dismissInvite`, `incomingInvites`, `updatePrivacy`, `privacySettings`; handles `invite-received` and `privacy-updated` WS messages with toast
- [x] `lobby-types.ts` — `send-invite`, `set-privacy` client types; `invite-received`, `invite-sent`, `privacy-updated` server types
- [x] `FriendsPage.tsx` — Incoming Invites section (join/dismiss), Recent Players section (add friend shortcut), Invite button on friend cards when in room, Privacy Settings collapsible panel with three toggle switches
- [x] 23 new unit tests in `phase-6.test.ts` (`RecentPlayersStore` × 11, `PlayerPrivacyStore` × 8, privacy guard integration × 3, mutual-recording integration × 1)

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


## Phase 28 — Compatibility & Stability Push (Complete)

**Goal:** Make RetroOasis feel dependable, not experimental. Ship a compatibility badge pipeline, known issues tracking, session health diagnostics, and a smoke-test matrix covering the top-25 multiplayer games.

### Phase 28a — Server: Compatibility Pipeline

- [x] `compatibility-store.ts` — `CompatibilityStore` with `verified/playable/partial/broken/untested` statuses; pre-seeded with 25 top multiplayer games (NES/SNES/GB/GBC/GBA/N64/NDS/GC/Wii/PSX)
- [x] `known-issues-store.ts` — `KnownIssuesStore` CRUD (severity levels, `open/investigating/resolved` lifecycle); pre-seeded with 5 realistic known issues
- [x] `session-health-store.ts` — `SessionHealthStore` ring-buffer (max 1000 records); `getSummary()` aggregates avgLatency/crashRate/disconnects; `getSmokeTestMatrix()` returns 25-entry pass/fail matrix

### Phase 28b — Server: REST Endpoints

- [x] `GET /api/compatibility` — all game compatibility summaries
- [x] `GET /api/compatibility/system/:system` — summaries by system
- [x] `GET /api/compatibility/:gameId` — per-game summary with `bestStatus`
- [x] `POST /api/compatibility` — upsert a compatibility record
- [x] `GET /api/known-issues` — all open known issues
- [x] `GET /api/known-issues/:gameId` — issues for a specific game
- [x] `POST /api/known-issues` — report a new issue
- [x] `PUT /api/known-issues/:id/status` — update issue status
- [x] `GET /api/session-health` — all session health records
- [x] `GET /api/session-health/:roomId` — health for a specific room
- [x] `POST /api/session-health` — record health data for a session
- [x] `GET /api/smoke-tests` — return the 25-entry smoke-test matrix
- [x] `GET /api/sessions/export` — export full session history as JSON

### Phase 28c — Desktop UI

- [x] `compatibility-service.ts` — fetch wrapper for compatibility/known-issues/smoke-test endpoints; `COMPAT_BADGE` display helpers
- [x] `CompatibilityPage.tsx` at `/compatibility` — three-tab page: Compatibility Badges (system filter + per-game badge), Known Issues (severity filter + lifecycle status), Smoke Tests (pass% summary + matrix)
- [x] `🛡️ Compatibility` nav item added to Layout under "More" group

### Phase 28d — Tests

- [x] 32 new unit tests in `phase-28.test.ts` — CompatibilityStore seed/CRUD/summary, KnownIssuesStore seed/report/updateStatus, SessionHealthStore record/summary/smoke matrix
- [x] `roadmap.md` updated with Phase 28 milestones


## Phase 29 — Desktop Productization (Complete)

**Goal:** Make RetroOasis installable and usable by non-developers. Ship a first-run wizard, persistent crash reporting opt-in, and all the onboarding hooks needed before a Tauri packaging pass.

### Phase 29a — Services

- [x] `crash-reporting.ts` — opt-in crash report collection (localStorage, max 50 reports); `recordCrash(error, context)`, `getCrashReports()`, `clearCrashReports()`, `exportCrashReports()` (JSON download); nothing sent to any remote server
- [x] `first-run-service.ts` — `isFirstRunComplete/markFirstRunComplete/resetFirstRun`; `SetupStep` enum (`display-name/emulator-paths/rom-directory/done`); `completeStep/getSetupProgress/isStepComplete/nextIncompleteStep`

### Phase 29b — Desktop UI

- [x] `SetupPage.tsx` at `/setup` — 4-step first-run wizard (display name, emulator paths, ROM directory + crash-reporting opt-in, done); progress dots; skip affordances; persists to localStorage; redirects to `/` on completion; rendered OUTSIDE the main Layout (no sidebar)
- [x] `SettingsPage.tsx` — Crash Reporting section added: toggle opt-in, show report count, Export Reports button (downloads JSON), Clear Reports button

### Phase 29c — Tests

- [x] 24 new unit tests in `phase-29.test.ts` — crash-reporting (opt-in, recordCrash, clear, export), first-run-service (isFirstRunComplete, completeStep, nextIncompleteStep, getSetupProgress)
- [x] `roadmap.md` updated with Phase 29 milestones

## Phase 30 — QoL & "Wow" Layer (Complete)

**Goal:** The stuff that makes people prefer RetroOasis. Rich game pages with artwork/metadata, per-game onboarding tips, netplay quality tips, recommended controller profiles, curated party-mode collections, and a safe patch/mod metadata browser.

### Phase 30a — Server: Rich Game Metadata

- [x] `game-metadata-store.ts` — `GameMetadataStore` with 35+ seeded entries covering NES/SNES/GB/GBC/GBA/N64/NDS/Wii/GC/Genesis/Dreamcast/PSX/PS2/PSP
  - `genre`, `developer`, `year` — display metadata for each game
  - `onboardingTips: string[]` — 1–3 tips shown on first visit to a game detail page
  - `netplayTips: string[]` — tips specifically for improving online play quality
  - `recommendedController: string` — best controller recommendation per game
  - `artworkColor: string` — CSS hex color for artwork backdrop (matches cover art palette)
  - `quickHostPreset?: '1v1' | '4-player' | 'full-party'` — suggested preset for the host modal
  - `get(gameId)`, `getAll()`, `set(record)`, `getByGenre(genre)`, `count()`

### Phase 30b — Server: Safe Patch Metadata

- [x] `patch-store.ts` — `PatchStore` with 18+ seeded safe patch entries
  - `PatchType`: `'translation' | 'qol' | 'bugfix' | 'difficulty' | 'enhancement'`
  - `safe: boolean` — only `safe: true` patches returned by the public REST API (never ROM binaries, only metadata)
  - `instructions: string[]` — step-by-step application guide
  - `tags: string[]` — `'recommended'`, `'multiplayer-compatible'`, `'beginner-friendly'`, etc.
  - `getAll()`, `getSafe()`, `getByGameId(id)`, `getById(id)`, `upsert(patch)`, `count()`

### Phase 30c — Server: Party Collections

- [x] `party-collection-store.ts` — `PartyCollectionStore` with 12 curated collections
  - Collections: Kart Racing Classics, Couch Fighters, Ultimate Party Night, Pokémon Multiverse, Streets Are Dangerous, Speedrun Ready, Hunt Together, Retro Platformer Marathon, Big Brain Energy, Zelda Chronicles, SEGA Golden Age, Online WFC Ready
  - `getAll()`, `getById(id)`, `getByGameId(gameId)`, `getByTag(tag)`, `upsert(collection)`, `count()`

### Phase 30d — REST Endpoints

- [x] `GET /api/game-metadata` — all seeded metadata entries
- [x] `GET /api/game-metadata/:gameId` — metadata for a single game (404 if unknown)
- [x] `GET /api/patches` — all safe patches; optionally `?gameId=X` to filter by game
- [x] `GET /api/party-collections` — all party collections; optionally `?tag=X` to filter by tag
- [x] `GET /api/party-collections/:id` — single collection by ID (404 if unknown)

### Phase 30e — Desktop UI

- [x] `PartyCollectionsPage.tsx` at `/party-collections` — browse all curated collections
  - Tag filter pill row (racing, fighting, party, co-op, competitive, casual, rpg, action, online, wfc, speedrun, all-ages, adventure)
  - Expandable collection cards showing curator note + full game list with system badge and links to `/game/:id`
  - Error + loading states
- [x] `🎉 Party Collections` nav item added to sidebar in `Layout.tsx` (Social group)
- [x] `/party-collections` route added to `App.tsx`
- [x] `GameDetailsPage.tsx` enhanced with Phase 30 rich content:
  - Artwork-color backdrop on the cover-emoji square (uses `metadata.artworkColor`)
  - Genre / developer / year pill row below system tips
  - **Getting Started** tip list (collapsed into an expandable card)
  - **Netplay Tips** panel (blue-tinted, only shown for multiplayer games)
  - **Recommended controller** inline badge
  - **Available Patches** expandable section listing safe patches with type badge, version, description, and optional info URL

### Phase 30f — Tests

- [x] 48 new unit tests in `phase-30.test.ts`:
  - `GameMetadataStore` — seed count, required fields, genre filter, set/upsert
  - `PatchStore` — seed count, safe-only filter, game filter, multiplayer-compatible tags, getById/upsert
  - `PartyCollectionStore` — seed count, required fields, getByGameId/getByTag/upsert
  - REST layer — GET metadata, 404 for unknown game, patch filtering, collection tag filtering, single collection fetch
- [x] `roadmap.md` updated with Phase 30 milestones


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

## Phase 8 — System Expansion UX (Complete)

**Goal:** Expand per-system UX for the core Nintendo platforms in the right order — NES/SNES/GB/GBC/GBA first (simpler backends), then N64, then NDS — with a tested golden list of recommended games and performance presets.

### Milestones
- [x] `AdapterOptions.performancePreset` ('fast' / 'balanced' / 'accurate') — N64 Mupen64Plus maps to `--emumode 2/1/0`; all other backends document the field and respect `compatibilityFlags`
- [x] `AdapterOptions.compatibilityFlags` — verbatim passthrough appended to emulator args; supported by FCEUX, Snes9x, mGBA, Mupen64Plus, and melonDS adapters
- [x] NES golden list: 9 games (Contra, Super Mario Bros., Double Dragon, Tecmo Super Bowl, Ice Climber, Balloon Fight, Dr. Mario, Mega Man 2, Ninja Gaiden)
- [x] SNES golden list: 9 games (Super Bomberman, Tetris Attack, Secret of Mana, SF2 Turbo, Super Mario Kart, Kirby Super Star, DKC, MK II, Contra III)
- [x] GB golden list: 6 games (Tetris, Pokémon Red/Blue, Dr. Mario, Kirby's Dream Land, Super Mario Land 2)
- [x] GBC golden list: 4 games (Pokémon Gold/Silver/Crystal, Dragon Warrior Monsters)
- [x] GBA golden list: 8 games (Pokémon FireRed/LeafGreen/Ruby/Sapphire/Emerald, Zelda: Four Swords, Mario Tennis, Mario Golf)
- [x] N64 golden list: 9 games (Mario Kart 64, Super Smash Bros., Mario Party 2, GoldenEye 007, Diddy Kong Racing, Mario Tennis, Mario Golf, Pokémon Stadium, Pokémon Stadium 2)
- [x] NDS golden list: 9+ games (Mario Kart DS, New SMB, Metroid Prime Hunters, Mario Party DS, Zelda: Phantom Hourglass, Pokémon Diamond/Platinum/Black, Tetris DS)
- [x] `NESPage` (`/nes`) — FCEUX relay, red theme, lobby + leaderboard tabs
- [x] `SNESPage` (`/snes`) — Snes9x relay, purple theme, lobby + leaderboard tabs
- [x] `GBAPage` (`/gba`) — mGBA link cable, indigo theme, covers GB/GBC/GBA with system filter, lobby + leaderboard tabs
- [x] `N64Page` (`/n64`) — Mupen64Plus relay, green theme, lobby + leaderboard tabs
- [x] `NDSPage` (`/nds`) — melonDS Wi-Fi relay + WFC, orange theme, lobby + leaderboard tabs
- [x] 82 new unit tests in `phase-8.test.ts` — system types, backends, adapters, performance presets, template counts, golden catalog, no-duplicate regression

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

## Phase 11 (Cloud) — Cloud & Account Features

**Goal:** Durable social platform — a user can sign in on another machine and keep their full ecosystem.

### Milestones
- [x] Optional account system (`AccountStore`): register with email + password, login → session token, revoke session, revoke all sessions on password change
- [x] Secure password hashing: scrypt (N=16384, r=8, p=1) with per-account random salt; constant-time verification via `timingSafeEqual`
- [x] Profile management: `updateProfile()` (displayName, isVerified), `changePassword()`, `getById()`, `getByEmail()`
- [x] Identity linking: `linkIdentity(accountId, identityToken)` bridges pre-registration anonymous history to the new account
- [x] SQLite schema: `accounts` table + `account_sessions` table with cascading deletes; phase-11 migration in `db.ts`
- [x] REST API — accounts: `POST /api/accounts/register`, `POST /api/accounts/login`, `POST /api/accounts/logout`, `GET /api/accounts/:id`, `PUT /api/accounts/:id/profile`, `POST /api/accounts/:id/link-identity`
- [x] Cloud save sync: `CloudSyncService` + `computeFileHash` in `@retro-oasis/save-system` (SHA-256 hash, pending-upload/pending-download/conflict/error states, pluggable `CloudStorageAdapter` interface)
- [x] Friend graph persistence: `FriendStore` backed by SQLite `friends` + `friend_requests` tables (symmetric friendship, request flow, pending/accepted/declined states)
- [x] Session history persistence: `SqliteSessionHistory` (full game session records surviving restarts)
- [x] Lightweight moderation/reporting: `ModerationStore` — file reports (player/room, 5 reason categories), auto-flag threshold (3 pending reports), moderator review/dismiss with notes, bulk clear
- [x] REST API — moderation: `POST /api/moderation/report`, `GET /api/moderation/reports`, `GET /api/moderation/reports/:targetId`, `PUT /api/moderation/reports/:id/review`, `PUT /api/moderation/reports/:id/dismiss`
- [x] SQLite schema: `moderation_reports` table with indexes on target_id and status; phase-11 migration in `db.ts`
- [x] 49 new tests covering all Phase 11 cloud/account features (phase-11.test.ts)

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

## Phase 32 — Wiimmfi & WiiLink Online Services (Complete)

**Goal:** Surface the two primary community Wi-Fi Connection revival services — Wiimmfi and WiiLink — as first-class features inside RetroOasis, giving players a clear overview of each service and zero-setup DNS auto-configuration.

### Milestones

- [x] `WfcProvider` interface gains `supportsWii: boolean` — distinguishes DS-only from DS+Wii providers
- [x] Wiimmfi description updated to emphasize it is the largest WFC revival service, running since 2014, covering DS and Wii titles
- [x] `WiiLink WFC` added as second provider in `WFC_PROVIDERS` (DNS `167.235.229.36`, URL `https://www.wiilink24.com`, `supportsWii: true`)
- [x] `WIILINK_CHANNELS` constant — 6 active WiiLink channel definitions: Forecast, News, Everybody Votes, Check Mii Out, Nintendo, Demae
- [x] `WiiLinkChannel` interface with `id`, `name`, `emoji`, `description`, `active` fields
- [x] `GET /api/wfc/wiilink-channels` REST endpoint — returns all WiiLink channel definitions
- [x] `OnlineServicesPage` (`/online-services`) — three-tab info hub:
  - **Overview**: history of the WFC shutdown, all three providers at a glance, zero-setup DNS explainer (3-step guide)
  - **Wiimmfi**: dedicated tab with key facts (founded 2014, DNS, platform coverage, 2000+ game count), notable game list, default-provider callout
  - **WiiLink**: dedicated tab with WiiLink WFC provider card, channel grid (6 channels with emoji + active badge), infrastructure facts, Wiimmfi-vs-WiiLink comparison note
- [x] 🌐 Online Services nav item added to sidebar (Social group)
- [x] Route `/online-services` registered in `App.tsx`
- [x] 39 new unit/integration tests in `phase-32.test.ts`:
  - `WFC_PROVIDERS` — length, order, required fields, per-provider DNS/URL/supportsWii assertions
  - `getWfcProvider()` — lookup by all three IDs + null for unknown
  - `DEFAULT_WFC_PROVIDER_ID` — still resolves to 'wiimmfi'
  - `WIILINK_CHANNELS` — length, required fields, all active, per-channel id checks, uniqueness
  - REST layer — `GET /api/wfc/providers` (3 providers), `GET /api/wfc/providers/:id` (200 + 404), `GET /api/wfc/wiilink-channels` (6 channels)
- [x] `roadmap.md` updated with Phase 32 milestones
