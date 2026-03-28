/**
 * localStorage-backed settings for RetroAchievements.org credentials.
 * Credentials are stored locally in plain text — appropriate for a desktop
 * app where localStorage is only accessible by the local user. If this were
 * a web app, server-side encryption or a secure vault (e.g., Electron's
 * safeStorage) would be recommended for the API token.
 */

const RA_USERNAME_KEY = 'ra-username';
const RA_TOKEN_KEY = 'ra-token';
const RA_HARDCORE_KEY = 'ra-hardcore';

export interface RetroAchievementsCredentials {
  username: string;
  token: string;
  hardcoreMode: boolean;
}

export function getRACredentials(): RetroAchievementsCredentials {
  return {
    username: localStorage.getItem(RA_USERNAME_KEY) ?? '',
    token: localStorage.getItem(RA_TOKEN_KEY) ?? '',
    hardcoreMode: localStorage.getItem(RA_HARDCORE_KEY) === 'true',
  };
}

export function setRACredentials(creds: Partial<RetroAchievementsCredentials>): void {
  if (creds.username !== undefined) localStorage.setItem(RA_USERNAME_KEY, creds.username);
  if (creds.token !== undefined) localStorage.setItem(RA_TOKEN_KEY, creds.token);
  if (creds.hardcoreMode !== undefined) localStorage.setItem(RA_HARDCORE_KEY, String(creds.hardcoreMode));
}

export function clearRACredentials(): void {
  localStorage.removeItem(RA_USERNAME_KEY);
  localStorage.removeItem(RA_TOKEN_KEY);
  localStorage.removeItem(RA_HARDCORE_KEY);
}

/** Returns true if both username and token are stored. */
export function isRAConnected(): boolean {
  const { username, token } = getRACredentials();
  return username.length > 0 && token.length > 0;
}
