# RetroOasis Full Product & Code Audit

**Date:** 2026-03-10
**Auditor:** Principal audit pass — all packages, apps, and docs reviewed
**Basis:** Complete source read of every file in `apps/` and `packages/`, all existing `docs/status/` notes, `src/` C++ inventory

---

## 1. Executive Summary

RetroOasis has a well-conceived product vision and a surprisingly solid WebSocket lobby core. The multiplayer room flow — create, join, ready, chat, copy room code — works end-to-end and is genuinely usable. The UI is cohesive, the visual language is consistent, and the DS/N64 contextual hints add real user value.

The problem is depth. Almost every feature one layer below the lobby collapses into a stub, seed data, or a `// TODO` comment:

- The **game library is entirely hardcoded mock data**. No user's ROMs are scanned or shown.
- The **friends/presence system shows permanently static mock friends**. It is indistinguishable from a mockup.
- The **save management UI operates on seeded localStorage fixtures** and exports JSON metadata, not actual save files.
- The **"Start Game" button allocates a relay port and stops**. No emulator is launched.
- Five of the eight `packages/` crates are completely unconnected to any running app code.

The architecture diagram is correct — the layers exist — but the wiring between them was never completed. The app presents a convincing demo rather than a usable product.

A new engineer reading only the README would believe features such as cloud save sync, automatic Wiimmfi WFC setup, and friend presence are working. They are not. This is the most critical honesty gap in the project.

---

## 2. What Is Strong

### 2.1 Lobby Server — `apps/lobby-server/`
**Classification: solid (within its scope)**

- Clean single-file architecture: `index.ts` → `handler.ts` → `LobbyManager` + `NetplayRelay`
- Full WebSocket protocol: create/join/spectate/leave/toggle-ready/start-game/list/chat/ping-pong
- `LobbyManager` correctly handles: host transfer on leave, slot renumbering, spectator support, idempotent re-join
- `NetplayRelay` correctly allocates TCP ports (9000–9200), buffers the 36-byte session token, and forwards subsequent data to all other sockets in the session
- Connection quality derived from server-side RTT and broadcast to all room members — a nice real-time touch
- Port deallocation on room deletion is handled

**Honest caveats:**
- Rooms are in-memory. A server restart wipes all rooms. Players have no way to recover.
- `connections` Map is module-level global, not attached to `LobbyManager`. If handler.ts were ever split, this breaks.
- No rate limiting on chat or room creation. One client can spam indefinitely.
- `game-starting` never sends `relayHost` (always `undefined`), so clients fall back to `'localhost'`, which fails in any non-local deploy.
- Relay sessions are never deallocated when a game ends naturally — only when the room's last player disconnects. Long-running sessions will exhaust the 200-port pool.

### 2.2 LobbyContext + Frontend Room Flow — `apps/desktop/src/context/LobbyContext.tsx`
**Classification: usable but fragile**

- Auto-reconnect with 3s delay works
- Ping loop starts on connect, stops on disconnect
- `currentRoomRef` sync pattern avoids stale closure in callbacks
- The `room-updated` / `room-left` / `room-list` state merges are correct
- `joinById` for direct URL navigation is a nice touch
- Error display with clearable banners is appropriate

**Honest caveats:**
- WS URL is hardcoded to `ws://localhost:8080` with no env-var override. Will break in any deployment.
- On reconnect after disconnect, `currentRoom` is cleared, which causes LobbyPage to re-trigger `joinById`. This works but navigates the user back through a join race condition if the server still has the room.
- `chatMessages` accumulates indefinitely in memory; no limit or expiry.

### 2.3 Emulator Adapters — `packages/emulator-bridge/src/adapters.ts`
**Classification: solid as a library (untested in practice)**

- Correct CLI flag conventions documented and implemented for all supported backends: FCEUX, Snes9x, mGBA, SameBoy, VBA-M, Mupen64Plus, melonDS, DeSmuME
- NDS screen layout and touch flags correctly mapped
- Wiimmfi WFC DNS override flag in place
- ROM scanner (`scanner.ts`) handles common extensions and infers titles

