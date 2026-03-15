# Phase 14 — Zelda & Metroid Online + Direct Messaging

**Status:** ✅ Complete  
**Date:** 2026-03-15

---

## Summary

Phase 14 adds two new game-specific multiplayer hub pages for the Zelda and Metroid franchises, and introduces in-app direct messaging between friends.

---

## Features Delivered

### Zelda Multiplayer Hub (`/zelda`)

A tabbed hub page covering three play modes:

| Tab | Games | Notes |
|-----|-------|-------|
| Co-op Rooms | GBA Four Swords 4P & 2P, GBC Oracle Ages/Seasons 2P | Online relay for GBA; link cable p2p for GBC |
| Battle Mode | NDS Phantom Hourglass 2P | Requires Wiimmfi — auto-configured by RetroOasis |
| Leaderboard | Global session leaderboard | Top 10 by session count |

WFC auto-config banner explains Wiimmfi setup for Phantom Hourglass battle rooms.

### Metroid Hunters Lobby (`/metroid`)

Three-tab page for Metroid Prime Hunters online:

| Tab | Description |
|-----|-------------|
| Online Matches | Browse/host up to 4P deathmatch rooms; WFC provider switcher |
| Quick Match | One-click find-or-create a 1v1 match; auto-queues if no room available |
| Rankings | Global leaderboard + all 7 playable hunters (Samus, Kanden, Spire, Trace, Noxus, Sylux, Weavel) |

### Direct Messaging

Real-time DMs between friends, fully integrated with the WebSocket connection:

**Server side:**
- `MessageStore` — in-memory DM store (zero-config default)
- `SqliteMessageStore` — persistent SQLite-backed store (activated by `DB_PATH` env var)
- `direct_messages` table with indexes on `(from_player, to_player)` and `(to_player, read_at)`
- REST endpoints:
  - `GET /api/messages/:player` — recent conversations for a player
  - `GET /api/messages/:player1/:player2` — conversation thread
  - `GET /api/messages/:player/unread-count` — total unread count
  - `POST /api/messages/send` — send a new DM
  - `POST /api/messages/read` — mark conversation as read
- WebSocket events:
  - `send-dm` (client → server) — send a DM
  - `dm-received` (server → client) — real-time push to recipient
  - `mark-dm-read` (client → server) — mark messages read
  - `dm-read-ack` (server → sender) — acknowledge read receipt

**Client side (`FriendsPage`):**
- Friends list doubles as DM inbox — click any friend to open their thread
- Unread count badge per friend in the list
- Message thread with chat bubble UI (own messages right-aligned, others left-aligned)
- Real-time delivery: incoming `dm-received` WS events update the open thread immediately
- Toast notification for incoming DMs (via `ToastContext`)
- `💬` badge on the Friends nav item when unread messages are present

### Game Catalog & Session Templates

New entries added to `seed-data.ts`:
- **Metroid Prime Hunters** (NDS) — 4P online deathmatch, Wiimmfi-supported
- **Zelda: Phantom Hourglass** (NDS) — 2P Battle Mode, Wiimmfi-supported
- **Zelda: Spirit Tracks** (NDS) — 2P local co-op

New session templates added to `templates.ts`:
- `nds-metroid-prime-hunters-2p` — quick 1v1 variant (Wiimmfi, 80ms latency target)
- `gba-zelda-four-swords-2p` — 2-player Four Swords variant (online relay, 100ms)

---

## Architecture Notes

- `MessageStore` follows the same in-memory / SQLite-backed pattern as `SaveStore` / `AchievementStore`
- DM routing uses the `displayNameToPlayerId` map already maintained by `handler.ts` for achievement pushes
- The `send-dm` WS handler resolves the sender's display name from the reverse map
- `SqliteMessageStore.hydrate()` pre-loads rows from DB into the in-memory map on startup

---

## Known Limitations

- DM history loaded from the REST API on thread open — messages sent before the REST fetch will appear when scrolling back
- No message delivery receipts beyond `dm-read-ack`
- Zelda/Metroid leaderboards are global (not game-specific)
- No push notifications for DMs when the app is minimised (browser Notification API is separate from the in-app toast system)
- NDS Zelda Four Swords Adventures (GCN–GBA cable) not supported (requires GameCube + GBA hardware)
