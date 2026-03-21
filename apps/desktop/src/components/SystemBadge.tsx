/** System colour map used for badge backgrounds (mirrors mock-games.ts values). */
export const SYSTEM_COLORS: Record<string, string> = {
  NES:  '#b0392b',
  SNES: '#7B5EA7',
  GB:   '#5c7a3e',
  GBC:  '#8b4513',
  GBA:  '#614fa8',
  N64:  '#009900',
  NDS:  '#E87722',
  GC:   '#6a0dad',
  '3DS': '#cc0000',
  Wii:  '#009ac7',
  'Wii U': '#009ac7',
  Genesis: '#1565c0',
  Dreamcast: '#e26222',
  PSX:  '#003087',
  PS2:  '#003087',
  PSP:  '#003087',
};

type BadgeSize = 'xs' | 'sm' | 'md';

const SIZE_CLASSES: Record<BadgeSize, string> = {
  xs: 'text-[9px] px-1.5 py-0.5',
  sm: 'text-[10px] px-2 py-0.5',
  md: 'text-xs px-3 py-1',
};

interface SystemBadgeProps {
  system: string;
  /** Override the badge background colour. Falls back to SYSTEM_COLORS lookup. */
  color?: string;
  size?: BadgeSize;
}

/** Pill badge that shows a system label with its characteristic colour. */
export function SystemBadge({ system, color, size = 'sm' }: SystemBadgeProps) {
  const bg = color ?? SYSTEM_COLORS[system] ?? 'var(--color-oasis-accent)';
  return (
    <span
      className={`${SIZE_CLASSES[size]} rounded-full font-black`}
      style={{ backgroundColor: bg, color: 'white' }}
    >
      {system}
    </span>
  );
}
