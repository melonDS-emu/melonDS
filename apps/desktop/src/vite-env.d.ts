/// <reference types="vite/client" />

interface ImportMetaEnv {
  /** WebSocket URL for the lobby server. Defaults to ws://localhost:8080 */
  readonly VITE_WS_URL?: string;
  /** Base URL of the local launch API. Defaults to http://localhost:8080 */
  readonly VITE_LAUNCH_API_URL?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
