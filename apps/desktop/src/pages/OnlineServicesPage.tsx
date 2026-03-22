import { useState, useEffect } from 'react';

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

interface WfcProvider {
  id: string;
  name: string;
  dnsServer: string;
  description: string;
  url: string;
  supportsWii: boolean;
  supportsGames: string[];
}

interface WiiLinkChannel {
  id: string;
  name: string;
  emoji: string;
  description: string;
  active: boolean;
}

// Fallback data shown when the server is offline
const FALLBACK_PROVIDERS: WfcProvider[] = [
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

const FALLBACK_CHANNELS: WiiLinkChannel[] = [
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

type ActiveTab = 'overview' | 'wiimmfi' | 'wiilink';

// ---------------------------------------------------------------------------
// Provider detail card
// ---------------------------------------------------------------------------

function ProviderCard({ provider }: { provider: WfcProvider }) {
  const accentColor = provider.id === 'wiimmfi' ? '#4a9eff' : provider.id === 'wiilink-wfc' ? '#00c9a7' : '#888';
  return (
    <div style={{ background: '#1a1a2e', borderRadius: 12, padding: 20, border: `1px solid ${accentColor}40`, display: 'flex', flexDirection: 'column', gap: 10 }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
        <div style={{ width: 10, height: 10, borderRadius: '50%', background: accentColor, flexShrink: 0 }} />
        <span style={{ fontWeight: 700, fontSize: 16, color: '#fff' }}>{provider.name}</span>
        {provider.supportsWii && (
          <span style={{ background: '#1a3a2e', border: '1px solid #00c9a740', borderRadius: 4, padding: '2px 7px', fontSize: 11, color: '#00c9a7', marginLeft: 'auto' }}>Wii + DS</span>
        )}
      </div>
      <p style={{ margin: 0, color: '#aaa', fontSize: 13, lineHeight: 1.6 }}>{provider.description}</p>
      <div style={{ display: 'flex', flexWrap: 'wrap', gap: 8, alignItems: 'center' }}>
        <span style={{ background: '#0d0d1a', border: '1px solid #333', borderRadius: 6, padding: '4px 10px', fontSize: 12, color: '#ccc', fontFamily: 'monospace' }}>
          DNS: {provider.dnsServer}
        </span>
        <span style={{ background: '#0d0d1a', border: '1px solid #333', borderRadius: 6, padding: '4px 10px', fontSize: 12, color: '#ccc' }}>
          {provider.url}
        </span>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// WiiLink channel card
// ---------------------------------------------------------------------------

function ChannelCard({ channel }: { channel: WiiLinkChannel }) {
  return (
    <div style={{ background: '#1a1a2e', borderRadius: 10, padding: 14, border: '1px solid #2a2a4a', display: 'flex', gap: 12, alignItems: 'flex-start' }}>
      <span style={{ fontSize: 28, flexShrink: 0 }}>{channel.emoji}</span>
      <div>
        <div style={{ fontWeight: 600, fontSize: 13, color: '#fff', marginBottom: 4 }}>
          {channel.name}
          {channel.active && <span style={{ marginLeft: 8, background: '#0d2a1a', border: '1px solid #00c9a740', borderRadius: 4, padding: '1px 6px', fontSize: 10, color: '#00c9a7' }}>Active</span>}
        </div>
        <div style={{ color: '#888', fontSize: 12, lineHeight: 1.5 }}>{channel.description}</div>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Setup step component
// ---------------------------------------------------------------------------

function SetupStep({ step, title, children }: { step: number; title: string; children: React.ReactNode }) {
  return (
    <div style={{ display: 'flex', gap: 14, alignItems: 'flex-start' }}>
      <div style={{ width: 28, height: 28, borderRadius: '50%', background: 'linear-gradient(135deg, #4a9eff, #7b5ea7)', color: '#fff', fontWeight: 700, fontSize: 13, display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0 }}>
        {step}
      </div>
      <div>
        <div style={{ fontWeight: 600, fontSize: 13, color: '#fff', marginBottom: 4 }}>{title}</div>
        <div style={{ color: '#888', fontSize: 12, lineHeight: 1.6 }}>{children}</div>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Main page
// ---------------------------------------------------------------------------

export default function OnlineServicesPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('overview');
  const [providers, setProviders] = useState<WfcProvider[]>(FALLBACK_PROVIDERS);
  const [channels, setChannels] = useState<WiiLinkChannel[]>(FALLBACK_CHANNELS);

  useEffect(() => {
    fetch(`${API}/api/wfc/providers`)
      .then((r) => r.json())
      .then((data: WfcProvider[]) => setProviders(data))
      .catch(() => setProviders(FALLBACK_PROVIDERS));

    fetch(`${API}/api/wfc/wiilink-channels`)
      .then((r) => r.json())
      .then((data: WiiLinkChannel[]) => setChannels(data))
      .catch(() => setChannels(FALLBACK_CHANNELS));
  }, []);

  const wiimmfi = providers.find((p) => p.id === 'wiimmfi');
  const wiilinkWfc = providers.find((p) => p.id === 'wiilink-wfc');

  const tabs: { id: ActiveTab; label: string; emoji: string }[] = [
    { id: 'overview', label: 'Overview',    emoji: '🌐' },
    { id: 'wiimmfi',  label: 'Wiimmfi',     emoji: '📡' },
    { id: 'wiilink',  label: 'WiiLink',     emoji: '📺' },
  ];

  return (
    <div style={{ padding: '24px 28px', maxWidth: 900, margin: '0 auto' }}>
      {/* Page header */}
      <div style={{ marginBottom: 24 }}>
        <h1 style={{ margin: 0, fontSize: 26, fontWeight: 800, color: '#fff' }}>
          🌐 Online Services
        </h1>
        <p style={{ margin: '6px 0 0', color: '#888', fontSize: 14 }}>
          Community-run replacements for Nintendo Wi-Fi Connection — RetroOasis auto-configures DNS so you can play online without any manual setup.
        </p>
      </div>

      {/* Tab bar */}
      <div style={{ display: 'flex', gap: 6, marginBottom: 24, borderBottom: '1px solid #2a2a2a', paddingBottom: 0 }}>
        {tabs.map((tab) => {
          const active = activeTab === tab.id;
          return (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              style={{
                padding: '8px 16px',
                border: 'none',
                background: 'none',
                cursor: 'pointer',
                fontSize: 13,
                fontWeight: active ? 700 : 400,
                color: active ? '#fff' : '#666',
                borderBottom: active ? '2px solid #e60012' : '2px solid transparent',
                marginBottom: -1,
              }}
            >
              {tab.emoji} {tab.label}
            </button>
          );
        })}
      </div>

      {/* ── Overview Tab ─────────────────────────────────────────────────── */}
      {activeTab === 'overview' && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 24 }}>
          {/* Intro banner */}
          <div style={{ background: 'linear-gradient(135deg, #0d1a2e 0%, #1a0d2e 100%)', borderRadius: 12, padding: '20px 24px', border: '1px solid #2a2a4a' }}>
            <h2 style={{ margin: '0 0 8px', fontSize: 16, fontWeight: 700, color: '#fff' }}>
              What happened to Nintendo Wi-Fi Connection?
            </h2>
            <p style={{ margin: 0, color: '#aaa', fontSize: 13, lineHeight: 1.7 }}>
              Nintendo shut down the Wi-Fi Connection service on <strong style={{ color: '#fff' }}>20 May 2014</strong>, taking online play
              offline for hundreds of DS and Wii titles. The retro gaming community immediately stepped in — community-run servers now
              host the same matchmaking, friend-code, and leaderboard infrastructure that Nintendo used to provide, free to use forever.
            </p>
          </div>

          {/* Provider summary cards */}
          <div>
            <h2 style={{ margin: '0 0 14px', fontSize: 15, fontWeight: 700, color: '#fff' }}>WFC Providers supported by RetroOasis</h2>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
              {providers.map((p) => <ProviderCard key={p.id} provider={p} />)}
            </div>
          </div>

          {/* How it works */}
          <div style={{ background: '#111', borderRadius: 12, padding: '18px 20px', border: '1px solid #222' }}>
            <h2 style={{ margin: '0 0 14px', fontSize: 15, fontWeight: 700, color: '#fff' }}>How RetroOasis sets it up for you</h2>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
              <SetupStep step={1} title="Pick a WFC provider">
                Choose Wiimmfi, WiiLink WFC, or AltWFC from the provider switcher on the Pokémon, Mario Kart, Metroid, or Zelda pages. Wiimmfi is selected by default.
              </SetupStep>
              <SetupStep step={2} title="RetroOasis injects the DNS">
                When you launch a WFC-enabled session, RetroOasis passes{' '}
                <code style={{ background: '#1a1a1a', padding: '1px 4px', borderRadius: 3, fontSize: 11 }}>--wfc-dns &lt;IP&gt;</code>{' '}
                directly to melonDS or Dolphin — no router or system DNS changes required.
              </SetupStep>
              <SetupStep step={3} title="Play online">
                The emulator connects to the community server transparently. Friend codes work just like they did on real hardware. Enjoy!
              </SetupStep>
            </div>
          </div>
        </div>
      )}

      {/* ── Wiimmfi Tab ──────────────────────────────────────────────────── */}
      {activeTab === 'wiimmfi' && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 20 }}>
          {/* Header */}
          <div style={{ background: 'linear-gradient(135deg, #0d1a3a 0%, #0d0d1a 100%)', borderRadius: 12, padding: '20px 24px', border: '1px solid #4a9eff40' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, marginBottom: 10 }}>
              <span style={{ fontSize: 36 }}>📡</span>
              <div>
                <h2 style={{ margin: 0, fontSize: 22, fontWeight: 800, color: '#fff' }}>Wiimmfi</h2>
                <a href={wiimmfi?.url ?? 'https://wiimmfi.de'} target="_blank" rel="noopener noreferrer" style={{ color: '#4a9eff', fontSize: 12, textDecoration: 'none' }}>
                  {wiimmfi?.url ?? 'https://wiimmfi.de'} ↗
                </a>
              </div>
            </div>
            <p style={{ margin: 0, color: '#aaa', fontSize: 13, lineHeight: 1.7 }}>
              {wiimmfi?.description}
            </p>
          </div>

          {/* Key facts */}
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))', gap: 12 }}>
            {[
              { label: 'Founded', value: '2014', emoji: '📅' },
              { label: 'DNS Server', value: wiimmfi?.dnsServer ?? '178.62.43.212', emoji: '🖥️' },
              { label: 'Supported Platforms', value: 'DS + Wii', emoji: '🎮' },
              { label: 'Game Count', value: '2 000+ titles', emoji: '📚' },
            ].map((f) => (
              <div key={f.label} style={{ background: '#111', borderRadius: 10, padding: '14px 16px', border: '1px solid #222', textAlign: 'center' }}>
                <div style={{ fontSize: 24, marginBottom: 6 }}>{f.emoji}</div>
                <div style={{ fontWeight: 700, fontSize: 15, color: '#fff', fontFamily: f.label === 'DNS Server' ? 'monospace' : undefined }}>{f.value}</div>
                <div style={{ color: '#666', fontSize: 11, marginTop: 2 }}>{f.label}</div>
              </div>
            ))}
          </div>

          {/* Notable games */}
          <div style={{ background: '#111', borderRadius: 12, padding: '18px 20px', border: '1px solid #222' }}>
            <h3 style={{ margin: '0 0 14px', fontSize: 14, fontWeight: 700, color: '#fff' }}>Notable supported games</h3>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: 8 }}>
              {[
                'Mario Kart Wii', 'Mario Kart DS', 'Super Smash Bros. Brawl',
                'Pokémon Diamond/Pearl', 'Pokémon HeartGold/SoulSilver',
                'Metroid Prime Hunters', 'Animal Crossing: Wild World',
                'New Super Mario Bros.', 'Tetris DS', 'Mario Strikers Charged',
              ].map((g) => (
                <span key={g} style={{ background: '#1a1a2e', border: '1px solid #4a9eff30', borderRadius: 6, padding: '4px 10px', fontSize: 12, color: '#ccc' }}>{g}</span>
              ))}
            </div>
          </div>

          {/* Wiimmfi setup note */}
          <div style={{ background: '#1a1a0d', borderRadius: 10, padding: '14px 16px', border: '1px solid #4a9eff30', fontSize: 13, color: '#aaa', lineHeight: 1.6 }}>
            💡 <strong style={{ color: '#fff' }}>RetroOasis default.</strong> Wiimmfi is the default WFC provider for all new sessions. The DNS IP{' '}
            <code style={{ background: '#111', padding: '1px 5px', borderRadius: 3, fontSize: 11, color: '#4a9eff' }}>178.62.43.212</code>{' '}
            is injected automatically — no extra configuration needed.
          </div>
        </div>
      )}

      {/* ── WiiLink Tab ──────────────────────────────────────────────────── */}
      {activeTab === 'wiilink' && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 20 }}>
          {/* Header */}
          <div style={{ background: 'linear-gradient(135deg, #0d2a1a 0%, #0d1a2e 100%)', borderRadius: 12, padding: '20px 24px', border: '1px solid #00c9a740' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, marginBottom: 10 }}>
              <span style={{ fontSize: 36 }}>📺</span>
              <div>
                <h2 style={{ margin: 0, fontSize: 22, fontWeight: 800, color: '#fff' }}>WiiLink</h2>
                <a href="https://www.wiilink24.com" target="_blank" rel="noopener noreferrer" style={{ color: '#00c9a7', fontSize: 12, textDecoration: 'none' }}>
                  https://www.wiilink24.com ↗
                </a>
              </div>
            </div>
            <p style={{ margin: 0, color: '#aaa', fontSize: 13, lineHeight: 1.7 }}>
              WiiLink is a community revival project that restores Wii online functions and channels, and adds new content and translations
              that were never available outside Japan. It also operates <strong style={{ color: '#fff' }}>WiiLink WFC</strong> — a modern
              Wi-Fi Connection replacement for Wii and DS online play built on fresh infrastructure.
            </p>
          </div>

          {/* WiiLink WFC */}
          <div>
            <h3 style={{ margin: '0 0 12px', fontSize: 14, fontWeight: 700, color: '#fff' }}>WiiLink WFC — Wi-Fi Connection Revival</h3>
            {wiilinkWfc && <ProviderCard provider={wiilinkWfc} />}
          </div>

          {/* WiiLink key facts */}
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))', gap: 12 }}>
            {[
              { label: 'DNS Server', value: wiilinkWfc?.dnsServer ?? '167.235.229.36', emoji: '🖥️' },
              { label: 'Platforms', value: 'Wii + DS', emoji: '🎮' },
              { label: 'Channels Restored', value: `${channels.length} channels`, emoji: '📺' },
              { label: 'Infrastructure', value: 'Modern cloud', emoji: '☁️' },
            ].map((f) => (
              <div key={f.label} style={{ background: '#111', borderRadius: 10, padding: '14px 16px', border: '1px solid #222', textAlign: 'center' }}>
                <div style={{ fontSize: 24, marginBottom: 6 }}>{f.emoji}</div>
                <div style={{ fontWeight: 700, fontSize: 15, color: '#fff', fontFamily: f.label === 'DNS Server' ? 'monospace' : undefined }}>{f.value}</div>
                <div style={{ color: '#666', fontSize: 11, marginTop: 2 }}>{f.label}</div>
              </div>
            ))}
          </div>

          {/* WiiLink channels */}
          <div>
            <h3 style={{ margin: '0 0 12px', fontSize: 14, fontWeight: 700, color: '#fff' }}>Restored Wii Channels</h3>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: 10 }}>
              {channels.map((ch) => <ChannelCard key={ch.id} channel={ch} />)}
            </div>
          </div>

          {/* Note */}
          <div style={{ background: '#1a0d2e', borderRadius: 10, padding: '14px 16px', border: '1px solid #00c9a730', fontSize: 13, color: '#aaa', lineHeight: 1.6 }}>
            💡 <strong style={{ color: '#fff' }}>WiiLink WFC vs Wiimmfi.</strong> Both services restore online play for the same DS/Wii titles.
            Wiimmfi has been running the longest and has the largest registered player base. WiiLink WFC offers newer infrastructure and
            tighter integration with the WiiLink channel ecosystem. You can switch providers any time from the provider switcher on game-specific pages.
          </div>
        </div>
      )}
    </div>
  );
}
