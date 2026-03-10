export interface MockGame {
  id: string;
  title: string;
  system: string;
  systemColor: string;
  maxPlayers: number;
  tags: string[];
  badges: string[];
  description: string;
  coverEmoji: string;
}

export const MOCK_GAMES: MockGame[] = [
  {
    id: 'snes-super-bomberman',
    title: 'Super Bomberman',
    system: 'SNES',
    systemColor: '#7B5EA7',
    maxPlayers: 4,
    tags: ['4P', 'Party', 'Versus', 'Battle'],
    badges: ['Party Favorite', 'Great Online'],
    description: 'The ultimate party battle game. Up to 4 players in explosive arena action.',
    coverEmoji: '💣',
  },
  {
    id: 'n64-mario-kart-64',
    title: 'Mario Kart 64',
    system: 'N64',
    systemColor: '#009900',
    maxPlayers: 4,
    tags: ['4P', 'Versus', 'Party'],
    badges: ['Party Favorite', 'Great Online'],
    description: 'Iconic 4-player kart racing. The definitive N64 party game.',
    coverEmoji: '🏎️',
  },
  {
    id: 'n64-super-smash-bros',
    title: 'Super Smash Bros.',
    system: 'N64',
    systemColor: '#009900',
    maxPlayers: 4,
    tags: ['4P', 'Versus', 'Battle'],
    badges: ['Party Favorite', 'Great Online'],
    description: 'The original platform fighter. 4-player chaos at its finest.',
    coverEmoji: '⚔️',
  },
  {
    id: 'n64-mario-party-2',
    title: 'Mario Party 2',
    system: 'N64',
    systemColor: '#009900',
    maxPlayers: 4,
    tags: ['4P', 'Party', 'Co-op'],
    badges: ['Party Favorite', 'Best with Friends'],
    description: 'Board game fun with dozens of mini-games for 4 players.',
    coverEmoji: '🎲',
  },
  {
    id: 'nds-mario-kart-ds',
    title: 'Mario Kart DS',
    system: 'NDS',
    systemColor: '#A0A0A0',
    maxPlayers: 8,
    tags: ['4P', 'Versus', 'Battle'],
    badges: ['Great Online', 'Party Favorite'],
    description: 'The best portable Mario Kart. Race with up to 8 players.',
    coverEmoji: '🏁',
  },
  {
    id: 'gba-pokemon-emerald',
    title: 'Pokémon Emerald',
    system: 'GBA',
    systemColor: '#4B0082',
    maxPlayers: 2,
    tags: ['2P', 'Trade', 'Battle', 'Link'],
    badges: ['Link Mode', 'Best with Friends'],
    description: 'Trade, battle, and explore the Battle Frontier with friends.',
    coverEmoji: '🐉',
  },
  {
    id: 'snes-tetris-attack',
    title: 'Tetris Attack',
    system: 'SNES',
    systemColor: '#7B5EA7',
    maxPlayers: 2,
    tags: ['2P', 'Versus'],
    badges: ['Great Online', 'Best with Friends'],
    description: 'Fast-paced competitive puzzle action. Perfect for quick matches.',
    coverEmoji: '🧩',
  },
  {
    id: 'nes-contra',
    title: 'Contra',
    system: 'NES',
    systemColor: '#E60012',
    maxPlayers: 2,
    tags: ['2P', 'Co-op'],
    badges: ['Good Local', 'Best with Friends'],
    description: 'Classic run-and-gun co-op action. Grab a friend and save the world.',
    coverEmoji: '🔫',
  },
  {
    id: 'gb-tetris',
    title: 'Tetris',
    system: 'GB',
    systemColor: '#8B956D',
    maxPlayers: 2,
    tags: ['2P', 'Versus', 'Link'],
    badges: ['Great Online', 'Link Mode'],
    description: 'The classic puzzle game with link cable versus mode.',
    coverEmoji: '🟦',
  },
  {
    id: 'gbc-pokemon-crystal',
    title: 'Pokémon Crystal',
    system: 'GBC',
    systemColor: '#6A5ACD',
    maxPlayers: 2,
    tags: ['2P', 'Trade', 'Battle', 'Link'],
    badges: ['Link Mode', 'Best with Friends'],
    description: 'The definitive Gen II experience. Trade and battle with friends.',
    coverEmoji: '💎',
  },
  {
    id: 'snes-secret-of-mana',
    title: 'Secret of Mana',
    system: 'SNES',
    systemColor: '#7B5EA7',
    maxPlayers: 3,
    tags: ['Co-op', '2P'],
    badges: ['Good Local', 'Best with Friends'],
    description: 'Action RPG with up to 3-player co-op.',
    coverEmoji: '⚔️',
  },
  {
    id: 'nds-tetris-ds',
    title: 'Tetris DS',
    system: 'NDS',
    systemColor: '#A0A0A0',
    maxPlayers: 4,
    tags: ['4P', 'Versus', 'Party'],
    badges: ['Great Online', 'Party Favorite'],
    description: 'Nintendo-themed Tetris with online and local wireless multiplayer.',
    coverEmoji: '🎮',
  },
];

export interface MockLobby {
  id: string;
  name: string;
  host: string;
  gameTitle: string;
  system: string;
  systemColor: string;
  playerCount: number;
  maxPlayers: number;
  status: string;
}

export const MOCK_LOBBIES: MockLobby[] = [
  {
    id: 'lobby-1',
    name: "Mario Kart Party!",
    host: 'Player2',
    gameTitle: 'Mario Kart 64',
    system: 'N64',
    systemColor: '#009900',
    playerCount: 2,
    maxPlayers: 4,
    status: 'waiting',
  },
  {
    id: 'lobby-2',
    name: 'Smash Night',
    host: 'LinkMaster',
    gameTitle: 'Super Smash Bros.',
    system: 'N64',
    systemColor: '#009900',
    playerCount: 3,
    maxPlayers: 4,
    status: 'waiting',
  },
  {
    id: 'lobby-3',
    name: 'Pokémon Trades',
    host: 'RetroFan',
    gameTitle: 'Pokémon Emerald',
    system: 'GBA',
    systemColor: '#4B0082',
    playerCount: 1,
    maxPlayers: 2,
    status: 'waiting',
  },
];
