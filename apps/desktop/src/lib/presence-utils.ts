import type { ActivityEventType } from '@retro-oasis/presence-client';

export function activityEmoji(type: ActivityEventType): string {
  if (type === 'started') return '🚀';
  if (type === 'joined') return '🚪';
  if (type === 'finished') return '🏁';
  return '🟢';
}

export function activityVerb(type: ActivityEventType): string {
  if (type === 'started') return 'started';
  if (type === 'joined') return 'joined';
  if (type === 'finished') return 'finished';
  return 'came online';
}

export function relativeTime(iso: string): string {
  const diff = Math.floor((Date.now() - new Date(iso).getTime()) / 1000);
  if (diff < 60) return 'just now';
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
  if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
  return `${Math.floor(diff / 86400)}d ago`;
}

const AVATAR_COLORS = ['#7c5cbf', '#009900', '#c0392b', '#2980b9', '#8e44ad', '#16a085'];

export function avatarColor(userId: string): string {
  let hash = 0;
  for (let i = 0; i < userId.length; i++) hash = userId.charCodeAt(i) + ((hash << 5) - hash);
  return AVATAR_COLORS[Math.abs(hash) % AVATAR_COLORS.length];
}
