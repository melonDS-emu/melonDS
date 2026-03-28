# RetroOasis Relay Host Deployment Guide

This guide explains how to run a production-ready RetroOasis lobby + relay server.

---

## Quick Start (Docker Compose)

The easiest way to run the full stack is with the provided `docker-compose.yml`:

```bash
# Copy the example env file and edit as needed
cp .env.example .env

# Start the stack
docker compose up -d

# View logs
docker compose logs -f lobby
```

---

## Environment Variables

All configuration is passed via environment variables.  None are required — the
server runs out-of-the-box with sensible defaults.

### Core

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `8080` | HTTP + WebSocket listen port |
| `DB_PATH` | *(unset — in-memory)* | Path to SQLite database file. When unset the server uses in-memory stores (data lost on restart). Set to e.g. `/var/data/retro-oasis.db` for persistence. |

### Relay

| Variable | Default | Description |
|----------|---------|-------------|
| `RELAY_HOST` | `localhost` | Public hostname or IP address sent to clients in `game-starting` events. Players' emulators will connect to this address. Set to your server's public IP or domain name. |
| `RELAY_PORT_MIN` | `9000` | Lowest TCP port in the relay pool. |
| `RELAY_PORT_MAX` | `9200` | Highest TCP port in the relay pool (inclusive). |
| `RELAY_BIND` | `0.0.0.0` | Interface the relay TCP sockets bind to. Use `127.0.0.1` to restrict to loopback (if a reverse proxy handles external traffic). |

---

## Docker Compose Configuration

The `docker-compose.yml` at the repository root runs the lobby server and
persists the database in a named volume:

```yaml
services:
  lobby:
    build:
      context: .
      dockerfile: apps/lobby-server/Dockerfile
    ports:
      - "8080:8080"
      - "9000-9200:9000-9200"
    environment:
      PORT: "8080"
      RELAY_HOST: "${RELAY_HOST:-localhost}"
      RELAY_PORT_MIN: "9000"
      RELAY_PORT_MAX: "9200"
      DB_PATH: "/data/retro-oasis.db"
    volumes:
      - lobby-data:/data
    restart: unless-stopped

volumes:
  lobby-data:
```

> **Port range note:** Exposing `9000-9200` maps 201 TCP ports, which Docker
> handles well but may require OS-level tweaks (`net.ipv4.ip_local_port_range`)
> on some hosts.  Reduce the range with `RELAY_PORT_MIN`/`RELAY_PORT_MAX` if
> needed.

---

## Reverse Proxy (nginx)

To terminate TLS and expose the server on the standard HTTPS/WSS port, add an
nginx upstream:

```nginx
upstream retro_oasis {
    server 127.0.0.1:8080;
}

server {
    listen 443 ssl http2;
    server_name retro.example.com;

    ssl_certificate     /etc/letsencrypt/live/retro.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/retro.example.com/privkey.pem;

    # WebSocket + HTTP API
    location / {
        proxy_pass http://retro_oasis;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_read_timeout 86400;
    }
}

# Redirect HTTP → HTTPS
server {
    listen 80;
    server_name retro.example.com;
    return 301 https://$host$request_uri;
}
```

When behind a reverse proxy, set `RELAY_HOST` to the **public** domain name so
clients receive the correct relay address:

```bash
RELAY_HOST=retro.example.com
```

> **Relay TCP ports** (9000–9200) must be accessible **directly** (not via the
> nginx proxy) because they carry raw TCP game traffic, not HTTP.  Open them in
> your firewall/security group and point DNS at the same server.

---

## Building the Docker Image

A minimal Dockerfile for the lobby server:

```dockerfile
FROM node:22-alpine AS build
WORKDIR /app
COPY package*.json ./
COPY apps/lobby-server/package.json apps/lobby-server/
COPY packages/ packages/
RUN npm ci --workspace=@retro-oasis/lobby-server --include-workspace-root

COPY apps/lobby-server/ apps/lobby-server/
COPY packages/ packages/

FROM node:22-alpine
WORKDIR /app
COPY --from=build /app/node_modules ./node_modules
COPY --from=build /app/apps/lobby-server ./apps/lobby-server
COPY --from=build /app/packages ./packages
COPY --from=build /app/package.json .

ENV NODE_ENV=production
CMD ["node", "--import", "tsx/esm", "apps/lobby-server/src/index.ts"]
```

