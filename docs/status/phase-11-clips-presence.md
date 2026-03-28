# Phase 11 — Clip Library, Social Sharing & Live Presence

> **Status:** Complete  
> **Completed:** 2026-03-12

## Goals

Build a full in-app clip library with playback and management; upgrade presence to use live server-side data; wire live friend requests.

## What Was Built

### 🎬 Clip Library (`/clips`)

- **`ClipsPage.tsx`** — full grid UI at `/clips` route
  - Clips grouped by game with a count badge
  - Search/filter by game title or tag
  - Empty state when no clips recorded
- **In-page video player** (`ClipPlayerModal`) — `<video>` element loaded from IndexedDB blob URL, autoplay, fullscreen support (F key or button)
- **Clip thumbnail generation** — `generateClipThumbnail()` in `clip-service.ts`; seeks to 50% of clip duration via `<video>` + canvas; falls back to 🎬 emoji if the browser cannot decode the blob
- **Clip tagging** — `tag?: string` field added to `ClipMeta`; `updateClipTag()` added to `clip-service.ts`; in-place tag editor on each clip card (Enter to save, Escape to cancel)
- **Actions**: Play, Export `.webm`, Copy shareable link, Tag, Delete — all available from a ⋯ overflow menu
- **Nav item** `🎬 Clips` added to the sidebar in `Layout.tsx`

### 👥 Live Friend Presence

- **`PresenceContext`** now listens to `friendStatuses` from `LobbyContext` (Phase 8 `friend-status-update` WS push) and overlays live status on top of the mock friend list
- Friends are correctly reflected as `online`, `in-game`, or `offline` based on real server events
- When no live data has been received the mock data is used as-is (graceful fallback)

### 📬 Live Friend Request UI

- **`lobby-types.ts`** extended with Phase 8 client messages (`register-identity`, `friend-request`, `friend-request-accept`, `friend-request-decline`, `friend-remove`) and server messages (`friend-request-received`, `friend-request-accepted`, `friend-request-declined`, `friend-status-update`, `identity-registered`)
- **`LobbyContext`** handles all new WS messages, exposes:
  - `pendingFriendRequests` — inbound requests awaiting response
  - `friendStatuses` — live status updates from the server
  - Actions: `registerIdentity`, `sendFriendRequest`, `acceptFriendRequest`, `declineFriendRequest`, `removeFriend`
- **`FriendsPage`** updated with:
  - "Add Friend" input — send a friend request by player ID
  - "Friend Requests" section — accept or decline pending inbound requests
- **Notification badge** — red badge on the Friends nav item showing the count of pending inbound friend requests

### 🏆 Tournament SQLite Persistence

- **`SqliteTournamentStore`** — drop-in replacement for the in-memory `TournamentStore`, following the `SqliteSessionHistory` pattern
- Bracket generation, BYE auto-advance, and result recording all fully persisted across restarts
- `getPlayerTournaments(displayName)` method for per-player history
- **`db.ts`** schema extended with `tournaments`, `tournament_players`, and `tournament_matches` tables
- **`index.ts`** now creates `SqliteTournamentStore` when `DB_PATH` is set; `tournamentStore.recordResult` null-check fixed for the union type

### 📊 Tournament History in Profile

- `fetchPlayerTournaments(displayName)` added to `stats-service.ts`
- `GET /api/tournaments/player/:displayName` endpoint added to the lobby server (SQLite only)
- **`ProfilePage`** shows a "Tournament History" grid: trophy icon for wins, medal for completions, game & player count, date

## Known Limitations

- `friend-status-update` broadcasts the friendId but not the display name, so newly discovered live friends show their ID as a placeholder display name until a display-name lookup is implemented
- `GET /api/tournaments/player/:displayName` returns 501 when `DB_PATH` is not set (in-memory mode doesn't have `getPlayerTournaments`)
- Clip thumbnail generation requires a browser environment that can play `.webm` video; no thumbnail is shown in headless/test contexts

## Files Changed

| File | Change |
|------|--------|
| `apps/desktop/src/lib/clip-service.ts` | Added `tag` field to `ClipMeta`, `updateClipTag()`, `generateClipThumbnail()` |
| `apps/desktop/src/pages/ClipsPage.tsx` | **New** — full clip library page |
| `apps/desktop/src/App.tsx` | Added `/clips` route |
| `apps/desktop/src/components/Layout.tsx` | Added Clips nav item + friend request badge |
| `apps/desktop/src/services/lobby-types.ts` | Phase 8 client + server message types |
| `apps/desktop/src/context/LobbyContext.tsx` | Friend WS handling + new actions/state |
| `apps/desktop/src/context/PresenceContext.tsx` | Live presence overlay from `friendStatuses` |
| `apps/desktop/src/pages/FriendsPage.tsx` | Add Friend input + Pending Requests section |
| `apps/desktop/src/lib/stats-service.ts` | `fetchPlayerTournaments()` + `PlayerTournamentEntry` |
| `apps/desktop/src/pages/ProfilePage.tsx` | Tournament History section |
| `apps/lobby-server/src/db.ts` | Tournament tables added to schema |
| `apps/lobby-server/src/sqlite-tournament-store.ts` | **New** — SQLite tournament store |
| `apps/lobby-server/src/index.ts` | Wire `SqliteTournamentStore`; `/api/tournaments/player/:name` endpoint; null-check fix |
| `docs/mvp/roadmap.md` | Phase 11 milestones marked complete |
