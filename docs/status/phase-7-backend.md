# Phase 7 Status — Backend Hardening + Advanced Integration

> **Current state (2026-03-11):** Phase 7 is substantially complete. The lobby server now has a proper auth/identity layer, save-sync REST endpoints, session history, live presence broadcast, deployable relay config, an integration test suite, and a push notification service on the desktop side.

## What was completed this phase

### Auth / identity layer
- **Display name validation** (`apps/lobby-server/src/auth.ts`):
  - `validateDisplayName` — checks min (1) / max (24) length and rejects control characters **before** normalisation.
  - `normalizeDisplayName` — trims and collapses internal whitespace.
  - Applied server-side on `create-room`, `join-room`, and `join-as-spectator`.
- **Room ownership tokens** (`auth.ts`):
  - `generateOwnerToken(roomId)` — HMAC-SHA256 of roomId, first 32 hex chars; deterministic within a process.
  - `verifyOwnerToken(roomId, token)` — constant-time comparison to prevent timing attacks.
  - Token returned in `room-created` server message (`ownerToken` field).
  - Required by the new `kick-player` message; stored as `ownerToken` in `LobbyContext`.

### Kick-player action
- `kick-player` client message added to the WS protocol.
- Server verifies host identity via `room.hostId === playerId` AND validates the `ownerToken`.
- Kicked player receives a `room-left` event; remaining players receive `room-updated`.

### Save-sync REST endpoints (`apps/lobby-server/src/save-store.ts`)
- In-memory `SaveStore` with per-slot deduplication and optimistic-concurrency conflict detection.
- `version` counter increments on every overwrite; caller can pass `expectedVersion` to detect conflicts.
- Endpoints:
  - `POST /api/saves/:gameId` — upload / replace a save (base64 data + name).
  - `GET /api/saves/:gameId` — list saves (metadata only, no payload).
  - `GET /api/saves/:gameId/:saveId` — download a specific save with data.
  - `DELETE /api/saves/:gameId/:saveId` — delete a save.
- Conflict response includes `serverVersion` and `serverUpdatedAt` for client-side resolution.

### Session history (`apps/lobby-server/src/session-history.ts`)
- In-memory `SessionHistory` stores up to 500 records (oldest evicted).
- `startSession` called on `start-game`; `endSession` called when room closes (explicit leave or disconnect).
- Records include: gameId, gameTitle, system, player names, startedAt, endedAt, durationSecs.
- Endpoints:
  - `GET /api/sessions` — all sessions (newest first); `?completed=true` to filter.
  - `GET /api/sessions/:roomId` — specific session by room ID.

### Live presence transport
- `broadcastPresence(lobby, connections)` called on `create-room`, `join-room`, `start-game`, and `close`.
- Server sends `presence-update` to **all** connected clients whenever presence changes.
- `PresencePlayer` type added to both server and desktop `lobby-types.ts`.
- `LobbyContext` exposes `onlinePlayers: PresencePlayer[]` updated in real time from the WS stream.
- Anonymous/unnamed connections are filtered out client-side.

### Deployable relay host configuration
- `RELAY_HOST` env var: the public hostname/IP surfaced in `game-starting` events (default `localhost`).
- `RELAY_PORT_MIN` / `RELAY_PORT_MAX` env vars: override the relay TCP port range (default 9000–9200).
- `RELAY_BIND` env var: the address the relay TCP servers bind to (default `0.0.0.0`).

### Push notification service (`apps/desktop/src/lib/notification-service.ts`)
- `NotificationService` class wraps the browser Notification API.
- Uses `globalThis.Notification` for test-environment compatibility.
- Permission management: `requestPermission()`, `permission` getter, `isSupported` getter.
- Convenience helpers: `notifyFriendOnline`, `notifyFriendInLobby`, `notifyFriendStartedGame`, `notifyRoomReady`, `notifyGameStarting`.
- Singleton `notificationService` exported for app-wide use.
- Auto-close timers on notifications.

### Integration test suite (`apps/lobby-server/src/__tests__/lobby-integration.test.ts`)
- 12 tests covering: connect/welcome, unique player IDs, create room (+ ownerToken), display name validation, whitespace normalisation, join by code, room-updated broadcast to host, invalid code error, start game (all ready), start game rejected (not all ready), kick player (valid token), kick player (invalid token), ping/pong.

### Unit tests added
- `auth.test.ts` — 13 tests for display name validation and token generation/verification.
- `session-history.test.ts` — 7 tests for session lifecycle and ordering.
- `save-store.test.ts` — 9 tests for CRUD, version conflict detection, and gameId scoping.
- `notification-service.test.ts` — 10 tests for permission states and notification creation.

## Total test count after Phase 7
| Workspace | Tests |
|-----------|-------|
| `@retro-oasis/lobby-server` | 49 |
| `@retro-oasis/desktop` | 40 |
| **Total** | **89** |

## Known limitations / remaining work
- **SaveStore** is in-memory — data lost on server restart. Phase 8 will migrate to SQLite.
- **SessionHistory** is in-memory — same limitation.
- **Friend list** is still client-side mock data (`PresenceClient.seedMockFriends`). Server-side friend social graph is Phase 8.
- **Push notifications** are wired up but not automatically triggered from LobbyContext yet — callers must invoke `notificationService.*` manually. A follow-up will hook into `presence-update` events.
- `LobbyContext.ownerToken` is stored but not yet surfaced in any UI component — available for the upcoming kick-player host controls panel.