**Honest caveats:** This code has never been called by a real user flow. It is untested outside unit coverage.

### 2.4 Session Templates — `packages/session-engine/src/templates.ts`
**Classification: solid metadata, zero integration**

- 35+ game-specific templates with correct per-game netplay modes, latency targets, save rules, and WFC configs
- `NO_SAVE_RULES` vs `DEFAULT_SAVE_RULES` distinction is correct
- Template lookup by gameId with system fallback is sensible

**Honest caveat:** Not referenced by a single line of app or server code. A complete dead letter.

### 2.5 UI Visual Design
**Classification: solid**

- Consistent CSS custom property design token system (`--color-oasis-*`)
- Tailwind usage is disciplined — no utility sprawl
- Dark theme, emoji-based cover art, and badge chips create a coherent retro aesthetic
- Responsive grid layouts work on mobile breakpoints
- Toast notifications, modal patterns, and inline confirm banners are well-executed

---

## 3. What Is Fragile

### 3.1 WebSocket Server Has No Authentication or Identity
**Classification: usable but fragile**

Player IDs are `randomUUID()` per connection. Display names are free-text strings with no validation. Anyone can claim any name. There is no session persistence, so a page reload creates a new identity and leaves the old player slot ghosted until the server notices the old socket closed.

This is acceptable for a demo but breaks any feature that assumes identity (friend lists, save attribution, cloud sync).

### 3.2 LobbyContext WebSocket URL Is Not Configurable
**Classification: technical debt**

`const WS_URL = 'ws://localhost:8080'` is a hardcoded constant in `LobbyContext.tsx`. There is no `VITE_WS_URL` or similar. Deploying the server to any host other than `localhost` requires a code change. This is a one-line fix but was never done across multiple phases.

### 3.3 Presence System Is a Permanent Mock
**Classification: misleading**

`PresenceProvider` calls `client.seedMockFriends()` unconditionally on mount. The friends list always shows the same 6 fabricated users (Player2, LinkMaster, StarFox99, etc.) with fake room codes (`MK6401`, `PARTY2`) that point to nonexistent rooms.

The `PresenceClient` class has `setStatus()` and `setCurrentGame()` methods with `// TODO: Broadcast to server` bodies. There is no transport. The "Join" buttons on friend cards open `JoinRoomModal` with hardcoded room codes; joining will fail silently because those rooms don't exist on the server.

From a user's perspective: the Friends page and homepage presence section look populated and interactive but are completely fake. This is the most misleading part of the product.

### 3.4 "Invite Friends" Button Is a No-Op
**Classification: broken**

`GameDetailsPage` renders an "Invite Friends" button with no `onClick` handler. It does nothing. It should either be removed or replaced with a copy-room-code action.

### 3.5 "Recently Played" Is Fake
**Classification: misleading**

`HomePage` renders a "Recently Played" section using `allGames.slice(0, 4)` — the first four games from the mock data array, always the same four games regardless of play history. There is no play tracking.

### 3.6 mock-games.ts Has a Duplicate Entry
**Classification: technical debt**

`n64-mario-party-2` appears twice in `MOCK_GAMES` (lines 45–57 and 59–68). Identical `id`, `title`, and all properties. This causes React key collisions in any list that uses `game.id` as key.

### 3.7 NDS Color Inconsistency
**Classification: technical debt**

`SYSTEM_COLORS.NDS` in `api-client.ts` is `'#A0A0A0'` (grey), but `mock-games.ts` hardcodes `'#E87722'` (orange) for all NDS games. `SavesPage.tsx` defines its own `SYSTEM_COLORS` map (third definition). Three separate system color definitions exist in three files with inconsistent values.

---

## 4. What Is Incomplete

### 4.1 The Core User Value Proposition — End-to-End Game Launch
**Classification: partial**

The entire game launch pipeline stops at a relay port banner:

1. Host creates room — ✓
2. Players join — ✓
3. All ready up — ✓
4. Host clicks "Start Game" — ✓ (sends `start-game` to server)
5. Server allocates relay port, broadcasts `game-starting` — ✓
6. UI shows green banner with relay address — ✓
7. **Emulator is launched with correct ROM, relay address, and player slot** — ✗ never happens
8. **Session token is sent to each player's emulator to authenticate with the relay** — ✗ never happens

