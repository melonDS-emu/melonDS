export type DesktopPlatform = "windows" | "macos" | "linux" | "unknown";

function readNavigatorPlatform(): string {
  if (typeof navigator === 'undefined') return '';

  const uaDataPlatform = navigator.userAgentData?.platform;
  if (typeof uaDataPlatform === 'string' && uaDataPlatform.trim()) {
    return uaDataPlatform;
  }

  if (typeof navigator.platform === 'string' && navigator.platform.trim()) {
    return navigator.platform;
  }

  if (typeof navigator.userAgent === 'string') {
    return navigator.userAgent;
  }

  return '';
}

export function detectDesktopPlatform(): DesktopPlatform {
  const raw = readNavigatorPlatform().toLowerCase();

  if (raw.includes('win')) return 'windows';
  if (raw.includes('mac')) return 'macos';
  if (raw.includes('linux') || raw.includes('x11')) return 'linux';
  return 'unknown';
}

export function isWindowsPlatform(): boolean {
  return detectDesktopPlatform() === 'windows';
}
