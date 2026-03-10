# MVP Phase 1 — Simpler Systems

## Goal

A polished multiplayer-first retro Nintendo app for simpler systems (NES, SNES, GB, GBC, GBA).

## Supported Systems

- NES
- SNES
- Game Boy
- Game Boy Color
- Game Boy Advance

## Features

### Game Library
- [x] Local game catalog with multiplayer metadata
- [x] System and tag filtering
- [x] Cover art / emoji placeholders
- [x] Multiplayer categorization (Party, Co-op, Versus, Link, Trade, Battle)
- [ ] Local ROM scanning
- [ ] Favorites and recently played persistence

### Lobby System
- [x] Create public/private rooms
- [x] Join rooms by ID or room code
- [x] Player slots and ready states
- [x] Host designation
- [x] Room code display (click-to-copy)
- [x] Real-time player list updates via WebSocket
- [x] Disconnect cleanup (room removed when host leaves)
- [x] Host Room modal with game selection
- [x] Join by Room Code modal
- [x] Room chat
- [ ] Friend invite flow
- [ ] Connection quality indicators

### Session Templates
- [x] Template data model
- [x] Per-game default templates (NES, SNES, GB, GBC, GBA + popular titles)
- [x] Auto-save rules per template
- [ ] Controller mapping presets UI

### Save System
- [x] Local save path management
- [x] Cloud sync data model
- [ ] Actual file I/O integration
- [ ] Import/export UI
- [ ] Conflict resolution UI

### Social
- [x] Friend presence data model
- [x] Online status display
- [ ] WebSocket-connected presence
- [ ] Invite notifications

### Input
- [x] Input profile data model
- [ ] Keyboard mapping UI
- [ ] Controller detection
- [ ] Hot-plugging support

## Target Experience

A user should be able to:
1. Open RetroOasis
2. See multiplayer game recommendations
3. Select a game → see its multiplayer details
4. Host a lobby → share room code ✅
5. Friend joins → both ready up ✅
6. Game launches with preconfigured settings
