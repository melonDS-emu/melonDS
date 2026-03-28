# RetroOasis UX Design Guide

## Design Philosophy

RetroOasis should feel like a **cozy social game room**, not a technical emulator tool.

### Do
- Use big, clear cards with rounded corners
- Show system badges with platform colors
- Use obvious call-to-action buttons ("Host a Room", "Ready Up")
- Keep animations subtle and playful
- Show player counts and multiplayer modes prominently
- Use warm, inviting colors
- Make the lobby feel like a physical room
- Use emoji and friendly copy

### Don't
- Expose emulator internals to the user
- Use dense settings panels
- Require technical knowledge to start a session
- Use terminal/developer aesthetics
- Overwhelm with options

## Color Palette

| Token | Hex | Usage |
|-------|-----|-------|
| `--oasis-bg` | `#1a1025` | Main background |
| `--oasis-surface` | `#241535` | Sidebar, secondary surfaces |
| `--oasis-card` | `#2e1d42` | Cards, panels |
| `--oasis-accent` | `#7c5cbf` | Primary actions, highlights |
| `--oasis-accent-light` | `#a78bfa` | Links, headings, active states |
| `--oasis-green` | `#34d399` | Ready, success, online |
| `--oasis-yellow` | `#fbbf24` | Host badge, warnings |
| `--oasis-red` | `#f87171` | Errors, disconnect |
| `--oasis-text` | `#f0e6ff` | Primary text |
| `--oasis-text-muted` | `#9a8bb3` | Secondary text, labels |

## System Colors

Each Nintendo system has a signature color for badges:

| System | Color | Hex |
|--------|-------|-----|
| NES | Red | `#E60012` |
| SNES | Purple | `#7B5EA7` |
| GB | Olive | `#8B956D` |
| GBC | Slate Blue | `#6A5ACD` |
| GBA | Indigo | `#4B0082` |
| N64 | Green | `#009900` |
| NDS | Silver | `#A0A0A0` |

## UI Copy Guidelines

Use friendly, game-like language:

| Instead of | Use |
|------------|-----|
| "Create Session" | "Host a Room" |
| "Connect to Server" | "Join a Room" |
| "Set Ready State" | "Ready Up" |
| "Multiplayer Games" | "Party Picks" |
| "Link Cable Session" | "Link Battle" / "Trade Room" |
| "Recent Activity" | "Recently Played" |
| "Configure" | "Settings" (only when necessary) |

## Compatibility Badges

Display these badges on game cards:

- 🟢 **Great Online** — Excellent online multiplayer experience
- 🔵 **Good Local** — Best for local/couch play
- 🤝 **Best with Friends** — Designed for friend groups
- 🔗 **Link Mode** — Link cable or wireless connectivity
- 🧪 **Experimental** — May have issues
- 👑 **Host Only Recommended** — Best when one person hosts
- 👆 **Touch Heavy** — Requires touch input (NDS)
- 🎉 **Party Favorite** — Top pick for party play

## Key Screens

### Home
- Hero: "Host a Room" + "Join a Room" buttons
- Joinable lobbies list
- Quick Party Picks (curated games)
- Recently Played

### Library
- System filter pills
- Tag filter pills (Party, Co-op, Versus, etc.)
- Game grid with cards
- Each card shows: emoji art, system badge, player count, tags

### Game Details
- Large cover area
- System badge + player count
- Description
- Tags and compatibility badges
- "Host Lobby" + "Invite Friends" actions

### Lobby Room
- Room name + room code
- Player list with slots
- Ready state per player
- Host badge
- "Ready Up" action
- "Leave" action
- Empty slots shown as dashed outlines
