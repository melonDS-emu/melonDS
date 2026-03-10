import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { BrowserRouter } from 'react-router-dom';
import { App } from './App';
import { LobbyProvider } from './context/LobbyContext';
import { PresenceProvider } from './context/PresenceContext';
import './index.css';

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <BrowserRouter>
      <LobbyProvider>
        <PresenceProvider>
          <App />
        </PresenceProvider>
      </LobbyProvider>
    </BrowserRouter>
  </StrictMode>
);
