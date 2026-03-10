# API Migration Checklist

Tracks which screens and components have been moved off in-line mock data
and are now wired through the typed `GameApiClient` (in `mock` mode by default).

Set `VITE_API_MODE=backend` and `VITE_BACKEND_URL=<url>` to point every screen
at a live Fastify/HTTP backend without changing any component code.

## Data-fetching screens

| Screen / Component     | Before                    | After                         | Status |
|------------------------|---------------------------|-------------------------------|--------|
| `HomePage`             | `MOCK_GAMES` (inline)     | `useGames()`                  | ✅ Done |
| `LibraryPage`          | `MOCK_GAMES` (inline filter) | `useGames({ system, tag })` | ✅ Done |
| `GameDetailsPage`      | `MOCK_GAMES.find()`       | `useGame(id)`                 | ✅ Done |
| `HostRoomModal`        | `MOCK_GAMES` (dropdown)   | `useGames()`                  | ✅ Done |
| `GameCard`             | typed `MockGame`          | typed `ApiGame`               | ✅ Done |

## Supporting infrastructure

| Deliverable                                  | Status |
|----------------------------------------------|--------|
| `src/types/api.ts` — canonical API types     | ✅ Done |
| `src/lib/api-client.ts` — `GameApiClient`    | ✅ Done |
| `src/lib/use-games.ts` — React hooks         | ✅ Done |
| Mock / backend / hybrid mode switching       | ✅ Done |
| Retry + fallback on backend calls            | ✅ Done |
| Smoke tests (`api-client.test.ts`, 14 tests) | ✅ Done |

## Remaining (out of scope for Phase 1)

| Item                                          | Notes |
|-----------------------------------------------|-------|
| Live lobbies (`/api/lobbies`)                 | Already served via WebSocket — REST endpoint can wrap `lobby-manager` if a Fastify backend is added |
| Providers / systems endpoint (`/api/systems`) | Implemented in client; backend endpoint needs Fastify route |
| User history / "Recently Played"              | Requires persistence layer (DB or local storage) |
| Search page (dedicated route)                 | `apiClient.searchGames()` is ready; UI route not yet added |
| Cloud-backed game catalog                     | `backend` mode ready; Fastify routes TBD |

## How to switch modes

```bash
# Always use bundled mock data (default, no backend required)
VITE_API_MODE=mock npm run dev:desktop

# Always call the remote HTTP backend
VITE_API_MODE=backend VITE_BACKEND_URL=http://localhost:3001 npm run dev:desktop

# Try backend, fall back to mock on network error
VITE_API_MODE=hybrid VITE_BACKEND_URL=http://localhost:3001 npm run dev:desktop
```

## Backend contract

When `VITE_API_MODE=backend`, the client expects the following REST endpoints:

| Method | Path                        | Returns        |
|--------|-----------------------------|----------------|
| GET    | `/api/games`                | `ApiGame[]`    |
| GET    | `/api/games?system=&tag=&maxPlayers=&search=` | `ApiGame[]` |
| GET    | `/api/games/:id`            | `ApiGame`      |
| GET    | `/api/games/search?q=`      | `ApiGame[]`    |
| GET    | `/api/systems`              | `string[]`     |

All types are defined in `apps/desktop/src/types/api.ts`.