Steps 7 and 8 are the entire point of the product. The relay TCP handshake expects a 36-byte UUID session token per connection, but no mechanism exists to distribute session tokens to clients, and `EmulatorBridge.launch()` is never called from any user flow.

### 4.2 Game Library — No ROM Discovery
**Classification: partial**

The library shows 23 hardcoded game cards from `mock-games.ts`. The `packages/emulator-bridge/src/scanner.ts` has a working ROM directory scanner with correct extension mapping. The `packages/game-db` package has a seeded catalog of ~30 games with metadata. None of these are wired to the UI.

There is no flow for users to specify a ROM directory, see their actual games, or associate a ROM file with a lobby room. `HostRoomModal` asks for game title/system by free text or mock-game selection — there is no file picker.

### 4.3 Presence / Friends — No Real Transport
**Classification: partial**

`PresenceClient` is a correct in-memory data structure with the right interface for a real implementation. But:
- No WebSocket or HTTP connection to any backend
- `setStatus()` and `setCurrentGame()` are stubs
- No mechanism to add or remove friends
- No "Add Friend" UI anywhere

### 4.4 Save System — localStorage Fixtures, Not Real Files
**Classification: partial**

`save-service.ts` operates on seeded `localStorage` records. The `exportSave()` function downloads JSON metadata, not an actual `.sav` file. The `packages/save-system/SaveManager` correctly uses `fs/promises` to manage real files — but the browser UI never calls it (correct, since Node.js `fs` doesn't run in the browser). The bridge between them (Tauri IPC, Electron IPC, or a dedicated API endpoint) was never built.

Cloud sync (`cloud-sync.ts`) has `upload()` and `download()` methods that immediately toggle the status field from `pending-*` back to `synced` with a `// TODO: Actually upload` comment sandwiched in between. It is a state machine with no side effects.

### 4.5 Saves Page — "Restore from Cloud" Is Simulated
**Classification: misleading**

The Saves page shows conflict resolution and cloud restore flows that feel real (modal, confirmation, toast). But `completePendingSync()` just flips a `syncStatus` field in localStorage. Nothing is downloaded. The cloud version of the save doesn't exist anywhere.

### 4.6 Authentication / Identity
**Classification: partial**

Display name is stored in `localStorage.getItem('retro-oasis-display-name')`. There is no registration, login, password, or OAuth. Every page reload generates a new player UUID on the server. Two players with the same display name are indistinguishable.

---

## 5. What Is Overcomplicated

### 5.1 Package Proliferation
**Classification: technical debt**

There are 8 packages in `/packages`. Of these, only `presence-client` and `shared-types` are consumed by any app. The other six (`game-db`, `session-engine`, `emulator-bridge`, `save-system`, `multiplayer-profiles`, `ui`) are completely disconnected from running code.

Each package has its own `tsconfig.json`, `package.json`, and build pipeline. This is workspace overhead for zero current user value. These packages are well-designed as future targets, but they should be honestly labeled as forward stubs, not active packages.

### 5.2 `packages/ui` Is Empty
**Classification: ready to cut**

`packages/ui/src/index.ts` exports only `UI_VERSION = '0.1.0'` and a TODO comment. The desktop app defines all its components inline. This package exists but does nothing.

### 5.3 Duplicate Game Data Sources
**Classification: technical debt**

The same game metadata exists in three separate places:
- `apps/desktop/src/data/mock-games.ts` — 22 entries (with one duplicate)
- `packages/game-db/src/seed-data.ts` — ~30 entries (separate, not read)
- Session templates in `packages/session-engine/src/templates.ts` — game IDs referenced but not cross-validated

These can drift and have drifted. `mock-games` lacks `gb-pokemon-red` (which appears in `save-service.ts` seed data). `game-db` has games not in `mock-games`. The canonical game list is unknown.

### 5.4 Three Separate System Color Maps
**Classification: technical debt**

System-to-color mappings are defined independently in:
1. `mock-games.ts` (inline per entry, not a map)
2. `api-client.ts` (`SYSTEM_COLORS` constant) — NDS is grey `#A0A0A0`
3. `SavesPage.tsx` (`SYSTEM_COLORS` constant) — NDS is orange `#E87722`

Values are inconsistent. One source of truth is needed.

### 5.5 `GameApiClient` Hybrid Mode with No Backend
**Classification: overbuilt for current state**

`api-client.ts` implements three modes: `mock`, `backend`, and `hybrid`. The `backend` mode constructs real HTTP requests to `VITE_BACKEND_URL`. There is no backend HTTP server for game data anywhere in the repo. The `/api/games` endpoints described in the JSDoc header do not exist. The retry logic (`fetchWithRetry` with 3 retries) is well-written but exercises a path that can never succeed.

### 5.6 `HostRoomModal` Requires Free-Text System Entry
**Classification: usable but fragile**

When hosting a room from the home page (not from a game detail page), the user types a system name as free text. There is no validation. A user typing "n64", "N64", or "Nintendo 64" would all result in different system strings stored in the room, causing the system badge to display incorrectly.

---

## 6. What Should Be Simplified or Removed

| Item | Action | Rationale |
|---|---|---|
| `packages/ui` | **Remove or collapse into desktop** | Exports only a version string; no components |
| `MockLobby` type and `MOCK_LOBBIES` in `mock-games.ts` | **Remove** | `MOCK_LOBBIES` is unused; replaced by real room data from WebSocket |
| "Invite Friends" button on `GameDetailsPage` | **Replace with copy-room-code** | Currently a no-op; misleads users |
| "Recently Played" section on `HomePage` | **Remove or make honest** | Always shows first 4 mock games; conveys false history |
| `GameApiClient` `backend` / `hybrid` modes | **Defer** | No backend exists; mock mode is the only path |
| `cloudSyncService` `upload()`/`download()` bodies | **Remove fake state transitions** or label clearly as stub | Fake sync completes immediately; misleads any reader |
| Duplicate `n64-mario-party-2` entry in `mock-games.ts` | **Delete one** | Causes React key collisions |
| `MOCK_LOBBIES` export | **Delete** | Unused |
| `WS_URL` hardcoded constant | **Move to env var** | Breaks any deployment |

---

## 7. Highest-Risk Technical Areas

### 7.1 The Relay Session Token Protocol Is Unimplemented Client-Side
**Risk: critical**

The TCP relay expects each connecting emulator to send a 36-character UUID session token as its first 36 bytes. The server correctly buffers and parses this token. But:
- The server never generates or sends session tokens to clients
- The `game-starting` event sends only `relayPort` and optionally `relayHost`
- Even if an emulator connected to the relay port, it would not know what token to send
- Without matching tokens, `forwardData` would forward to no sockets (since `sockets` map would be empty or mismatched)

The relay works for direct connection scenarios but the multiplayer forwarding behavior is untested with real emulators.

### 7.2 No Room Persistence — Server Restart Kills All Sessions
**Risk: high**

`LobbyManager` is pure in-memory. Any server crash or deploy evicts every active room. Users in a game will see a disconnect with no recovery path. This is appropriate for early development but is an operational risk if the server is left running for real users.

### 7.3 `EmulatorBridge.launch()` Uses `backendId` as Executable Name
**Risk: high**

```ts
child = spawn(options.backendId, args, { ... })
```

The `backendId` values are strings like `'mupen64plus'`, `'mgba'`, `'melonds'`. This will fail if:
- The binary has a different name on the host system (`melonDS` vs `melonds`)
- The binary is not on `$PATH`
- The binary is at an absolute path not matching the ID

There is no user-configurable emulator path. The `KNOWN_BACKENDS` catalog has `executableName` and `downloadUrl` fields but `launch()` ignores them.

### 7.4 Presence Fake Data Allows Accidental "Joining" Dead Rooms
**Risk: medium**

The homepage shows friend session cards for "Player2" (room code `MK6401`) and "StarFox99" (room code `PARTY2`). Clicking "Join" opens `JoinRoomModal` pre-filled with the hardcoded code. Submitting it sends a real `join-room` WebSocket message to the server. The server will respond with an error ("Could not join room"). The user sees "Could not join — check the room code" with no explanation that these are demo friends. This is a confusing experience.

### 7.5 `connections` Map Is Global State in `handler.ts`
**Risk: medium for scaling**

The `connections` Map is module-level. This is fine for a single Node.js process but means there is no path to horizontal scaling (multiple server processes) without introducing a shared store (Redis pub/sub, etc.). Low immediate risk but architectural dead-end.

---

## 8. Highest-Value Next Improvements

These are ranked by product impact per engineering effort.

### 8.1 Fix WS URL to Be Configurable — 30 minutes
Read `VITE_WS_URL` from env with `ws://localhost:8080` fallback. Enables non-localhost deployment. Unblocks all subsequent testing.

### 8.2 Wire `EmulatorBridge.launch()` into `game-starting` — 2 days
When the server broadcasts `game-starting`, the client should call `EmulatorBridge.launch()` with the correct ROM path, netplay host/port, and player slot. This closes the loop on the core product promise. Requires:
- A way for the user to specify their ROM file path (file picker or ROM directory config)
- A client-side mechanism to invoke Node.js APIs (Tauri command or Electron IPC bridge)
- Distributing a session token (generate in server, send in `game-starting`, pass to emulator)

### 8.3 Replace Mock Friends with Real Lobby Presence — 2–3 days
Extend the lobby server's `room-updated` broadcast to include presence data. Clients can build a real "who is in which room" view from existing `publicRooms` state they already receive. This eliminates the fake friends display without requiring a separate presence service.

### 8.4 Wire `packages/game-db` to the Library — 1 day
Replace `MOCK_GAMES` import with `GameCatalog.getAll()` from `packages/game-db`. Merge the two seed data sets. Eliminate the duplicate. The filtering and `ApiGame` normalization in `api-client.ts` can be reused.

### 8.5 Add ROM Directory Configuration — 1 day
Add a Settings page (or first-run modal) where users specify their ROM directory. Wire `scanRomDirectory()` from `packages/emulator-bridge/scanner.ts` to populate the library. The scanner is correct and complete. This is a missing UI + IPC bridge, not a missing algorithm.

### 8.6 Remove or Honestly Label Fake Presence — 1 hour
Either remove the Friends page, clearly label it "Coming Soon," or populate it from real room data. Leaving fake persistent friends that produce join errors is actively harmful to first impressions.

### 8.7 Fix the Duplicate Game Entry — 5 minutes
Delete the second `n64-mario-party-2` block in `mock-games.ts`.

### 8.8 Fix `game-starting` to Include `relayHost` — 1 hour
The server broadcasts `{ type: 'game-starting', roomId, relayPort }` but never sets `relayHost`. Clients default to `'localhost'`. Add `relayHost: process.env.RELAY_HOST ?? 'localhost'` to the server broadcast.

---

## 9. Package-by-Package Notes

### `apps/desktop`
**Overall: usable but fragile**

- Pages: Home, Library, GameDetails, Lobby, Saves, Friends — all render
- LobbyContext WebSocket flow is solid
- All game data sourced from `mock-games.ts` — never from `packages/game-db`
- Presence from `PresenceContext` is entirely mock seeded
- Saves from `save-service.ts` use localStorage seed data
- 1 test file (`api-client.test.ts`) with 12 smoke tests — all test mock mode only
- `packages/ui` is imported nowhere
- `packages/shared-types` is not imported by the desktop app (it defines its own types inline in `services/lobby-types.ts`)
- `MOCK_LOBBIES` in `mock-games.ts` is never imported or used

### `apps/lobby-server`
**Overall: solid for its scope**

- 5 source files, cleanly organized
- In-memory only: acceptable for development, problematic for production
- No authentication, no rate limiting, no input validation beyond JSON parse
- `game-starting` relay host field bug (always `undefined`)
- Relay port leak for rooms that end naturally without all players disconnecting
- Dependency: only `ws` — admirably minimal

### `packages/shared-types`
**Overall: solid as a type library**

- Defines `Game`, `Lobby`, `Save`, `Session`, `Emulator`, `Social`, `Compatibility` types
- Well-organized, well-structured
- Ironically, `apps/desktop` does not import from it — defines its own `lobby-types.ts`

### `packages/game-db`
**Overall: solid but unused**

- `GameCatalog` class with query/filter API — works correctly
- ~30 game seed entries in `seed-data.ts`
- Never imported by any app
- Overlaps with `mock-games.ts` (different game lists, no single canonical source)

### `packages/emulator-bridge`
**Overall: solid library, zero integration**

- `EmulatorBridge`: correct process spawning, SIGTERM, state tracking
- `adapters.ts`: correct CLI flags per backend, well-documented
- `scanner.ts`: working ROM file scanner with title inference
- `backendId`-as-executable-name bug (ignores `KNOWN_BACKENDS.executableName`)
- Never called from any app code

### `packages/save-system`
**Overall: solid library, zero integration**

- `SaveManager`: correct `fs/promises` implementation for local saves
- `CloudSyncService`: scaffold-only — upload/download are no-ops
- Not called by any app; browser can't use `fs` anyway (Tauri/Electron bridge missing)

### `packages/session-engine`
**Overall: solid metadata, zero integration**

- `SessionEngine`: correct state machine (preparing → launching → active → ended)
- `SessionTemplateStore`: 35+ game templates with correct per-game configs
- Never instantiated or called from any app or server
- `SessionTemplateConfig.wfcConfig` contains correct Wiimmfi DNS server — unused

### `packages/multiplayer-profiles`
**Overall: solid data structure, zero integration**

- `MultiplayerProfileStore`: stores per-game multiplayer config
- Zero entries populated anywhere (no seed data, no runtime population)
- Never called from any app

### `packages/presence-client`
**Overall: correct interface, no transport**

- `PresenceClient`: clean in-memory implementation
- `setStatus()`, `setCurrentGame()` have `// TODO: Broadcast to server`
- `seedMockFriends()` is the only method that gets called in production
- Used by desktop app, but only to deliver static mock data

### `packages/ui`
**Overall: ready to cut**

- Exports `UI_VERSION = '0.1.0'` and a TODO comment
- No components
- Zero app imports

---

## 10. Recommended Priorities for the Next Milestone

The goal of the next milestone should be to turn the lobby into a complete game session — closing the loop from "room ready" to "emulators launched and connected."

### Must Do (Close the Core Loop)

1. **Make WS URL configurable** via `VITE_WS_URL` env var
2. **Add ROM file/directory configuration UI** (file picker or settings page)
3. **Wire `EmulatorBridge.launch()`** into the `game-starting` client handler
4. **Generate and distribute session tokens** so relay handshakes work
5. **Fix `relayHost` in `game-starting`** server broadcast

### Should Do (Fix Misleading UX)

6. **Remove or replace fake friends** — either stub "Friends (Coming Soon)" or build minimal real lobby presence
7. **Remove "Recently Played"** section until play tracking exists
8. **Fix "Invite Friends" button** — make it copy the room code with one click
9. **Fix duplicate `n64-mario-party-2`** in mock-games.ts

### Should Do (Reduce Technical Debt)

10. **Consolidate game data** — merge `mock-games.ts` and `game-db/seed-data.ts` into a single source; wire `packages/game-db` to the UI
11. **Consolidate system colors** — one canonical map, imported everywhere
12. **Mark `packages/ui` as placeholder** or delete it
13. **Add relay port deallocation** when game ends, not just on player disconnect

### Defer (Until Core Loop Ships)

- Cloud save sync (no backend)
- Real authentication / user accounts
- `packages/multiplayer-profiles` integration
- `packages/session-engine` integration into the lobby flow
- Tauri packaging / distribution
- Testing beyond the single mock-mode API client test suite

### Cut (Low Value, High Confusion)

- `GameApiClient` `backend` / `hybrid` modes until an HTTP backend exists
- `MOCK_LOBBIES` export from `mock-games.ts`
- `CloudSyncService` upload/download fake transitions (replace with honest "not yet implemented" throws)
