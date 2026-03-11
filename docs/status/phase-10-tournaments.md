# Phase 10 — Tournaments & Clip Sharing

> **Status: Complete** (implemented 2026-03-11)

## What Was Built

### Server — Tournament Engine (`tournament-store.ts`)

- **`TournamentStore`** — in-memory single-elimination bracket engine.
- Supports 2–16 players; pads to the next power of two with automatic BYE advancement.
- Round structure: round 1 = first round played, final = `log2(size)`.
- **`create(params)`** — builds bracket, seeds players, auto-advances BYEs.
- **`recordResult(tournamentId, matchId, winner)`** — marks match complete, advances winner to next round slot, propagates BYE chains, sets `status: 'completed'` and `winner` when the final match is decided.

### Server — REST Endpoints (`index.ts`)

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/tournaments` | List all tournaments (newest first) |
| `POST` | `/api/tournaments` | Create a bracket (`name`, `gameId`, `gameTitle`, `system`, `players[]`) |
| `GET` | `/api/tournaments/:id` | Get full tournament (bracket + standings) |
| `POST` | `/api/tournaments/:id/matches/:matchId/result` | Record match result (`winner`) |

### Server — WebSocket Push

- `tournament-updated` message (`{ type: 'tournament-updated', tournamentId }`) is broadcast to all connected clients whenever a match result is recorded.
- `achievement-unlocked` messages are pushed when a tournament winner earns a new achievement.

### Server — Tournament Achievements (`achievement-store.ts`)

Three new achievement definitions added to the `tournaments` category:

| ID | Name | Condition |
|----|------|-----------|
| `first-blood` | First Blood | Win your first tournament match |
| `champion` | Champion | Win a tournament |
| `dynasty` | Dynasty | Win 3 tournaments |

`checkTournamentAchievements(playerId, displayName, wins, matchWins)` method added to both `AchievementStore` and `SqliteAchievementStore`.

### Frontend — Tournaments Page (`/tournaments`)

- Create new bracket form: name, game, system, player list (one per line, min 2).
- Tournament list (card per tournament with system badge and status dot).
- Bracket visualiser: single-elimination tree view, round labels (Quarter-finals / Semi-finals / Final).
- Match cards show both players; completed matches show winner with trophy and loser with strikethrough.
- "Report Result" button on each eligible match opens a modal to select the winner.
- Standings panel: win count per player, sorted descending.
- Live reload via `tournament-updated` WebSocket messages.

### Frontend — Clip Service (`clip-service.ts`)

- `startClipRecording(stream, opts)` / `stopClipRecording(handle)` — MediaRecorder-based clip capture.
- Clips stored as `Blob` in **IndexedDB** (`retro-oasis-clips` DB); metadata in `localStorage`.
- `listClipMeta(gameId?)` — fast metadata listing without loading blobs.
- `loadClip(meta)` — resolves to a revocable `blob:` URL for in-page playback.
- `deleteClip(id)` — removes blob + metadata.
- `exportClip(meta)` — triggers browser download as `.webm`.
- `copyClipLink(meta)` — copies `blob:` URL to clipboard.

### Frontend — Home Page Widget

- "🎬 Recent Clips" section appears when 1+ clips exist, showing the 3 most recent clips with game title, duration, and capture date.

### Frontend — Navigation

- "🏆 Tournaments" nav item added to the sidebar between Friends and Saves.
- `/tournaments` route registered in `App.tsx`.

## Test Coverage

- `src/__tests__/tournament-store.test.ts` — 18 tests covering creation, BYE advancement, result recording, winner propagation, error cases, and the count helper.

## Known Limitations

- `TournamentStore` is in-memory only; a `SqliteTournamentStore` following the `SqliteSessionHistory` pattern is straightforward to add when persistence is needed.
- Clip recording requires a `MediaStream` source (e.g. `canvas.captureStream()`); the emulator canvas must be accessible to the browser page.  Until the Tauri IPC bridge is wired up, clips can be captured from any browser `MediaStream` the user provides.
- `blob:` URLs for clip sharing are session-scoped (revoked on page close); a server-side upload endpoint would be needed for true persistent sharing.
