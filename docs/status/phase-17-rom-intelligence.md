# Phase 17 — ROM Intelligence, Global Chat & Notification Center

**Status:** ✅ Complete  
**Date:** 2026-03-19

---

## Summary

Phase 17 added three major improvements to RetroOasis:

1. Levenshtein-based fuzzy ROM title matching replacing the fragile slug heuristic
2. A global lobby chat channel so all connected players can talk in real time
3. An in-app notification center that aggregates server-side events per player

---

## Milestones

### ✅ 1. Fuzzy ROM title matching

**File:** `apps/desktop/src/lib/rom-fuzzy-match.ts`

`fuzzyMatchGameId(system, rawTitle, catalog)` uses a rolling-array Levenshtein
distance implementation (no new npm dependencies) to find the closest game
catalog entry for a scanned ROM.  Both strings are normalised (lowercase,
non-alphanumeric stripped to spaces, whitespace collapsed) before comparison.
A match is only accepted when the edit distance is less than
`0.4 × max(len_a, len_b)` — this 40 % threshold rejects clearly wrong matches.

`LibraryPage` now imports `fuzzyMatchGameId` and calls it against the full
`useGames()` catalog instead of the old `inferGameId` slug generator.  The
fetch for the full catalog is an additional `useGames()` call without filters,
so it runs in parallel with the filtered view the user actually sees.

---

### ✅ 2. Global lobby chat

**Server:** `apps/lobby-server/src/global-chat-store.ts`

`GlobalChatStore` is an in-memory ring buffer capped at 500 messages.
`post(playerId, displayName, text)` appends and evicts the oldest entry when
full.  `getRecent(limit?)` returns the most recent N messages (default 100).

**WebSocket protocol:**

| Direction     | Type                  | Description                                              |
|---------------|-----------------------|----------------------------------------------------------|
| client→server | `send-global-chat`    | `{ type, text }` — broadcast to all connected clients    |
| server→client | `global-chat-message` | `{ type, id, playerId, displayName, text, timestamp }`   |

**REST:**

| Method | Path               | Description                                  |
|--------|--------------------|----------------------------------------------|
| GET    | `/api/chat?limit=N`| Returns `{ messages: GlobalChatMessage[] }`  |

**Desktop:** `GlobalChatPage` (`/chat`) fetches the recent message history on
mount, subscribes to `global-chat-message` WS events for real-time updates,
and sends via `ws.send({ type: 'send-global-chat', text })`.  Messages from
the current player appear right-aligned (bubble style).

---

### ✅ 3. Notification center

**Server:** `apps/lobby-server/src/notification-store.ts`

`NotificationStore` keeps a per-player list (max 200 entries, evicting oldest)
with `add`, `list`, `markRead`, `markAllRead`, and `unreadCount` methods.

Notification types:

| Type                   | Triggered by                              |
|------------------------|-------------------------------------------|
| `achievement-unlocked` | `pushAchievementUnlocks()` in handler     |
| `friend-request`       | `send friend-request` WS message          |
| `dm-received`          | `send-dm` WS message to an online player  |

**REST:**

| Method | Path                                            | Description               |
|--------|-------------------------------------------------|---------------------------|
| GET    | `/api/notifications/:playerId`                  | List all notifications     |
| POST   | `/api/notifications/:playerId/read`             | Mark all read              |
| POST   | `/api/notifications/:playerId/read/:notifId`    | Mark one notification read |

**Desktop:** `NotificationsPage` (`/notifications`) groups notifications into
Unread / Earlier sections, shows a type icon per entry, and has a "Mark all
read" button.  The Layout sidebar polls `GET /api/notifications/:playerId`
every 30 seconds and shows an unread count badge on the 🔔 nav item.

---

## Tests Added

**File:** `apps/lobby-server/src/__tests__/phase-17.test.ts`

| Suite               | Tests | Coverage highlights                              |
|---------------------|-------|--------------------------------------------------|
| `GlobalChatStore`   | 6     | post, getRecent limit, eviction, clear           |
| `NotificationStore` | 12    | add, list, markRead, markAllRead, unreadCount, max-per-player cap |

Total new tests: **18**  
Prior test count: **271**  
New total: **289**

---

## Known Limitations

- `GlobalChatStore` is in-memory only; messages are lost on server restart.
  A SQLite-backed variant (`SqliteGlobalChatStore`) can be added following the
  `SqliteGameRatingsStore` pattern when persistence is needed.
- `NotificationStore` is also in-memory; a SQLite variant is deferred.
- The notification badge in `Layout` polls every 30 s via REST rather than
  receiving a server push; a WebSocket push for `notification-created` events
  can be added in a future phase.
- Friend-accepted notifications are not yet auto-generated (only friend-request
  is wired); this can be added to the `friend-request-accept` handler.
