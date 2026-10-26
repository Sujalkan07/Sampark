import React, { createContext, useContext, useState, useEffect, useCallback } from 'react';
import { api } from '../api/client';
import { useAuth } from './AuthContext';

const PreferencesContext = createContext(null);

const DEFAULT_PREFERENCES = {
  theme: 'system',
  sound_enabled: true,
  desktop_notifications_enabled: false,
  read_receipts_enabled: true,
};

function getStoredPreferences() {
  try {
    const raw = localStorage.getItem('sampark_preferences');
    if (raw) {
      return JSON.parse(raw);
    }
  } catch {}
  return null;
}

export function PreferencesProvider({ children }) {
  const { user } = useAuth();
  const [preferences, setPreferences] = useState(() => {
    const stored = getStoredPreferences();
    return stored ? { ...DEFAULT_PREFERENCES, ...stored } : DEFAULT_PREFERENCES;
  });
  const [loading, setLoading] = useState(false);
  const [effectiveDark, setEffectiveDark] = useState(() => {
    if (typeof document !== 'undefined' && document.documentElement.classList.contains('dark')) {
      return true;
    }
    const stored = getStoredPreferences();
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

  // Load preferences from backend when authenticated
  useEffect(() => {
    if (!user) return;

    let ignore = false;
    async function load() {
      try {
        setLoading(true);
        const data = await api.getPreferences();
        if (!ignore && data?.preferences) {
          const stored = getStoredPreferences();
          // Use stored local preference if explicitly set, else use backend preference
          const chosenTheme = stored?.theme || data.preferences.theme || 'system';
          const merged = {
            ...DEFAULT_PREFERENCES,
            ...data.preferences,
            theme: chosenTheme,
          };
          setPreferences(merged);
          applyTheme(chosenTheme);
          try {
            localStorage.setItem('sampark_preferences', JSON.stringify(merged));
          } catch {}
        }
      } catch (err) {
        // Fall back to stored preferences
      } finally {
        if (!ignore) setLoading(false);
      }
    }

    load();
    return () => {
      ignore = true;
    };
  }, [user, applyTheme]);

  // Update preferences function
  const updatePreferences = useCallback(
    async (newPrefs) => {
      let mergedState;
      setPreferences((prev) => {
        mergedState = { ...prev, ...newPrefs };
        try {
          localStorage.setItem('sampark_preferences', JSON.stringify(mergedState));
        } catch {}
        return mergedState;
      });

      // Synchronously apply theme immediately without waiting for state reconciliation
      if (newPrefs.theme) {
        applyTheme(newPrefs.theme);
      }

      if (user) {
        try {
          await api.updatePreferences(newPrefs);
        } catch (err) {
          console.warn('Failed to sync preferences to backend:', err);
        }
      }
    },
    [user, applyTheme]
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

