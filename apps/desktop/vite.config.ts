import { defineConfig } from 'vitest/config';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';

// When running inside the Tauri dev server, the host and port are configured
// to match tauri.conf.json → build.devUrl.
const isTauri = !!process.env.TAURI_ENV_PLATFORM;

export default defineConfig({
  plugins: [react(), tailwindcss()],

  // Tauri expects a fixed dev server URL so the webview can connect.
  server: {
    host: isTauri ? '0.0.0.0' : 'localhost',
    port: 5173,
    strictPort: true,
  },

  // Prevent Vite from obscuring Rust errors in the Tauri dev console.
  clearScreen: false,

  // Environment variables prefixed with TAURI_ are exposed to the frontend.
  envPrefix: ['VITE_', 'TAURI_'],

  test: {
    environment: 'node',
    include: ['src/**/*.test.ts'],
  },
});
