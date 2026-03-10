# RetroOasis Development Roadmap

## Phase 1 — Foundation (Current)

**Goal:** Polished multiplayer-first retro Nintendo app for simpler systems.

### Systems
- NES, SNES, GB, GBC, GBA

### Milestones
- [x] Monorepo structure
- [x] Core TypeScript domain types
- [x] Game catalog with multiplayer seed data (30 games)
- [x] Emulator bridge interfaces and backend definitions
- [x] Save system scaffold (local + cloud models)
- [x] Session engine scaffold
- [x] Presence client scaffold
- [x] Multiplayer profiles + input management
- [x] React frontend scaffold with mocked data
- [x] WebSocket lobby server prototype
- [x] Architecture and UX documentation
- [ ] Tauri integration for native desktop app
- [ ] ROM scanning and library management
- [ ] Actual emulator process launching via bridge
- [ ] WebSocket-connected lobby in frontend
- [ ] Controller mapping UI
- [ ] Local save file I/O
- [ ] Cloud save sync implementation
- [ ] Friend invite flow

## Phase 2 — N64 + Enhanced Social

**Goal:** Add N64 support with party game focus, plus richer social features.

### Systems
- Add: Nintendo 64

### Features
- [ ] N64 backend integration (Mupen64Plus)
- [ ] 4-player party game optimization
- [ ] Analog stick mapping quality
- [ ] Spectator mode
- [ ] Voice chat hooks
- [ ] Host migration
- [ ] Connection diagnostics
- [ ] Party activity feed
- [ ] Latency indicators for N64 games

## Phase 3 — Nintendo DS + Premium Features

**Goal:** Add DS support as a premium differentiator with unique UX.

### Systems
- Add: Nintendo DS

### Features
- [ ] melonDS integration (this repository's core)
- [ ] Dual-screen layout controls (stacked, side-by-side, focus modes)
- [ ] Touch input mapping (mouse/touchpad)
- [ ] DS wireless-inspired room UX
- [ ] Link/Battle/Trade session presets for DS games
- [ ] Advanced compatibility badges
- [ ] Screen swap hotkeys
- [ ] DS-specific session templates

## Future Ideas
- Tournament-style rooms
- Seasonal featured games
- Ranked/casual matchmaking tags
- Custom room themes
- Achievement system
- Game clip sharing
- Mobile companion app
