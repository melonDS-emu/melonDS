/**
 * WFC provider definitions for Nintendo DS and Wii online play.
 *
 * These providers act as DNS servers that point the NDS/Wii Wi-Fi stack at
 * community-run replacements for Nintendo's discontinued Wi-Fi Connection
 * service (shut down May 2014).
 *
 * RetroOasis auto-configures the selected provider when launching a
 * WFC-enabled DS or Wii session — no manual DNS setup required.
 */

export interface WfcProvider {
  /** Stable identifier used in session templates and API payloads. */
  id: string;
  /** Display name shown in UI. */
  name: string;
  /** Primary DNS server IP passed to melonDS/Dolphin via --wfc-dns. */
  dnsServer: string;
  /** Short human-readable description. */
  description: string;
  /** Project home page (for display only). */
  url: string;
  /**
   * Whether this provider also supports Wii WFC titles (not just DS).
   * When true, it is shown as an option on the Wii Online section.
   */
  supportsWii: boolean;
  /**
   * List of gameIds that this provider explicitly supports.
   * Empty array means the provider claims broad WFC compatibility.
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
      'The largest Nintendo WFC revival service, running since 2014 when Nintendo shut down the original Wi-Fi Connection. ' +
      'Supports Pokémon, Mario Kart DS/Wii, Metroid Prime Hunters, Super Smash Bros. Brawl, and hundreds more DS and Wii titles.',
    url: 'https://wiimmfi.de',
    supportsWii: true,
    supportsGames: [],
  },
  {
    id: 'wiilink-wfc',
    name: 'WiiLink WFC',
    dnsServer: '167.235.229.36',
    description:
      'WiiLink WFC is the Wi-Fi Connection revival service operated by the WiiLink project — a community effort that also restores ' +
      'Wii online channels (Forecast, News, Everybody Votes, and more). Offers modern infrastructure and active ongoing development.',
    url: 'https://www.wiilink24.com',
    supportsWii: true,
    supportsGames: [],
  },
  {
    id: 'altwfc',
    name: 'AltWFC',
    dnsServer: '172.104.88.237',
    description:
      'Alternative WFC server with broad DS game support. Good fallback if Wiimmfi is unreachable, and useful for games not yet registered on Wiimmfi.',
    url: 'https://altwfc.com',
    supportsWii: false,
    supportsGames: [],
  },
];

/** Look up a provider by its stable ID. Returns null if not found. */
export function getWfcProvider(id: string): WfcProvider | null {
  return WFC_PROVIDERS.find((p) => p.id === id) ?? null;
}

/** Default provider ID used when no override is specified. */
export const DEFAULT_WFC_PROVIDER_ID = 'wiimmfi';

/**
 * WiiLink channel services (separate from WFC).
 * These restore Wii online channels that Nintendo shut down, providing
 * weather forecasts, news, voting, and other channel content.
 */
export interface WiiLinkChannel {
  id: string;
  name: string;
  emoji: string;
  description: string;
  /** Whether the channel is currently active on WiiLink. */
  active: boolean;
}

export const WIILINK_CHANNELS: WiiLinkChannel[] = [
  {
    id: 'forecast',
    name: 'Forecast Channel',
    emoji: '🌤️',
    description: 'Real-time weather forecasts for locations worldwide, restored with live data feeds.',
    active: true,
  },
  {
    id: 'news',
    name: 'News Channel',
    emoji: '📰',
    description: 'Worldwide news headlines delivered to your Wii, brought back with current news sources.',
    active: true,
  },
  {
    id: 'everybody-votes',
    name: 'Everybody Votes Channel',
    emoji: '🗳️',
    description: 'Community polls and worldwide voting, fully restored with active question rotations.',
    active: true,
  },
  {
    id: 'check-mii-out',
    name: 'Check Mii Out Channel',
    emoji: '😊',
    description: 'Share and rate Mii characters with players worldwide. Contests and popularity rankings.',
    active: true,
  },
  {
    id: 'nintendo',
    name: 'Nintendo Channel',
    emoji: '🎮',
    description: 'Game demos, trailers, and recommendations. Restored with curated retro gaming content.',
    active: true,
  },
  {
    id: 'demae',
    name: 'Demae Channel',
    emoji: '🍜',
    description: 'Food delivery channel originally Japan-only, now available internationally via WiiLink with custom menus.',
    active: true,
  },
];
