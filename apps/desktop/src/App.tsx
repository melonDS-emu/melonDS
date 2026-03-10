import { Routes, Route } from 'react-router-dom';
import { Layout } from './components/Layout';
import { HomePage } from './pages/HomePage';
import { GameDetailsPage } from './pages/GameDetailsPage';
import { LobbyPage } from './pages/LobbyPage';
import { LibraryPage } from './pages/LibraryPage';

export function App() {
  return (
    <Routes>
      <Route element={<Layout />}>
        <Route path="/" element={<HomePage />} />
        <Route path="/library" element={<LibraryPage />} />
        <Route path="/game/:gameId" element={<GameDetailsPage />} />
        <Route path="/lobby/:lobbyId" element={<LobbyPage />} />
      </Route>
    </Routes>
  );
}
