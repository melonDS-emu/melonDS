# RetroOasis Development Roadmap

## Phase 1 — Foundation (Complete)

**Goal:** Polished multiplayer-first retro Nintendo app for simpler systems.

### Systems
- NES, SNES, GB, GBC, GBA

### Milestones
- [x] Monorepo structure
- [x] Core TypeScript domain types
- [x] Game catalog with multiplayer seed data (30 games)
- [x] Emulator bridge interfaces and backend definitions
- [x] Save system scaffold (local + cloud models)
- [x] Session engine scaffold with Phase 1 default templates
- [x] Presence client scaffold
- [x] Multiplayer profiles + input management
- [x] React frontend scaffold with mocked data
- [x] WebSocket lobby server prototype
- [x] Architecture and UX documentation
- [x] WebSocket-connected lobby in frontend (host/join rooms, ready state, chat)
- [x] Host a Room modal with game selection
- [x] Join by Room Code modal (with Spectate option)
- [x] Lobby page with real-time player list, ready-up, host controls, and chat
- [x] Disconnect cleanup (rooms closed when host disconnects)
- [x] Host migration (host transferred on disconnect)
- [x] Room code one-click copy with visual confirmation
- [x] Spectator mode (join-as-spectator, spectator list in lobby)
- [x] Ping/pong heartbeat with live latency display
- [x] Connection quality indicators per player
- [x] Netplay TCP relay server (ports 9000–9200, room-scoped sessions)
- [x] Real child_process emulator launching (SIGTERM stop)
- [x] Emulator netplay args per backend (FCEUX, Snes9x, mGBA, Mupen64Plus, melonDS)
- [x] ROM file scanner (by extension, recursive, grouped by system)
- [x] Save file real I/O (fs/promises create, delete, import, export)
- [x] Relay port surfaced to frontend on game-starting event
- [ ] Tauri integration for native desktop app
- [ ] ROM scanning UI in library page
- [ ] Controller mapping UI
- [ ] Cloud save sync implementation
- [ ] Friend invite flow (room code sharing via external messaging)

## Phase 2 — N64 + Enhanced Social

**Goal:** Add N64 support with party game focus, plus richer social features.

### Systems
- Add: Nintendo 64

### Milestones
- [x] N64 session templates (Mupen64Plus, 2P + 4P defaults, MK64, Smash Bros presets)
- [x] Netplay relay supports N64 packet forwarding
- [x] SameBoy backend added (GB/GBC — best-in-class accuracy + link cable via TCP)
- [x] VisualBoyAdvance-M (VBA-M) backend added (GB/GBC/GBA — link cable via TCP)
- [x] System adapters updated with backend-specific link cable args (SameBoy, VBA-M)
- [x] GB Pokémon Gen I catalog entries (Red, Blue, Yellow — link/trade)
- [x] GBC link game catalog entries (Zelda Oracle of Ages, Oracle of Seasons)
- [x] GBA Pokémon Gen III catalog entries (Ruby, Sapphire, LeafGreen — link/trade)
- [x] GBA Four Swords catalog entry (4-player link co-op)
- [x] Session templates for all new Pokémon link/trade games
- [x] Session template for Zelda Four Swords 4P
- [x] N64 backend integration: Mupen64Plus install/detection notes + analog calibration guidance
- [x] N64 playerSlot support in adapter (--netplay-player arg)
- [x] N64 default input profiles: Xbox, PlayStation, keyboard (analog stick mappings)
- [x] Session templates: Mario Party 2, GoldenEye 007, Diddy Kong Racing (4P)
- [x] Mock game catalog expanded with Mario Party 2, GoldenEye 007, Diddy Kong Racing
- [x] "Best 4P" badge on party game cards (N64 4-player games)
- [x] Party hint banners on game detail pages (N64-specific messaging)
- [x] N64 party spotlight section on home page
- [x] Game summary card in lobby with party context and system badge
- [x] Open slots invite nudge in lobby (share room code prompt)
- [x] Connection diagnostics UI
- [x] Spectator mode (Phase 1 carry-over) ✅
- [ ] Tauri integration for native desktop app (Phase 1 carry-over)
- [ ] Controller mapping UI (bind N64_DEFAULT_PROFILES to UI)
- [ ] Voice chat hooks
- [ ] Party activity feed

## Phase 3 — Nintendo DS + Premium Features

**Goal:** Add DS support as a premium differentiator with unique UX.

### Systems
- Add: Nintendo DS

### Milestones
- [x] melonDS session templates (2P + 4P defaults, WFC/Pokémon presets)
- [x] WFC config in session templates (Wiimmfi DNS server)
- [x] NDS Pokémon WFC presets (Diamond, Pearl, Platinum, HGSS, Mario Kart DS)
- [x] Dual-screen layout options in session templates
- [x] melonDS netplay args in system adapter (--wifi-host / --wifi-port)
- [ ] melonDS integration (this repository's core — C++ build + IPC bridge)
- [ ] Dual-screen layout controls UI (stacked, side-by-side, focus modes)
- [ ] Touch input mapping (mouse/touchpad)
- [ ] DS wireless-inspired room UX
- [ ] Advanced compatibility badges
- [ ] Screen swap hotkeys

## Future Ideas
- Tournament-style rooms
- Seasonal featured games
- Ranked/casual matchmaking tags
- Custom room themes
- Achievement system
- Game clip sharing
- Mobile companion app