---

## Health Check

The server does not expose a dedicated `/health` endpoint yet.  A simple HTTP
check against any known endpoint (e.g. `GET /api/systems`) works:

```bash
curl -f http://localhost:8080/api/systems
```

---

## SQLite Persistence

When `DB_PATH` is set, the server creates a SQLite database (WAL mode) at that
path on first boot.  The schema is applied automatically via `CREATE TABLE IF
NOT EXISTS` statements, so upgrades are generally safe without a migration step
for additive changes.

Backed tables:

| Table | Contents |
|-------|----------|
| `players` | Persistent player identities |
| `rooms` | Active and closed lobby rooms |
| `room_players` | Player membership per room |
| `room_spectators` | Spectator membership per room |
| `sessions` | Session history records |
| `session_players` | Per-session player display names |
| `saves` | Save-file blobs with versioning |
| `friends` | Accepted friendships (symmetric) |
| `friend_requests` | Pending / accepted / declined requests |
| `matchmaking_queue` | Players waiting for a match |

**Backup**: Copy the `.db` file while the server is idle, or use SQLite's
online backup API (`sqlite3 retro-oasis.db ".backup /path/to/backup.db"`).

---

## Phase 8 API Reference

### Friends

| Method | Path | Body / Query | Description |
|--------|------|------|-------------|
| `GET` | `/api/friends` | `?userId=<id>` | List friends |
| `POST` | `/api/friends/add` | `{userId, friendId}` | Send a friend request |
| `DELETE` | `/api/friends/:friendId` | `?userId=<id>` | Remove a friend |
| `GET` | `/api/friends/requests` | `?userId=<id>` | Pending incoming requests |
| `POST` | `/api/friends/requests/:requestId/accept` | `{userId}` | Accept a request |
| `POST` | `/api/friends/requests/:requestId/decline` | `{userId}` | Decline a request |

### Matchmaking

| Method | Path | Body / Query | Description |
|--------|------|------|-------------|
| `POST` | `/api/matchmaking/join` | `{playerId, displayName, gameId, gameTitle, system, maxPlayers}` | Join queue |
| `DELETE` | `/api/matchmaking/leave` | `?playerId=<id>` | Leave queue |
| `GET` | `/api/matchmaking` | — | Current queue (debug) |

### Player Identity

| Method | Path | Body / Query | Description |
|--------|------|------|-------------|
| `POST` | `/api/identity` | `{identityToken, displayName, updateName?}` | Register/look up identity |
| `GET` | `/api/identity/:playerId` | — | Get identity by stable player ID |

### Session Stats (Phase 8 enhancements)

| Method | Path | Query | Description |
|--------|------|-------|-------------|
| `GET` | `/api/sessions/stats/most-played` | `?limit=10` | Top games by session count |
| `GET` | `/api/sessions/stats/total-time` | — | Total accumulated play time (seconds) |
| `GET` | `/api/sessions/stats/player/:displayName` | — | Per-player session stats |

---

## WebSocket Messages (Phase 8)

### Client → Server

| Type | Payload | Description |
|------|---------|-------------|
| `register-identity` | `{identityToken, displayName}` | Link WebSocket session to a persistent identity |
| `friend-request` | `{toPlayerId}` | Send a friend request |
| `friend-request-accept` | `{requestId}` | Accept an incoming request |
| `friend-request-decline` | `{requestId}` | Decline an incoming request |
| `friend-remove` | `{friendId}` | Remove a friend |
| `matchmaking-join` | `{gameId, gameTitle, system, maxPlayers, displayName}` | Join matchmaking queue |
| `matchmaking-leave` | *(none)* | Leave matchmaking queue |

### Server → Client

| Type | Payload | Description |
|------|---------|-------------|
| `identity-confirmed` | `{persistentId, displayName}` | Identity registered/resolved |
| `friend-request-received` | `{requestId, fromId, fromDisplayName}` | Someone sent you a request |
| `friend-request-accepted` | `{requestId, byId, byDisplayName}` | Your request was accepted |
| `friend-request-declined` | `{requestId}` | Your request was declined |
| `friend-status-update` | `{friendId, status, roomCode?, gameTitle?}` | Friend came online/offline/joined game |
| `matchmaking-queued` | `{position}` | Joined the queue at this position |
| `match-found` | `{room}` | Match found — auto-created room details |
