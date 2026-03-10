# Phase 5 — Presence and Social Discovery

**Goal:** Make RetroOasis feel alive even before a session starts. Surface friends, recent activity, and joinable rooms so the home screen is immediately inviting.

## Status: Complete ✅

---

## What Was Built

### 1. `packages/presence-client` — Enriched Data Model

- Added `roomCode` field to `FriendInfo` so joinable friends can surface their room code directly in the UI.
- Added `RecentActivity` type capturing join/start/finish/online events with timestamp, player, game title, and optional room code.
- Exported `ActivityEventType` and `OnlineStatus` as standalone types for reuse.
- Added `seedMockFriends()` method to `PresenceClient` — seeds 6 realistic friends (in-game / online / idle / offline) for development and demo use.

### 2. `apps/desktop/src/context/PresenceContext.tsx` — New React Context

- `PresenceProvider` initialises a `PresenceClient`, seeds mock friends, and derives `recentActivity` items from their state.
- Exposes `friends`, `onlineFriends`, `joinableSessions`, and `recentActivity` to any component.
- Mounted in `main.tsx` inside `LobbyProvider`.

### 3. `apps/desktop/src/components/Layout.tsx` — Live Sidebar

| Before | After |
|--------|-------|
| Static hardcoded friend stubs | Dynamic list from `PresenceContext` |
| No actions on friends | Hover-reveal **Join** button for joinable sessions |
| No indication of online count | Online count badge on **Friends** nav item + footer count |
| No Friends nav item | **👥 Friends** added to sidebar nav |

The Join button opens `JoinRoomModal` pre-filled with the friend's room code. Navigation to the lobby happens via a `useEffect` once `currentRoom` is set.

### 4. `apps/desktop/src/components/JoinRoomModal.tsx` — `initialCode` Prop

Added optional `initialCode?: string` prop so the modal can be opened pre-filled from a friend's room code. Fully backwards-compatible.

### 5. `apps/desktop/src/pages/HomePage.tsx` — Social Home Sections

Two new sections appear between **Joinable Lobbies** and **N64 Party Games**:

**👥 Friends Playing Now**
- Renders a card for each friend with `isJoinable = true`.
- Each card shows the friend's avatar initial, display name, current game, and a **▶ Join** button.
- Links to `/friends` for the full list.

**⚡ Recent Activity**
- Shows the 4 most recent activity events.
- Each row: emoji icon · "Player2 joined Mario Kart 64" · relative timestamp.
- Joinable events show an inline **Join** button.
- Links to `/friends` for the full feed.

### 6. `apps/desktop/src/pages/FriendsPage.tsx` — New Social Discovery Page

Route: `/friends`

Sections:
- **Header** — summary counts (in game · online · idle).
- **🚀 Open Rooms from Friends** — spotlight cards for every joinable session, showing avatar, name, game title, room code, and **▶ Join**.
- **Filter tabs** — All / Online / In Game with live counts.
- **Friends list** — full roster with avatar, status dot + label, "Playing X" subtitle, and join button for open rooms.
- **⚡ Recent Activity** — full activity feed with inline join buttons.

### 7. `apps/desktop/src/App.tsx` — Route Added

`/friends` → `<FriendsPage />`

---

## Acceptance Criteria Review

| Criterion | Met |
|-----------|-----|
| App feels active and social | ✅ Friends sidebar + home sections surface activity immediately |
| Users can quickly see join opportunities | ✅ Joinable sessions on home page, sidebar hover-join, Friends page spotlight |
| Presence supports hosting/joining rather than distracting | ✅ Social surfaces are secondary to hero actions; clean, compact layout |

---

## Mock Data (Development)

`PresenceClient.seedMockFriends()` provides:

| Friend | Status | Game | Joinable |
|--------|--------|------|----------|
| Player2 | 🟢 In Game | Mario Kart 64 | ✅ `MK6401` |
| LinkMaster | 🟢 In Game | Super Smash Bros. | ❌ (full) |
| StarFox99 | 🟢 In Game | Mario Party 2 | ✅ `PARTY2` |
| RetroFan | 🔵 Online | — | ❌ |
| ZeldaQueen | 🟡 Idle | — | ❌ |
| Bomber_X | ⚫ Offline | — | ❌ |

---

## Future Work

- Connect `PresenceClient` to the lobby WebSocket server so friend presence is live rather than mocked.
- Add a friend-request / add-friend flow.
- Persist display name and friend list server-side.
- Push notifications when a joinable session opens.
- Activity persistence (server-side event log).
