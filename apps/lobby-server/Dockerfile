FROM node:22-alpine AS deps
WORKDIR /app

# Copy workspace manifests for dependency caching
COPY package.json package-lock.json ./
COPY apps/lobby-server/package.json apps/lobby-server/
COPY packages/emulator-bridge/package.json packages/emulator-bridge/
COPY packages/game-db/package.json packages/game-db/
COPY packages/multiplayer-profiles/package.json packages/multiplayer-profiles/
COPY packages/presence-client/package.json packages/presence-client/
COPY packages/save-system/package.json packages/save-system/
COPY packages/session-engine/package.json packages/session-engine/
COPY packages/shared-types/package.json packages/shared-types/
COPY packages/ui/package.json packages/ui/

RUN npm ci --workspace=@retro-oasis/lobby-server --include-workspace-root

# Copy source
COPY apps/lobby-server/ apps/lobby-server/
COPY packages/ packages/

# ---------------------------------------------------------------------------
FROM node:22-alpine
WORKDIR /app

COPY --from=deps /app/node_modules ./node_modules
COPY --from=deps /app/apps/lobby-server ./apps/lobby-server
COPY --from=deps /app/packages ./packages
COPY --from=deps /app/package.json .

# Create data directory for SQLite
RUN mkdir -p /data

ENV NODE_ENV=production

EXPOSE 8080
# Relay TCP ports are declared in docker-compose.yml

CMD ["node", "--import", "tsx/esm", "apps/lobby-server/src/index.ts"]
