# RetroOasis Development Roadmap

> **Honest progress snapshot (2026-03-10)**
>
> - The current working stack is **React + Vite desktop UI + WebSocket lobby server + TCP relay + local HTTP launch API (`/api/launch`)**.
> - **Tauri/native packaging, a C++ IPC bridge into the melonDS core in `/src`, and a broader REST backend for catalog/save/presence data are still not complete.**
> - The lobby now triggers **local emulator launch requests** through the server, but **real per-game ROM discovery/selection and relay token handoff into emulator processes are still incomplete**.
> - Presence, activity, saves, and cloud flows have useful UI scaffolding, but the **live/server-backed versions remain future work unless explicitly marked “mock/dev/demo.”**

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
- [ ] Relay session token handoff from `game-starting` into emulator processes
- [ ] Editable controller remapping UI
- [ ] Game catalog REST backend (`/api/games`, `/api/systems`) to back `VITE_API_MODE=backend`
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
- [ ] Editable N64 controller remapping UI
- [ ] Voice chat hooks
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
- [ ] DS wireless-inspired room UX
- [x] Advanced compatibility badges
- [x] Screen swap hotkeys

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
- [ ] Touch input calibration panel (map mouse coordinates to DS screen coords)
- [x] DS-specific compatibility badges (WFC Online, Touch Controls, Download Play)
- [ ] Advanced: DSi mode detection and DSiWare support

## Cross-Cutting Quality Work

**Goal:** Keep the current prototype honest, debuggable, and safe to extend while native packaging and deeper backend work are still in progress.

### Milestones
- [x] `docs/status/debug-pass.md` — targeted bug-fix pass across lobby/frontend/server flows
- [x] `docs/status/current-state-audit.md` — honest working-vs-mocked inventory
- [x] `docs/status/full-audit.md` — full product/code audit with gap analysis
- [x] Phase status notes for N64, saves, presence, and DS work
- [x] Desktop API smoke tests (`apps/desktop/src/lib/__tests__/api-client.test.ts`, 14 tests)
- [x] Workspace validation documented and re-verified after `npm install` (`npm run typecheck`, `npm run test -w @retro-oasis/desktop`)
- [ ] Integration coverage for emulator launch, relay token flow, ROM scanning, save I/O, and live presence
- [ ] Native desktop verification checklist for Tauri / IPC flows
- [ ] Backend hardening: auth/identity, rate limiting, persistence, and deployable relay host configuration

## Current Carry-Over / Blockers

- [ ] Tauri native shell and IPC command surface for desktop-only capabilities
- [ ] C++ IPC bridge between the TypeScript launcher and the melonDS core in `/src`
- [ ] Backend expansion beyond WebSocket rooms + `/api/launch` (catalog, saves, presence, auth)
- [ ] Real ROM ownership/discovery flow: filesystem scan, file association, and per-room ROM selection
- [ ] Relay authentication handoff: feed per-player session tokens into launched emulator processes

## Future Ideas
- Tournament-style rooms
- Seasonal featured games
- Ranked/casual matchmaking tags
- Custom room themes
- Achievement system
- Game clip sharing
- Mobile companion app
