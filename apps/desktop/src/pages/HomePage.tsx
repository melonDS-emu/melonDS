import { useState, useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useGames } from '../lib/use-games';
import { GameCard } from '../components/GameCard';
import { LobbyCard } from '../components/LobbyCard';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';
import { useLobby } from '../context/LobbyContext';
import { usePresence } from '../context/PresenceContext';
import { activityEmoji, activityVerb, relativeTime } from '../lib/presence-utils';
import { fetchLeaderboard, type LeaderboardEntry } from '../lib/stats-service';
import { listClipMeta, formatClipDuration, type ClipMeta } from '../lib/clip-service';
import {
  fetchCurrentEvents,
  fetchFeaturedGames,
  themeGradient,
  themeAccent,
  type SeasonalEvent,
  type FeaturedGame,
} from '../lib/events-service';
import type { Room } from '../services/lobby-types';
import type { FriendInfo } from '@retro-oasis/presence-client';

export function HomePage() {
  const navigate = useNavigate();
  const { publicRooms, createRoom, joinByCode, joinAsSpectator, currentRoom, listRooms, connectionState } = useLobby();
  const { joinableSessions, recentActivity } = usePresence();
  const [showHost, setShowHost] = useState(false);
  const [showJoin, setShowJoin] = useState(false);
  const [joinFriend, setJoinFriend] = useState<FriendInfo | null>(null);
  const [topPlayers, setTopPlayers] = useState<LeaderboardEntry[]>([]);
  const [recentClips, setRecentClips] = useState<ClipMeta[]>([]);
  const [activeEvents, setActiveEvents] = useState<SeasonalEvent[]>([]);
  const [featuredGames, setFeaturedGames] = useState<FeaturedGame[]>([]);

  const { data: allGames } = useGames();
  const n64PartyGames = allGames.filter((g) => g.system === 'N64' && g.tags.includes('Party'));
  const ndsShowcaseGames = allGames.filter((g) => g.system === 'NDS');
  const partyPicks = allGames.filter((g) => g.tags.includes('Party') && g.system !== 'N64' && g.system !== 'NDS');
  const recentGames = allGames.slice(0, 4);

  useEffect(() => {
    if (connectionState === 'connected') listRooms();
  }, [connectionState, listRooms]);

  useEffect(() => {
    fetchLeaderboard('sessions', 5).then(setTopPlayers).catch(() => {});
  }, []);

  useEffect(() => {
    setRecentClips(listClipMeta().slice(0, 3));
  }, []);

  useEffect(() => {
    fetchCurrentEvents().then((d) => setActiveEvents(d.active)).catch(() => {});
    fetchFeaturedGames().then(setFeaturedGames).catch(() => {});
  }, []);

  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  function handleHostConfirm(payload: Parameters<typeof createRoom>[0], displayName: string) {
    createRoom(payload, displayName);
    setShowHost(false);
  }

  function handleJoinConfirm(roomCode: string, displayName: string) {
    joinByCode(roomCode, displayName);
    setShowJoin(false);
    setJoinFriend(null);
  }

  function handleSpectateConfirm(roomCode: string, displayName: string) {
    joinAsSpectator({ roomCode }, displayName);
    setShowJoin(false);
    setJoinFriend(null);
  }

  function toLobbyCard(room: Room) {
    const game = allGames.find((g) => g.id === room.gameId);
    return {
      id: room.id,
      name: room.name,
      host: room.players[0]?.displayName ?? 'Unknown',
      gameTitle: room.gameTitle,
      system: room.system.toUpperCase(),
      systemColor: game?.systemColor ?? 'var(--color-oasis-accent)',
      playerCount: room.players.length,
      maxPlayers: room.maxPlayers,
      status: room.status,
    };
  }

  const displayLobbies = publicRooms.length > 0
    ? publicRooms.slice(0, 6).map(toLobbyCard)
    : [];

  return (
    <div className="space-y-8 max-w-5xl">

      {/* ── Connection banner ── */}
      {connectionState !== 'connected' && (
        <div
          className="px-4 py-2.5 rounded-xl text-xs font-bold flex items-center gap-2"
          style={{
            backgroundColor: connectionState === 'connecting'
              ? 'rgba(255,179,0,0.1)'
              : 'rgba(255,255,255,0.04)',
            color: connectionState === 'connecting'
              ? 'var(--color-oasis-yellow)'
              : 'var(--color-oasis-text-muted)',
            border: `1px solid ${connectionState === 'connecting' ? 'rgba(255,179,0,0.3)' : 'var(--n-border)'}`,
          }}
        >
          <span className="w-1.5 h-1.5 rounded-full inline-block" style={{ backgroundColor: connectionState === 'connecting' ? 'var(--color-oasis-yellow)' : 'var(--color-oasis-text-muted)' }} />
          <span>
            {connectionState === 'connecting'
              ? 'Connecting to lobby server…'
              : 'Offline — lobby server unreachable'}
          </span>
        </div>
      )}

      {/* ── Hero CTA ── */}
      <div
        className="rounded-3xl p-7 relative overflow-hidden"
        style={{
          background: 'linear-gradient(135deg, #1c0008 0%, #0c0c20 50%, #00100a 100%)',
          border: '1px solid rgba(230,0,18,0.25)',
          boxShadow: '0 0 60px rgba(230,0,18,0.08), 0 0 120px rgba(0,80,40,0.06)',
        }}
      >
        {/* Background decoration */}
        <div
          className="absolute inset-0 pointer-events-none"
          style={{
            background: 'radial-gradient(ellipse at 80% 50%, rgba(230,0,18,0.08) 0%, transparent 60%), radial-gradient(ellipse at 10% 80%, rgba(0,150,60,0.06) 0%, transparent 50%)',
          }}
        />
        <div className="relative z-10 flex flex-col gap-5">
          <div>
            <h1 className="text-4xl font-black tracking-tight leading-none mb-1">
              <span className="text-gradient-red">Retro</span>
              <span style={{ color: '#f0f0ff' }}>Oasis</span>
            </h1>
            <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Play classic games together, online
            </p>
          </div>
          <div className="flex gap-3">
            <button
              onClick={() => setShowHost(true)}
              className="flex-1 py-3.5 rounded-2xl text-base font-black btn-glow-red shimmer-overlay"
              style={{ color: 'white' }}
            >
              Host a Room
            </button>
            <button
              onClick={() => setShowJoin(true)}
              className="flex-1 py-3.5 rounded-2xl text-base font-black btn-glass"
              style={{ color: 'var(--color-oasis-text)' }}
            >
              Join a Room
            </button>
          </div>
        </div>
      </div>

      {/* ── Seasonal Event Banner ── */}
      {activeEvents.length > 0 && (
        <div
          className="rounded-2xl p-5 flex items-start gap-4 relative overflow-hidden"
          style={{
            background: themeGradient(activeEvents[0].theme),
            border: `1px solid ${themeAccent(activeEvents[0].theme)}44`,
            boxShadow: `0 0 30px ${themeAccent(activeEvents[0].theme)}20`,
          }}
        >
          <div
            className="absolute inset-0 pointer-events-none"
            style={{ background: 'radial-gradient(ellipse at 90% 10%, rgba(255,255,255,0.08) 0%, transparent 50%)' }}
          />
          <span className="text-3xl flex-shrink-0 relative z-10">{activeEvents[0].emoji}</span>
          <div className="flex-1 min-w-0 relative z-10">
            <div className="flex items-center gap-2 mb-1">
              <h2 className="font-black text-base" style={{ color: '#fff' }}>
                {activeEvents[0].name}
              </h2>
              <span
                className="text-[10px] font-black px-2 py-0.5 rounded-full"
                style={{ backgroundColor: `${themeAccent(activeEvents[0].theme)}22`, color: themeAccent(activeEvents[0].theme) }}
              >
                ● Live
              </span>
              {(activeEvents[0].xpMultiplier ?? 1) > 1 && (
                <span
                  className="text-[10px] font-black px-2 py-0.5 rounded-full"
                  style={{ backgroundColor: 'rgba(255,179,0,0.2)', color: 'var(--color-oasis-yellow)' }}
                >
                  ⚡ {activeEvents[0].xpMultiplier}× XP
                </span>
              )}
            </div>
            <p className="text-xs" style={{ color: 'rgba(255,255,255,0.7)' }}>
              {activeEvents[0].description}
            </p>
          </div>
          <Link
            to="/events"
            className="flex-shrink-0 text-xs font-black px-3 py-1.5 rounded-full transition-all hover:brightness-110 relative z-10"
            style={{ backgroundColor: themeAccent(activeEvents[0].theme), color: '#000' }}
          >
            Details →
          </Link>
        </div>
      )}

      {/* ── Joinable Lobbies ── */}
      <section>
        <SectionHeader
          title="🔥 Joinable Lobbies"
          action={
            connectionState === 'connected'
              ? <button onClick={listRooms} className="text-xs font-bold hover:opacity-80 transition-opacity" style={{ color: 'var(--color-oasis-text-muted)' }}>↻ Refresh</button>
              : undefined
          }
        />
        {displayLobbies.length > 0 ? (
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3">
            {displayLobbies.map((lobby) => (
              <LobbyCard key={lobby.id} lobby={lobby} />
            ))}
          </div>
        ) : (
          <EmptyState
            message={connectionState === 'connected'
              ? 'No open lobbies right now — be the first to host!'
              : 'Connect to the lobby server to see open rooms.'}
          />
        )}
      </section>

      {/* ── Featured Games This Week ── */}
      {featuredGames.length > 0 && (
        <section>
          <SectionHeader
            title="⭐ Featured This Week"
            action={<Link to="/events" className="text-xs font-bold hover:opacity-80 transition-opacity" style={{ color: 'var(--color-oasis-text-muted)' }}>All Events →</Link>}
          />
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-2">
            {featuredGames.map((g) => (
              <Link
                key={g.gameId}
                to={`/game/${g.gameId}`}
                className="rounded-xl p-3 flex flex-col items-center gap-1.5 text-center n-card shimmer-overlay"
                style={{ backgroundColor: 'var(--color-oasis-card)' }}
              >
                <span className="text-2xl">{g.emoji}</span>
                <p className="text-xs font-black leading-tight" style={{ color: 'var(--color-oasis-text)' }}>
                  {g.gameTitle}
                </p>
                <p className="text-[9px] font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {g.reason}
                </p>
              </Link>
            ))}
          </div>
        </section>
      )}

      {/* ── Friends Playing Now ── */}
      {joinableSessions.length > 0 && (
        <section>
          <SectionHeader
            title="👥 Friends Playing Now"
            subtitle="Jump in and play with your crew"
            action={<Link to="/friends" className="text-xs font-bold hover:opacity-80 transition-opacity" style={{ color: 'var(--color-oasis-text-muted)' }}>All Friends →</Link>}
          />
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3">
            {joinableSessions.map((friend) => (
              <FriendSessionCard
                key={friend.userId}
                friend={friend}
                onJoin={() => setJoinFriend(friend)}
              />
            ))}
          </div>
        </section>
      )}

      {/* ── Recent Activity ── */}
      {recentActivity.length > 0 && (
        <section>
          <SectionHeader
            title="⚡ Recent Activity"
            action={<Link to="/friends" className="text-xs font-bold hover:opacity-80 transition-opacity" style={{ color: 'var(--color-oasis-text-muted)' }}>See All →</Link>}
          />
          <div
            className="rounded-2xl overflow-hidden"
            style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
          >
            {recentActivity.slice(0, 4).map((item, i) => (
              <div
                key={item.id}
                className="px-4 py-3 flex items-center gap-3 hover:bg-white/[0.02] transition-colors"
                style={{ borderTop: i > 0 ? '1px solid var(--n-border)' : undefined }}
              >
                <span className="text-base flex-shrink-0">{activityEmoji(item.type)}</span>
                <div className="flex-1 min-w-0">
                  <p className="text-xs" style={{ color: 'var(--color-oasis-text)' }}>
                    <span className="font-black">{item.displayName}</span>{' '}
                    <span style={{ color: 'var(--color-oasis-text-muted)' }}>{activityVerb(item.type)}</span>
                    {item.gameTitle && (
                      <> <span className="font-bold">{item.gameTitle}</span></>
                    )}
                  </p>
                </div>
                <span className="text-[10px] flex-shrink-0" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {relativeTime(item.timestamp)}
                </span>
                {item.roomCode && joinableSessions.some((f) => f.userId === item.userId) && (
                  <button
                    onClick={() => {
                      const friend = joinableSessions.find((f) => f.userId === item.userId);
                      if (friend) setJoinFriend(friend);
                    }}
                    className="text-[10px] font-black px-2.5 py-1 rounded-full btn-glow-red"
                    style={{ color: 'white' }}
                  >
                    Join
                  </button>
                )}
              </div>
            ))}
          </div>
        </section>
      )}

      {/* ── N64 Party Spotlight ── */}
      {n64PartyGames.length > 0 && (
        <section>
          <SectionHeader
            title="🟢 N64 Party Games"
            subtitle="Best with 4 players — grab your friends!"
            action={<Link to="/library" className="text-xs font-bold hover:opacity-80 transition-opacity" style={{ color: 'var(--color-oasis-text-muted)' }}>View All →</Link>}
          />
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
            {n64PartyGames.map((game) => (
              <GameCard key={game.id} game={game} />
            ))}
          </div>
        </section>
      )}

      {/* ── Nintendo DS Spotlight ── */}
      {ndsShowcaseGames.length > 0 && (
        <section>
          <SectionHeader
            title="📱 Nintendo DS — Dual Screen"
            subtitle="Touch controls, WFC online, and local wireless"
            titleColor="var(--color-oasis-ds-orange)"
            action={<Link to="/library" className="text-xs font-bold hover:opacity-80 transition-opacity" style={{ color: 'var(--color-oasis-text-muted)' }}>View All →</Link>}
          />
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
            {ndsShowcaseGames.slice(0, 8).map((game) => (
              <GameCard key={game.id} game={game} />
            ))}
          </div>
          <div
            className="mt-3 px-4 py-3 rounded-xl text-xs flex items-start gap-3"
            style={{
              backgroundColor: 'rgba(230,126,34,0.06)',
              border: '1px solid rgba(230,126,34,0.2)',
            }}
          >
            <span className="text-base flex-shrink-0">💡</span>
            <div style={{ color: 'var(--color-oasis-text-muted)' }}>
              <span className="font-black" style={{ color: 'var(--color-oasis-text)' }}>Getting started with DS:</span>{' '}
              Install melonDS, place your BIOS files in{' '}
              <code
                className="text-[10px] px-1 py-0.5 rounded font-mono"
                style={{ backgroundColor: 'rgba(255,255,255,0.08)' }}
              >
                ~/.config/melonDS/
              </code>
              , then host a room and launch your ROM. The touch screen is controlled by your mouse.
              WFC games like Pokémon and Mario Kart connect via Wiimmfi automatically.
            </div>
          </div>
        </section>
      )}

      {/* ── More Party Picks ── */}
      {partyPicks.length > 0 && (
        <section>
          <SectionHeader
            title="🎉 More Party Picks"
            action={<Link to="/library" className="text-xs font-bold hover:opacity-80 transition-opacity" style={{ color: 'var(--color-oasis-text-muted)' }}>View All →</Link>}
          />
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
            {partyPicks.map((game) => (
              <GameCard key={game.id} game={game} />
            ))}
          </div>
        </section>
      )}

      {/* ── Leaderboard teaser ── */}
      {topPlayers.length > 0 && (
        <section>
          <SectionHeader
            title="🏆 Top Players"
            action={<Link to="/profile" className="text-xs font-bold hover:opacity-80 transition-opacity" style={{ color: 'var(--color-oasis-text-muted)' }}>Full leaderboard →</Link>}
          />
          <div
            className="rounded-2xl overflow-hidden"
            style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
          >
            {topPlayers.map((entry, i) => (
              <div
                key={entry.playerId}
                className="flex items-center gap-3 px-4 py-3 hover:bg-white/[0.02] transition-colors"
                style={{ borderTop: i > 0 ? '1px solid var(--n-border)' : undefined }}
              >
                <span className="w-6 text-sm font-black text-center" style={{ color: 'var(--color-oasis-yellow)' }}>
                  {entry.rank === 1 ? '🥇' : entry.rank === 2 ? '🥈' : entry.rank === 3 ? '🥉' : `#${entry.rank}`}
                </span>
                <span className="flex-1 text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
                  {entry.displayName}
                </span>
                <span
                  className="text-[10px] font-bold px-2 py-0.5 rounded-full"
                  style={{ backgroundColor: 'rgba(255,255,255,0.05)', color: 'var(--color-oasis-text-muted)' }}
                >
                  {entry.value} sessions
                </span>
              </div>
            ))}
          </div>
        </section>
      )}

      {/* ── Recently Played ── */}
      <section>
        <SectionHeader title="🕹️ Recently Played" />
        <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
          {recentGames.map((game) => (
            <GameCard key={game.id} game={game} />
          ))}
        </div>
      </section>

      {/* ── Recent Clips ── */}
      {recentClips.length > 0 && (
        <section>
          <SectionHeader
            title="🎬 Recent Clips"
            subtitle="Captured session recordings"
          />
          <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
            {recentClips.map((clip) => (
              <div
                key={clip.id}
                className="n-card rounded-2xl p-4 flex items-center gap-3 shimmer-overlay"
                style={{ backgroundColor: 'var(--color-oasis-card)' }}
              >
                <span className="text-xl">▶</span>
                <div className="flex-1 min-w-0">
                  <p className="text-sm font-black truncate" style={{ color: 'var(--color-oasis-text)' }}>
                    {clip.gameTitle}
                  </p>
                  <p className="text-[10px] mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    {formatClipDuration(clip.durationSecs)} · {new Date(clip.capturedAt).toLocaleDateString()}
                  </p>
                </div>
              </div>
            ))}
          </div>
        </section>
      )}

      {/* Modals */}
      {showHost && (
        <HostRoomModal onConfirm={handleHostConfirm} onClose={() => setShowHost(false)} />
      )}
      {(showJoin || joinFriend) && (
        <JoinRoomModal
          initialCode={joinFriend?.roomCode}
          onConfirm={handleJoinConfirm}
          onSpectate={handleSpectateConfirm}
          onClose={() => { setShowJoin(false); setJoinFriend(null); }}
        />
      )}
    </div>
  );
}

