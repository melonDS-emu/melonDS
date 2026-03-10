import { WebSocketServer } from 'ws';
import { LobbyManager } from './lobby-manager';
import { NetplayRelay } from './netplay-relay';
import { handleConnection } from './handler';

const PORT = parseInt(process.env.PORT ?? '8080', 10);
const wss = new WebSocketServer({ port: PORT });
const lobbyManager = new LobbyManager();
const netplayRelay = new NetplayRelay();

wss.on('connection', (ws) => {
  handleConnection(ws, lobbyManager, netplayRelay);
});

wss.on('listening', () => {
  console.log(`RetroOasis Lobby Server running on ws://localhost:${PORT}`);
  console.log(`Netplay relay available on TCP ports 9000-9200`);
});
