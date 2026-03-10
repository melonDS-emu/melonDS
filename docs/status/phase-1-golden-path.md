# Phase 1 — Golden Path Stabilization

**Goal:** Make the core multiplayer flow dependable and repeatable:

```
open app → choose game → host room → friend joins → ready up → launch → return cleanly
```

---

## What Was Fixed

### Server (`apps/lobby-server`)

| Area | Fix |
|------|-----|
| **Relay cleanup** | `leave-room` and player disconnect now call `relay.deallocateSession()` when the last player leaves a room, freeing TCP ports 9000–9200 and preventing port exhaustion |
| **Spectator notifications** | When the last player disconnects and a room is deleted, spectators now receive a `room-left` message so their UI clears correctly |
| **In-game state broadcast** | `start-game` now emits `room-updated` (with `status: 'in-game'` and `relayPort`) before `game-starting`, so clients that reload can rehydrate the correct room state |
| **Join error copy** | The join-room failure message now explains the three common reasons (wrong code, room full, game already started) instead of a bare "Could not join room" |

### Frontend (`apps/desktop`)

| Area | Fix |
|------|-----|
| **Single-player start** | `allReady` check changed from `players.length > 1` to `players.length >= 1` so a solo host can start for testing |
| **In-game room state** | `LobbyContext` now updates `currentRoom.status` and `currentRoom.relayPort` on the `game-starting` event, keeping local state consistent with server state |
| **Join loading state** | The "Joining lobby…" screen now shows any server error (e.g. wrong code) so the user isn't stuck waiting forever |
| **Relay banner copy** | Banner updated from technical emulator-address copy to plain "Session Live — Everyone's in!" language; relay endpoint details remain in the Connection Diagnostics panel |
| **Host hint** | When the host is ready but other players are not, a "Waiting for all players to ready up…" hint appears under the Start Game button |

---

## QA Checklist — Host / Join / Launch / Cleanup

Run through this checklist manually before each release cut.

### 1. Host Creates a Room
- [ ] Open the app — home page loads, lobby server status shows "connected"
- [ ] Click **Host a Room** → fill in your name and pick a game → click **Create Room**
- [ ] You are taken to the lobby page for the new room
- [ ] Your name appears in the player list as **HOST** with a green **✓ Ready** badge
- [ ] A 6-character room code is visible in the top-right corner
- [ ] Clicking the room code copies it and shows **✓ Copied!** for 2 seconds

### 2. Guest Joins by Room Code
- [ ] In a second browser window (or tab), click **Join a Room**
- [ ] Enter the room code and a different display name → click **Join Room**
- [ ] Guest is taken to the same lobby page
- [ ] Both the host and guest see each other's names in the player list
- [ ] Guest starts with **Not Ready** badge; host starts with **✓ Ready**

### 3. Ready States Are Accurate
- [ ] Guest clicks **✓ Ready Up** → both players see the badge change to **✓ Ready**
- [ ] Host sees the "Waiting for all players to ready up…" hint disappear once guest readies
- [ ] Guest clicks **⏸ Unready** → badge reverts to **Not Ready** for both players
- [ ] The **▶ Start Game** button is only enabled (not greyed out) when every player is ready

### 4. Host Starts the Session
- [ ] With all players ready, host clicks **▶ Start Game**
- [ ] Both players see the green **🎮 Session Live — Everyone's in!** banner
- [ ] Both players see the room status reflected (no longer shows "Not Ready" state)
- [ ] Connection Diagnostics panel shows the relay endpoint (host/port)
- [ ] Guest sees the same relay endpoint as the host

### 5. Disconnect and Cleanup
- [ ] **Guest disconnects**: Host's player list updates automatically; host can continue; room stays open
- [ ] **Host disconnects while guest is in room**: Guest sees the player list update; someone is promoted to host (host migration); room stays open
- [ ] **Last player leaves (or host disconnects from a 1-player room)**: Room is deleted; relay port is freed (verify via server logs `[relay] Session … closed`)
- [ ] **Spectator in a deleted room**: Spectator is returned to the home page (no infinite "Joining lobby…" screen)

### 6. Error Handling
- [ ] Enter a wrong room code → error message explains "The code may be wrong, the room may be full, or the game has already started"
- [ ] Join a full room → same helpful error message
- [ ] Lobby server is offline → home page shows "⚠️ Offline — lobby server unreachable"
- [ ] Server reconnects → home page auto-recovers to "connected" state within a few seconds

### 7. Return Cleanly
- [ ] Click **Leave** from the lobby page → you are returned to the home page
- [ ] Public room list on the home page no longer shows the room you just left (or it shows as having one fewer player)
- [ ] Reopening the app after a full disconnect does not show a stale room

---

## Known Gaps (Not Blocking Golden Path)

- Emulator does not launch automatically; users must start their emulator manually and point it at the relay address shown in Connection Diagnostics.
- Room state is in-memory on the server; restarting the server discards all rooms.
- Friend invite flow (sharing room code outside the app) is not yet implemented.
- Game library uses mock data; actual ROM scanning is not yet wired up.
