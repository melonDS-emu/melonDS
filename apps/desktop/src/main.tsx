import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { BrowserRouter } from 'react-router-dom';
import { App } from './App';
import { LobbyProvider } from './context/LobbyContext';
import { PresenceProvider } from './context/PresenceContext';
import { ToastProvider } from './context/ToastContext';
import './index.css';

window.addEventListener('error', (event) => {
  console.error('[app] Uncaught error:', event.error ?? event.message);
});

window.addEventListener('unhandledrejection', (event) => {
  console.error('[app] Unhandled promise rejection:', event.reason);
});

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <BrowserRouter>
      <ToastProvider>
        <LobbyProvider>
          <PresenceProvider>
            <App />
          </PresenceProvider>
        </LobbyProvider>
      </ToastProvider>
    </BrowserRouter>
  </StrictMode>
);
