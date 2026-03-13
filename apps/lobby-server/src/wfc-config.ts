/**
 * WFC provider definitions for Nintendo DS online play.
 *
 * These providers act as DNS servers that point the NDS Wi-Fi stack at
 * community-run replacements for Nintendo's discontinued Wi-Fi Connection
 * service (shut down May 2014).
 *
 * RetroOasis auto-configures the selected provider when launching a
 * WFC-enabled DS session — no manual DNS setup required.
 */

export interface WfcProvider {
  /** Stable identifier used in session templates and API payloads. */
  id: string;
  /** Display name shown in UI. */
  name: string;
  /** Primary DNS server IP passed to melonDS via --wfc-dns. */
  dnsServer: string;
  /** Short human-readable description. */
  description: string;
  /** Project home page (for display only). */
  url: string;
  /**
   * List of gameIds that this provider explicitly supports.
   * Empty array means the provider claims broad NDS WFC compatibility.
   */
  supportsGames: string[];
}

/** All supported WFC providers, in preference order. */
export const WFC_PROVIDERS: WfcProvider[] = [
  {
    id: 'wiimmfi',
    name: 'Wiimmfi',
    dnsServer: '178.62.43.212',
    description:
      'The largest Nintendo WFC replacement server. Supports Pokémon, Mario Kart DS, Metroid Prime Hunters, and hundreds more DS and Wii titles.',
    url: 'https://wiimmfi.de',
    supportsGames: [],
  },
  {
    id: 'altwfc',
    name: 'AltWFC',
    dnsServer: '172.104.88.237',
    description:
      'Alternative WFC server with broad DS game support. Good fallback if Wiimmfi is unreachable, and useful for games not yet registered on Wiimmfi.',
    url: 'https://altwfc.com',
    supportsGames: [],
  },
];

/** Look up a provider by its stable ID. Returns null if not found. */
export function getWfcProvider(id: string): WfcProvider | null {
  return WFC_PROVIDERS.find((p) => p.id === id) ?? null;
}

/** Default provider ID used when no override is specified. */
export const DEFAULT_WFC_PROVIDER_ID = 'wiimmfi';
