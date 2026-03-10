# Debug Pass — RetroOasis

> Date: 2026-03-10  
> Scope: Full stack — frontend, lobby server, shared packages  
> Priority flows audited: app startup, ROM/library loading, host room, join by code, ready/unready sync, session launch, disconnect/reconnect, save sync, presence/friends, DS & N64 presets, return-to-lobby, repeated session cycles

---

## Bugs Found & Fixed

### Bug 1 — CRITICAL: Stale `currentRoom` after WebSocket disconnect blocks auto-rejoin

**File:** `apps/desktop/src/context/LobbyContext.tsx`  
**Root cause:** `ws.onclose` only cleared `playerId` and `latencyMs`. On reconnect the client got a new `playerId` but `currentRoom` kept the old room object with the old player ID. The `LobbyPage` auto-join effect (`if (lobbyId && !currentRoom)`) never fired because `currentRoom` was non-null, leaving users stuck on a ghost lobby that no longer existed server-side.  
**Fix:** Added `setCurrentRoom(null)`, `setRelayInfo(null)`, and `setChatMessages([])` to `ws.onclose`.  
**Regression risk:** Low — clients will re-join a lobby via the existing auto-join effect on LobbyPage after reconnect, which is the correct behaviour. Any in-progress game will require a manual rejoin as spectator (no auto-rejoin for `in-game` rooms — accepted limitation).

---

### Bug 2 — HIGH: `startGame` allowed re-triggering on an already in-game room

**File:** `apps/lobby-server/src/lobby-manager.ts` → `startGame()`  
**Root cause:** `startGame` only validated host identity and all-ready state, not the room's current status. Calling it again when `room.status === 'in-game'` succeeded (all players still marked ready), re-broadcast `game-starting` to everyone, and re-allocated relay (harmlessly, since `allocateSession` is idempotent). Repeated rapid clicking or client-side state lag could trigger this unintentionally.  
**Fix:** Added `if (room.status !== 'waiting') return null;` guard.  
**Regression risk:** Low — the guard only prevents a redundant server-side operation that was harmless but noisy.

---

### Bug 3 — HIGH: Duplicate player slot numbers after a player leaves

**File:** `apps/lobby-server/src/lobby-manager.ts` → `leaveRoom()`  
**Root cause:** Player slots were assigned as `room.players.length` at join time (0, 1, 2…). When a player departed the slots of the remaining players were never renumbered. If player 1 (slot 1) left and a new player joined, they were assigned `slot: 1` (the new length), colliding with any player who still had slot 1. The LobbyPage displayed both as "P2".  
**Fix:** Added `room.players.forEach((p, index) => { p.slot = index; })` after the filter step.  
**Regression risk:** Medium — any client UI that caches a player's slot by index may momentarily show a stale slot label during the `room-updated` broadcast, but the next render with fresh data corrects it. No data loss.

---

### Bug 4 — MEDIUM: Room code join was case-sensitive

**File:** `apps/lobby-server/src/lobby-manager.ts` → `joinByCode()` / `joinByCodeAsSpectator()`  
**Root cause:** Room codes are generated as uppercase (e.g., `AB3XPQ`) but the comparison was a strict equality check `room.roomCode === roomCode`. Users who typed lowercase or mixed-case codes received a silent "Could not join room" error.  
**Fix:** Normalise `roomCode` to uppercase via `.toUpperCase()` before the loop comparison.  
**Regression risk:** Very low — only makes join more permissive; existing uppercase codes still match.

---

### Bug 5 — MEDIUM: Chat messages not cleared on room leave or WS disconnect

**File:** `apps/desktop/src/context/LobbyContext.tsx`  
**Root cause:** `leaveRoom()` cleared `currentRoom` and `relayInfo` but not `chatMessages`. Messages from a previous room accumulated in the array across sessions. In the common flow the `roomId` filter in LobbyPage suppresses them, but the array grew unboundedly and caused confusion when quickly rejoining the same room after leaving (messages briefly reappeared before the filter could apply to the empty room).  
**Fix:** Added `setChatMessages([])` to both `leaveRoom()` and `ws.onclose`.  
**Regression risk:** Low — chat state is session-scoped and ephemeral by design.

