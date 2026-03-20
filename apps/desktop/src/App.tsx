import { Routes, Route } from 'react-router-dom';
import { Layout } from './components/Layout';
import { HomePage } from './pages/HomePage';
import { GameDetailsPage } from './pages/GameDetailsPage';
import { LobbyPage } from './pages/LobbyPage';
import { LibraryPage } from './pages/LibraryPage';
import { SavesPage } from './pages/SavesPage';
import { FriendsPage } from './pages/FriendsPage';
import { SettingsPage } from './pages/SettingsPage';
import { ProfilePage } from './pages/ProfilePage';
import { TournamentsPage } from './pages/TournamentsPage';
import { ClipsPage } from './pages/ClipsPage';
import { PokemonPage } from './pages/PokemonPage';
import { MarioKartPage } from './pages/MarioKartPage';
import { MarioSportsPage } from './pages/MarioSportsPage';
import { ZeldaPage } from './pages/ZeldaPage';
import { MetroidPage } from './pages/MetroidPage';
import { EventsPage } from './pages/EventsPage';
import { CommunityPage } from './pages/CommunityPage';
import { GlobalChatPage } from './pages/GlobalChatPage';
import { NotificationsPage } from './pages/NotificationsPage';
import { RetroAchievementsPage } from './pages/RetroAchievementsPage';
import WiiPage from './pages/WiiPage';
import GameCubePage from './pages/GameCubePage';
import GenesisPage from './pages/GenesisPage';
import DreamcastPage from './pages/DreamcastPage';
import PSXPage from './pages/PSXPage';
import PS2Page from './pages/PS2Page';
import PSPPage from './pages/PSPPage';

export function App() {
  return (
    <Routes>
      <Route element={<Layout />}>
        <Route path="/" element={<HomePage />} />
        <Route path="/library" element={<LibraryPage />} />
        <Route path="/game/:gameId" element={<GameDetailsPage />} />
        <Route path="/lobby/:lobbyId" element={<LobbyPage />} />
        <Route path="/saves" element={<SavesPage />} />
        <Route path="/friends" element={<FriendsPage />} />
        <Route path="/settings" element={<SettingsPage />} />
        <Route path="/profile" element={<ProfilePage />} />
        <Route path="/tournaments" element={<TournamentsPage />} />
        <Route path="/clips" element={<ClipsPage />} />
        <Route path="/pokemon" element={<PokemonPage />} />
        <Route path="/mario-kart" element={<MarioKartPage />} />
        <Route path="/mario-sports" element={<MarioSportsPage />} />
        <Route path="/zelda" element={<ZeldaPage />} />
        <Route path="/metroid" element={<MetroidPage />} />
        <Route path="/wii" element={<WiiPage />} />
        <Route path="/gc" element={<GameCubePage />} />
        <Route path="/genesis" element={<GenesisPage />} />
        <Route path="/dreamcast" element={<DreamcastPage />} />
        <Route path="/psx" element={<PSXPage />} />
        <Route path="/ps2" element={<PS2Page />} />
        <Route path="/psp" element={<PSPPage />} />
        <Route path="/events" element={<EventsPage />} />
        <Route path="/community" element={<CommunityPage />} />
        <Route path="/chat" element={<GlobalChatPage />} />
        <Route path="/notifications" element={<NotificationsPage />} />
        <Route path="/retro-achievements" element={<RetroAchievementsPage />} />
      </Route>
    </Routes>
  );
}
