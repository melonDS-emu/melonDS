import { Outlet, Link, useLocation } from 'react-router-dom';

const NAV_ITEMS = [
  { path: '/', label: '🏠 Home', emoji: '🏠' },
  { path: '/library', label: '🎮 Library', emoji: '🎮' },
  { path: '/saves', label: '💾 Saves', emoji: '💾' },
];

export function Layout() {
  const location = useLocation();

  return (
    <div className="flex h-screen">
      {/* Sidebar */}
      <nav className="w-56 flex-shrink-0 flex flex-col" style={{ backgroundColor: 'var(--color-oasis-surface)' }}>
        <div className="p-4 border-b border-white/10">
          <h1 className="text-xl font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
            🌴 RetroOasis
          </h1>
          <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Play Together
          </p>
        </div>

        <div className="flex-1 py-4">
          {NAV_ITEMS.map((item) => (
            <Link
              key={item.path}
              to={item.path}
              className="block px-4 py-2.5 text-sm font-medium transition-colors rounded-lg mx-2 mb-1"
              style={{
                backgroundColor: location.pathname === item.path ? 'var(--color-oasis-card)' : 'transparent',
                color: location.pathname === item.path ? 'var(--color-oasis-accent-light)' : 'var(--color-oasis-text-muted)',
              }}
            >
              {item.label}
            </Link>
          ))}
        </div>

        {/* Online friends stub */}
        <div className="p-4 border-t border-white/10">
          <p className="text-xs font-semibold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Friends Online
          </p>
          <div className="space-y-2">
            <FriendBadge name="Player2" status="in-game" game="Mario Kart 64" />
            <FriendBadge name="LinkMaster" status="online" />
            <FriendBadge name="RetroFan" status="idle" />
          </div>
        </div>
      </nav>

      {/* Main content */}
      <main className="flex-1 overflow-y-auto p-6">
        <Outlet />
      </main>
    </div>
  );
}

function FriendBadge({ name, status, game }: { name: string; status: string; game?: string }) {
  const statusColor =
    status === 'in-game' ? 'var(--color-oasis-green)' :
    status === 'online' ? 'var(--color-oasis-accent-light)' :
    'var(--color-oasis-text-muted)';

  return (
    <div className="flex items-center gap-2">
      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: statusColor }} />
      <div>
        <p className="text-xs font-medium" style={{ color: 'var(--color-oasis-text)' }}>{name}</p>
        {game && <p className="text-[10px]" style={{ color: 'var(--color-oasis-text-muted)' }}>{game}</p>}
      </div>
    </div>
  );
}