---

### Bug 6 — MEDIUM: Ready/Start-Game buttons visible and active after game had already started

**File:** `apps/desktop/src/pages/LobbyPage.tsx`  
**Root cause:** The non-spectator action buttons (Ready Up, Start Game, Leave) were always rendered when `!isSpectating`, regardless of `room.status`. After a game started (`status === 'in-game'`), all players were still marked ready so `allReady` remained true and the "Start Game" button stayed enabled. A host clicking it again sent another `start-game` message to the server. The server-side guard (Bug 2 fix) prevents the actual double-start, but the UI was misleading.  
**Fix:** Wrapped the pre-game action row in `room.status === 'waiting'`. A standalone Leave button is displayed for `status !== 'waiting'` states so users can always exit.  
**Regression risk:** Low — only cosmetic/control flow change. All server-side guards remain in place.

---

## Files Changed

| File | Change |
|---|---|
| `apps/desktop/src/context/LobbyContext.tsx` | Clear room/relay/chat state on WS `close`; clear chat in `leaveRoom` |
| `apps/lobby-server/src/lobby-manager.ts` | `startGame` status guard; slot renumbering in `leaveRoom`; uppercase normalisation in `joinByCode` / `joinByCodeAsSpectator` |
| `apps/desktop/src/pages/LobbyPage.tsx` | Gate ready/start-game actions on `room.status === 'waiting'`; add standalone Leave for in-game state |
| `docs/status/debug-pass.md` | This file |

---

## Highest-Risk Remaining Issues

1. **No reconnect-to-in-game recovery.** If a player disconnects mid-game, the server removes them (correct per current design). On reconnect they cannot rejoin a `status === 'in-game'` room as a player — they can only spectate. A full rejoin flow would require a `rejoin-game` message type, a grace period during which the player slot is reserved, and client-side relay re-negotiation.

2. **`getRoomsForPlayer` ignores spectator rooms.** The `spectatorsByRoom` pre-snapshot in `handler.ts` `ws.on('close')` is populated by `getRoomsForPlayer` which only iterates `room.players`. Spectators in rooms that have no remaining players after cleanup don't receive a `room-left` broadcast. In practice spectators only exist in active rooms (players still present), so the real-world impact is minimal, but the assumption is fragile.

3. **In-memory relay port pool exhaustion.** If sessions are started in rapid succession and rooms are never explicitly cleaned up (e.g., all players disconnect simultaneously and `leaveRoom` is not called), TCP server instances are left open. `deallocateSession` correctly handles this path, but any crash of the lobby server without a graceful shutdown leaks all TCP ports (9000–9200) until the OS reclaims them.

4. **Single-player rooms can start immediately.** `startGame` only requires `allReady && status === 'waiting'`. A solo host (1 player) is marked `ready` by default at room creation, so `allReady` is true the instant they create a room. This allows an accidental one-player session start with no warning.

5. **Save conflict resolution is UI-only.** `resolveConflict` in `save-service.ts` updates `syncStatus` to `pending-upload` / `pending-download` but there is no actual network sync triggered. `completePendingSync` is a simulation helper. Production use requires wiring to a real cloud storage API.

---

## Recommended Next Fixes

1. **Rejoin-game flow** — `rejoin-game` message + server-side player slot reservation with a TTL (e.g., 30 s).
2. **`getRoomsForPlayer` to include spectator rooms** — rename to `getRoomsForParticipant` and union `room.spectators`.
3. **Solo-player start guard** — warn or block `startGame` when `room.players.length < 2` (configurable per template).
4. **Relay graceful shutdown** — `SIGTERM` handler on the lobby server that calls `deallocateSession` for all active sessions.
5. **Cloud sync integration** — replace `completePendingSync` placeholder with real API calls.
