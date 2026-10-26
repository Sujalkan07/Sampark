/**
 * Date and presence utilities for Sampark
 * Properly handles UTC timestamps stored by SQLite (datetime('now'))
 * and eliminates local timezone offset skew.
 */

export function parseUtcDate(dateStr) {
  if (!dateStr) return null;
  try {
    const trimmed = String(dateStr).trim();
    if (!trimmed) return null;
    // SQLite datetime('now') returns 'YYYY-MM-DD HH:MM:SS' without timezone.
    // Explicitly append 'Z' so browsers parse it as UTC rather than local time.
    const iso = trimmed.includes('T')
      ? (trimmed.endsWith('Z') ? trimmed : trimmed + 'Z')
      : trimmed.replace(' ', 'T') + 'Z';
    const d = new Date(iso);
    return isNaN(d.getTime()) ? null : d;
  } catch {
    return null;
  }
}

export function getPresenceStatus(lastActiveAt) {
  if (!lastActiveAt) return { isOnline: false, label: 'Offline' };
  const date = parseUtcDate(lastActiveAt);
  if (!date) return { isOnline: false, label: 'Offline' };

  try {
    const now = Date.now();
    const diffSecs = Math.floor((now - date.getTime()) / 1000);

    // Active within 120 seconds (with tolerance for minor client/server clock drift)
    if (diffSecs >= -60 && diffSecs < 120) {
      return { isOnline: true, label: 'Active now' };
    }
    if (diffSecs < -60) {
      // Future timestamp beyond drift: safely display active recently
      return { isOnline: true, label: 'Active recently' };
    }
    if (diffSecs < 3600) {
      const mins = Math.max(1, Math.floor(diffSecs / 60));
      return { isOnline: false, label: `Active ${mins}m ago` };
    }
    if (diffSecs < 86400) {
      const hours = Math.floor(diffSecs / 3600);
      return { isOnline: false, label: `Active ${hours}h ago` };
    }
    return {
      isOnline: false,
      label: `Active ${date.toLocaleDateString(undefined, { month: 'short', day: 'numeric' })}`,
    };
  } catch {
    return { isOnline: false, label: 'Offline' };
  }
}

export function formatMessageTime(dateStr) {
  const d = parseUtcDate(dateStr);
  if (!d) return '';
  return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}

export function formatPreviewTime(dateStr) {
  const d = parseUtcDate(dateStr);
  if (!d) return '';
  const today = new Date();
  if (d.toDateString() === today.toDateString()) {
    return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  }
  return d.toLocaleDateString(undefined, { month: 'short', day: 'numeric' });
}

export function formatJoinedDate(dateStr) {
  const d = parseUtcDate(dateStr);
  if (!d) return 'Active Member';
  return d.toLocaleDateString(undefined, {
    month: 'short',
    year: 'numeric',
  });
}
