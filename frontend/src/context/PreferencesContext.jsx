import React, { createContext, useContext, useState, useEffect, useCallback } from 'react';
import { api } from '../api/client';
import { useAuth } from './AuthContext';

const PreferencesContext = createContext(null);

const DEFAULT_PREFERENCES = {
  theme: 'system',
  sound_enabled: true,
  soundNotifications: true,
  desktop_notifications_enabled: false,
  desktopNotifications: false,
  read_receipts_enabled: true,
  readReceiptsEnabled: true,
};

function getStoredPreferences(userId) {
  try {
    const key = userId ? `sampark_preferences_${userId}` : 'sampark_preferences_guest';
    const raw = localStorage.getItem(key);
    if (raw) {
      return JSON.parse(raw);
    }
  } catch {}
  return null;
}

export function PreferencesProvider({ children }) {
  const { user } = useAuth();
  const [preferences, setPreferences] = useState(() => {
    const stored = getStoredPreferences(user?.id);
    return stored ? { ...DEFAULT_PREFERENCES, ...stored } : DEFAULT_PREFERENCES;
  });
  const [loading, setLoading] = useState(false);
  const [effectiveDark, setEffectiveDark] = useState(() => {
    if (typeof document !== 'undefined' && document.documentElement.classList.contains('dark')) {
      return true;
    }
    const stored = getStoredPreferences(user?.id);
    if (stored?.theme === 'dark') return true;
    if (stored?.theme === 'light') return false;
    return (
      typeof window !== 'undefined' &&
      window.matchMedia &&
      window.matchMedia('(prefers-color-scheme: dark)').matches
    );
  });

  // Apply theme to document element
  const applyTheme = useCallback((themeChoice) => {
    if (typeof document === 'undefined') return;
    const root = document.documentElement;
    let isDark = false;

    if (themeChoice === 'dark') {
      isDark = true;
    } else if (themeChoice === 'light') {
      isDark = false;
    } else {
      // system
      isDark =
        typeof window !== 'undefined' &&
        window.matchMedia &&
        window.matchMedia('(prefers-color-scheme: dark)').matches;
    }

    if (isDark) {
      root.classList.add('dark');
      root.setAttribute('data-theme', 'dark');
      root.style.colorScheme = 'dark';
      document.body?.classList.add('dark');
    } else {
      root.classList.remove('dark');
      root.setAttribute('data-theme', 'light');
      root.style.colorScheme = 'light';
      document.body?.classList.remove('dark');
    }
    setEffectiveDark(isDark);
  }, []);

  // Sync theme whenever preferences.theme changes
  useEffect(() => {
    applyTheme(preferences.theme);

    if (preferences.theme === 'system' && window.matchMedia) {
      const mediaQuery = window.matchMedia('(prefers-color-scheme: dark)');
      const handler = () => {
        if (preferences.theme === 'system') {
          applyTheme('system');
        }
      };
      mediaQuery.addEventListener('change', handler);
      return () => mediaQuery.removeEventListener('change', handler);
    }
  }, [preferences.theme, applyTheme]);

  // Load preferences from backend when authenticated, or reset when logged out
  useEffect(() => {
    if (!user) {
      setPreferences(DEFAULT_PREFERENCES);
      applyTheme('system');
      return;
    }

    // Apply any account-scoped cached preference immediately
    const cached = getStoredPreferences(user.id);
    if (cached) {
      setPreferences({ ...DEFAULT_PREFERENCES, ...cached });
      applyTheme(cached.theme || 'system');
    }

    let ignore = false;
    async function load() {
      try {
        setLoading(true);
        const data = await api.getPreferences();
        if (!ignore && data?.preferences) {
          // Account-scoped server preferences take authoritative priority
          const serverTheme = data.preferences.theme || 'system';
          const merged = {
            ...DEFAULT_PREFERENCES,
            ...data.preferences,
            sound_enabled: data.preferences.soundNotifications ?? data.preferences.sound_enabled ?? true,
            soundNotifications: data.preferences.soundNotifications ?? data.preferences.sound_enabled ?? true,
            desktop_notifications_enabled: data.preferences.desktopNotifications ?? data.preferences.desktop_notifications_enabled ?? false,
            desktopNotifications: data.preferences.desktopNotifications ?? data.preferences.desktop_notifications_enabled ?? false,
            read_receipts_enabled: data.preferences.readReceiptsEnabled ?? data.preferences.read_receipts_enabled ?? true,
            readReceiptsEnabled: data.preferences.readReceiptsEnabled ?? data.preferences.read_receipts_enabled ?? true,
            theme: serverTheme,
          };
          setPreferences(merged);
          applyTheme(serverTheme);
          try {
            localStorage.setItem(`sampark_preferences_${user.id}`, JSON.stringify(merged));
          } catch {}
        }
      } catch (err) {
        // Fall back to account cached preferences
      } finally {
        if (!ignore) setLoading(false);
      }
    }

    load();
    return () => {
      ignore = true;
    };
  }, [user, applyTheme]);

  // Update preferences function with rollback on network failure
  const updatePreferences = useCallback(
    async (newPrefs) => {
      const prevPreferences = preferences;
      const prevTheme = preferences.theme;

      const mergedState = {
        ...prevPreferences,
        ...newPrefs,
        sound_enabled: newPrefs.soundNotifications ?? newPrefs.sound_enabled ?? prevPreferences.sound_enabled,
        soundNotifications: newPrefs.soundNotifications ?? newPrefs.sound_enabled ?? prevPreferences.soundNotifications,
        desktop_notifications_enabled: newPrefs.desktopNotifications ?? newPrefs.desktop_notifications_enabled ?? prevPreferences.desktop_notifications_enabled,
        desktopNotifications: newPrefs.desktopNotifications ?? newPrefs.desktop_notifications_enabled ?? prevPreferences.desktopNotifications,
        read_receipts_enabled: newPrefs.readReceiptsEnabled ?? newPrefs.read_receipts_enabled ?? prevPreferences.read_receipts_enabled,
        readReceiptsEnabled: newPrefs.readReceiptsEnabled ?? newPrefs.read_receipts_enabled ?? prevPreferences.readReceiptsEnabled,
      };

      setPreferences(mergedState);

      // Synchronously apply theme immediately without waiting for state reconciliation
      if (newPrefs.theme) {
        applyTheme(newPrefs.theme);
      }

      const storageKey = user?.id ? `sampark_preferences_${user.id}` : 'sampark_preferences_guest';
      try {
        localStorage.setItem(storageKey, JSON.stringify(mergedState));
      } catch {}

      if (user) {
        try {
          await api.updatePreferences(newPrefs);
        } catch (err) {
          console.warn('Failed to sync preferences to backend, reverting to previous state:', err);
          setPreferences(prevPreferences);
          applyTheme(prevTheme);
          try {
            localStorage.setItem(storageKey, JSON.stringify(prevPreferences));
          } catch {}
          throw err;
        }
      }
    },
    [user, preferences, applyTheme]
  );

  // Quick 1-click theme toggle (Light <-> Dark)
  const toggleTheme = useCallback(() => {
    const isCurrentlyDark =
      typeof document !== 'undefined'
        ? document.documentElement.classList.contains('dark')
        : effectiveDark;
    const nextTheme = isCurrentlyDark ? 'light' : 'dark';
    applyTheme(nextTheme);
    updatePreferences({ theme: nextTheme });
  }, [effectiveDark, applyTheme, updatePreferences]);

  const value = {
    preferences,
    updatePreferences,
    isDarkMode: effectiveDark,
    toggleTheme,
    loading,
  };

  return (
    <PreferencesContext.Provider value={value}>
      {children}
    </PreferencesContext.Provider>
  );
}

export function usePreferences() {
  const context = useContext(PreferencesContext);
  if (!context) {
    throw new Error('usePreferences must be used within a PreferencesProvider');
  }
  return context;
}

