import React, { useState } from 'react';
import { useAuth } from '../context/AuthContext';
import { usePreferences } from '../context/PreferencesContext';
import { api } from '../api/client';
import { playNotificationChime } from '../utils/sound';

export function SettingsModal({ isOpen, onClose }) {
  const { user, updateUser } = useAuth();
  const { preferences, updatePreferences } = usePreferences();

  const [activeTab, setActiveTab] = useState('appearance'); // appearance, notifications, privacy, profile
  const [fullName, setFullName] = useState(user?.fullName || '');
  const [bio, setBio] = useState(user?.bio || '');
  const [profileSaving, setProfileSaving] = useState(false);
  const [profileMessage, setProfileMessage] = useState(null);
  const [profileError, setProfileError] = useState(null);

  if (!isOpen) return null;

  const handleSaveProfile = async (e) => {
    e.preventDefault();
    try {
      setProfileSaving(true);
      setProfileError(null);
      setProfileMessage(null);

      const res = await api.updateProfile({ fullName: fullName.trim(), bio: bio.trim() });
      if (res?.user) {
        updateUser(res.user);
        setProfileMessage('Profile updated successfully!');
      }
    } catch (err) {
      setProfileError(err.message || 'Failed to update profile.');
    } finally {
      setProfileSaving(false);
    }
  };

  const handleRequestNotificationPermission = async () => {
    if (!('Notification' in window)) {
      alert('Desktop notifications are not supported by your browser.');
      return;
    }

    if (Notification.permission === 'granted') {
      updatePreferences({ desktop_notifications_enabled: !preferences.desktop_notifications_enabled });
    } else {
      const permission = await Notification.requestPermission();
      if (permission === 'granted') {
        updatePreferences({ desktop_notifications_enabled: true });
      } else {
        updatePreferences({ desktop_notifications_enabled: false });
      }
    }
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/50 backdrop-blur-xs animate-fade-in">
      <div
        className="w-full max-w-xl bg-surface-container-lowest border border-border-hairline rounded-2xl shadow-xl overflow-hidden flex flex-col max-h-[85vh]"
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div className="px-6 py-4 border-b border-border-hairline flex items-center justify-between bg-surface-container-low/30">
          <div className="flex items-center gap-2.5">
            <div className="w-8 h-8 rounded-lg bg-surface-container flex items-center justify-center text-primary">
              <span className="material-symbols-outlined text-[20px]">settings</span>
            </div>
            <div>
              <h2 className="text-base font-bold text-primary">Settings & Preferences</h2>
              <p className="text-xs text-on-surface-variant">Customize your Sampark experience</p>
            </div>
          </div>
          <button
            onClick={onClose}
            className="w-8 h-8 rounded-lg hover:bg-surface-container flex items-center justify-center text-outline hover:text-on-surface transition-colors cursor-pointer"
            type="button"
          >
            <span className="material-symbols-outlined text-[18px]">close</span>
          </button>
        </div>

        {/* Tab Navigation */}
        <div className="flex border-b border-border-hairline bg-surface-container-lowest px-6 pt-2 gap-1 overflow-x-auto">
          {[
            { id: 'appearance', label: 'Appearance', icon: 'palette' },
            { id: 'notifications', label: 'Notifications', icon: 'notifications' },
            { id: 'privacy', label: 'Privacy', icon: 'security' },
            { id: 'profile', label: 'Profile', icon: 'person' },
          ].map((tab) => (
            <button
              key={tab.id}
              type="button"
              onClick={() => setActiveTab(tab.id)}
              className={`flex items-center gap-2 px-3.5 py-2.5 text-xs font-semibold border-b-2 transition-all cursor-pointer whitespace-nowrap ${
                activeTab === tab.id
                  ? 'border-saffron text-primary font-bold'
                  : 'border-transparent text-on-surface-variant hover:text-on-surface'
              }`}
            >
              <span className="material-symbols-outlined text-[16px]">{tab.icon}</span>
              <span>{tab.label}</span>
            </button>
          ))}
        </div>

        {/* Tab Content Body */}
        <div className="p-6 overflow-y-auto flex-1 space-y-6">
          {/* TAB 1: APPEARANCE */}
          {activeTab === 'appearance' && (
            <div className="space-y-4">
              <div>
                <label className="text-xs font-bold text-primary block mb-1">Theme</label>
                <p className="text-xs text-on-surface-variant mb-3">
                  Choose how Sampark looks on your device.
                </p>
                <div className="grid grid-cols-3 gap-3">
                  {[
                    { id: 'light', label: 'Light', icon: 'light_mode', desc: 'Clean & crisp' },
                    { id: 'dark', label: 'Dark', icon: 'dark_mode', desc: 'Deep slate tones' },
                    { id: 'system', label: 'System', icon: 'settings_brightness', desc: 'Sync with OS' },
                  ].map((themeOpt) => {
                    const isSelected = preferences.theme === themeOpt.id;
                    return (
                      <button
                        key={themeOpt.id}
                        type="button"
                        onClick={() => updatePreferences({ theme: themeOpt.id })}
                        className={`p-3.5 rounded-xl border-2 text-left transition-all cursor-pointer flex flex-col justify-between h-24 ${
                          isSelected
                            ? 'border-saffron bg-saffron/10 shadow-xs ring-1 ring-saffron/30'
                            : 'border-border-hairline bg-surface-container-lowest hover:bg-surface-container-low'
                        }`}
                      >
                        <div className="flex items-center justify-between w-full">
                          <span
                            className={`material-symbols-outlined text-[20px] ${
                              isSelected ? 'text-saffron' : 'text-outline'
                            }`}
                          >
                            {themeOpt.icon}
                          </span>
                          {isSelected && (
                            <span className="w-2 h-2 rounded-full bg-saffron"></span>
                          )}
                        </div>
                        <div>
                          <div className="text-xs font-bold text-primary">{themeOpt.label}</div>
                          <div className="text-[10px] text-outline">{themeOpt.desc}</div>
                        </div>
                      </button>
                    );
                  })}
                </div>
              </div>
            </div>
          )}

          {/* TAB 2: NOTIFICATIONS */}
          {activeTab === 'notifications' && (
            <div className="space-y-5">
              <div className="flex items-center justify-between p-3.5 rounded-xl bg-surface-container-low/50 border border-border-hairline">
                <div className="space-y-0.5">
                  <div className="text-xs font-bold text-primary">Message Sounds</div>
                  <div className="text-xs text-on-surface-variant">
                    Play a gentle chime when a new message arrives.
                  </div>
                </div>
                <div className="flex items-center gap-3">
                  <button
                    type="button"
                    onClick={() => playNotificationChime()}
                    className="px-2.5 py-1 text-[11px] font-semibold text-secondary hover:bg-surface-container rounded-md border border-border-hairline transition-colors cursor-pointer"
                    title="Preview Chime"
                  >
                    Test
                  </button>
                  <label className="relative inline-flex items-center cursor-pointer">
                    <input
                      type="checkbox"
                      checked={preferences.sound_enabled}
                      onChange={(e) => updatePreferences({ sound_enabled: e.target.checked })}
                      className="sr-only peer"
                    />
                    <div className="w-9 h-5 bg-surface-container-high peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-4 after:w-4 after:transition-all peer-checked:bg-saffron"></div>
                  </label>
                </div>
              </div>

              <div className="flex items-center justify-between p-3.5 rounded-xl bg-surface-container-low/50 border border-border-hairline">
                <div className="space-y-0.5">
                  <div className="text-xs font-bold text-primary">Desktop Notifications</div>
                  <div className="text-xs text-on-surface-variant">
                    Receive browser notifications when Sampark is in the background.
                  </div>
                </div>
                <label className="relative inline-flex items-center cursor-pointer">
                  <input
                    type="checkbox"
                    checked={preferences.desktop_notifications_enabled}
                    onChange={handleRequestNotificationPermission}
                    className="sr-only peer"
                  />
                  <div className="w-9 h-5 bg-surface-container-high peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-4 after:w-4 after:transition-all peer-checked:bg-saffron"></div>
                </label>
              </div>
            </div>
          )}

          {/* TAB 3: PRIVACY */}
          {activeTab === 'privacy' && (
            <div className="space-y-5">
              <div className="flex items-center justify-between p-3.5 rounded-xl bg-surface-container-low/50 border border-border-hairline">
                <div className="space-y-0.5 max-w-sm">
                  <div className="text-xs font-bold text-primary">Read Receipts</div>
                  <div className="text-xs text-on-surface-variant leading-relaxed">
                    Show blue checkmarks when messages have been seen. When disabled, you will neither send nor receive read confirmations.
                  </div>
                </div>
                <label className="relative inline-flex items-center cursor-pointer shrink-0 ml-4">
                  <input
                    type="checkbox"
                    checked={preferences.read_receipts_enabled}
                    onChange={(e) => updatePreferences({ read_receipts_enabled: e.target.checked })}
                    className="sr-only peer"
                  />
                  <div className="w-9 h-5 bg-surface-container-high peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-4 after:w-4 after:transition-all peer-checked:bg-saffron"></div>
                </label>
              </div>
            </div>
          )}

          {/* TAB 4: PROFILE */}
          {activeTab === 'profile' && (
            <form onSubmit={handleSaveProfile} className="space-y-4">
              {profileMessage && (
                <div className="p-3 rounded-lg bg-emerald-500/10 border border-emerald-500/25 text-xs text-india-green dark:text-emerald-400 font-semibold">
                  {profileMessage}
                </div>
              )}
              {profileError && (
                <div className="p-3 rounded-lg bg-red-500/10 border border-red-500/25 text-xs text-error dark:text-red-400 font-semibold">
                  {profileError}
                </div>
              )}

              <div>
                <label className="text-xs font-bold text-primary block mb-1">Full Name</label>
                <input
                  type="text"
                  value={fullName}
                  onChange={(e) => setFullName(e.target.value)}
                  className="w-full px-3 py-2 bg-surface-container-low focus:bg-surface-container-lowest text-xs text-on-surface rounded-lg border border-border-hairline focus:border-secondary outline-none transition-all"
                  placeholder="Your Full Name"
                />
              </div>

              <div>
                <label className="text-xs font-bold text-primary block mb-1">Bio</label>
                <textarea
                  value={bio}
                  onChange={(e) => setBio(e.target.value)}
                  rows={3}
                  className="w-full px-3 py-2 bg-surface-container-low focus:bg-surface-container-lowest text-xs text-on-surface rounded-lg border border-border-hairline focus:border-secondary outline-none transition-all resize-none"
                  placeholder="Write a few words about yourself..."
                />
              </div>

              <div className="grid grid-cols-2 gap-3 text-xs font-mono text-outline">
                <div>
                  <span className="block text-[10px] uppercase font-bold text-outline/60">Username</span>
                  <span className="text-on-surface-variant font-semibold">@{user?.username}</span>
                </div>
                <div>
                  <span className="block text-[10px] uppercase font-bold text-outline/60">Email</span>
                  <span className="text-on-surface-variant font-semibold">{user?.email}</span>
                </div>
              </div>

              <div className="flex justify-end pt-2">
                <button
                  type="submit"
                  disabled={profileSaving}
                  className="px-4 py-2 bg-primary hover:bg-primary-hover text-white text-xs font-bold rounded-lg shadow-xs cursor-pointer transition-colors disabled:opacity-50"
                >
                  {profileSaving ? 'Saving...' : 'Save Changes'}
                </button>
              </div>
            </form>
          )}
        </div>

        {/* Footer */}
        <div className="px-6 py-3 border-t border-border-hairline bg-surface-container-low/20 flex justify-end">
          <button
            type="button"
            onClick={onClose}
            className="px-4 py-1.5 text-xs font-semibold text-primary hover:bg-surface-container rounded-lg border border-border-hairline transition-colors cursor-pointer"
          >
            Done
          </button>
        </div>
      </div>
    </div>
  );
}
