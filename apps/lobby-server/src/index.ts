import { WebSocketServer } from 'ws';
import { LobbyManager } from './lobby-manager';
import { handleConnection } from './handler';

const PORT = parseInt(process.env.PORT ?? '8080', 10);
const wss = new WebSocketServer({ port: PORT });
const lobbyManager = new LobbyManager();

wss.on('connection', (ws) => {
  handleConnection(ws, lobbyManager);
});

wss.on('listening', () => {
  console.log(`RetroOasis Lobby Server running on ws://localhost:${PORT}`);
});