// ── Helper sub-components ──────────────────────────────────────────────────────

function SectionHeader({
  title,
  subtitle,
  action,
  titleColor,
}: {
  title: string;
  subtitle?: string;
  action?: React.ReactNode;
  titleColor?: string;
}) {
  return (
    <div className="flex items-start justify-between mb-4">
      <div>
        <h2
          className="text-base font-black section-title-line"
          style={{ color: titleColor ?? 'var(--color-oasis-text)' }}
        >
          {title}
        </h2>
        {subtitle && (
          <p className="text-xs mt-0.5 font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {subtitle}
          </p>
        )}
      </div>
      {action && (
        <div
          className="mt-0.5 text-xs font-bold"
          style={{ color: 'var(--color-oasis-text-muted)' }}
        >
          {action}
        </div>
      )}
    </div>
  );
}

function EmptyState({ message }: { message: string }) {
  return (
    <div
      className="py-8 px-4 rounded-2xl text-center"
      style={{
        backgroundColor: 'rgba(255,255,255,0.02)',
        border: '1px dashed rgba(255,255,255,0.08)',
      }}
    >
      <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
        {message}
      </p>
    </div>
  );
}

function FriendSessionCard({ friend, onJoin }: { friend: FriendInfo; onJoin: () => void }) {
  return (
    <div
      className="rounded-2xl p-4 flex items-center gap-3 n-card shimmer-overlay"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      <div
        className="w-2 h-2 rounded-full flex-shrink-0"
        style={{
          backgroundColor: 'var(--color-oasis-green)',
          boxShadow: '0 0 8px var(--color-oasis-green)',
        }}
      />
      <div className="flex-1 min-w-0">
        <p className="text-sm font-black truncate">{friend.displayName}</p>
        <p className="text-xs truncate font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {friend.currentGameTitle ?? 'In a game'}
        </p>
      </div>
      <button
        onClick={onJoin}
        className="text-xs font-black px-3 py-1.5 rounded-full btn-glow-red"
        style={{ color: 'white' }}
      >
        ▶ Join
      </button>
    </div>
  );
}

