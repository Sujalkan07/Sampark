import React, { useState, useEffect } from 'react';
import { useAuth } from '../context/AuthContext';
import { api } from '../api/client';
import { TricolourBadge } from './TricolourBadge';
import { AlertBanner } from './AlertBanner';
import { getPresenceStatus, formatJoinedDate, parseUtcDate } from '../utils/date';

export function ProfileModal({ isOpen, onClose, mode = 'view', targetUser = null, onProfileUpdated }) {
  const { user: currentUser, updateUser } = useAuth();

  const isSelf = mode === 'edit' || (targetUser && targetUser.id === currentUser?.id);
  const displayUser = isSelf ? currentUser : (targetUser || currentUser);

  const [activeTab, setActiveTab] = useState('profile'); // 'profile' or 'password'
  
  // Profile edit fields
  const [fullName, setFullName] = useState('');
  const [bio, setBio] = useState('');
  const [savingProfile, setSavingProfile] = useState(false);
  const [profileError, setProfileError] = useState(null);
  const [profileSuccess, setProfileSuccess] = useState(null);

  // Password fields
  const [currentPassword, setCurrentPassword] = useState('');
  const [newPassword, setNewPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [savingPassword, setSavingPassword] = useState(false);
  const [passwordError, setPasswordError] = useState(null);
  const [passwordSuccess, setPasswordSuccess] = useState(null);

  useEffect(() => {
    if (isOpen && displayUser) {
      setFullName(displayUser.fullName || '');
      setBio(displayUser.bio || '');
      setProfileError(null);
      setProfileSuccess(null);
      setCurrentPassword('');
      setNewPassword('');
      setConfirmPassword('');
      setPasswordError(null);
      setPasswordSuccess(null);
      setActiveTab('profile');
    }
  }, [isOpen, displayUser]);

  if (!isOpen || !displayUser) return null;

  // Presence calculation via normalized UTC parser
  const presence = getPresenceStatus(displayUser.lastActiveAt);

  const initials = (displayUser.fullName || displayUser.username || 'U')
    .split(' ')
    .map((n) => n[0])
    .join('')
    .substring(0, 2)
    .toUpperCase();

  const handleSaveProfile = async (e) => {
    e?.preventDefault?.();
    const trimmedName = fullName.trim();
    if (!trimmedName) {
      setProfileError('Full name cannot be empty.');
      return;
    }
    if (bio.length > 300) {
      setProfileError('Bio cannot exceed 300 characters.');
      return;
    }

    try {
      setSavingProfile(true);
      setProfileError(null);
      setProfileSuccess(null);

      const res = await api.updateProfile({ fullName: trimmedName, bio: bio.trim() });
      if (res && res.user) {
        updateUser(res.user);
        onProfileUpdated?.(res.user);
        setProfileSuccess('Profile updated successfully.');
      } else {
        throw new Error('Failed to update profile.');
      }
    } catch (err) {
      setProfileError(err.message || 'Unable to update profile.');
    } finally {
      setSavingProfile(false);
    }
  };

  const handleSavePassword = async (e) => {
    e?.preventDefault?.();
    if (!currentPassword) {
      setPasswordError('Current password is required.');
      return;
    }
    if (!newPassword || newPassword.length < 8) {
      setPasswordError('New password must be at least 8 characters long.');
      return;
    }
    if (newPassword !== confirmPassword) {
      setPasswordError('New password and confirmation do not match.');
      return;
    }

    try {
      setSavingPassword(true);
      setPasswordError(null);
      setPasswordSuccess(null);

      await api.changePassword({ currentPassword, newPassword });
      setPasswordSuccess('Password changed successfully.');
      setCurrentPassword('');
      setNewPassword('');
      setConfirmPassword('');
    } catch (err) {
      setPasswordError(err.message || 'Unable to change password.');
    } finally {
      setSavingPassword(false);
    }
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/40 backdrop-blur-xs animate-fade-in">
      <div
        className="w-full max-w-md bg-surface-container-lowest border border-border-hairline rounded-2xl shadow-xl overflow-hidden flex flex-col max-h-[90vh]"
        onClick={(e) => e.stopPropagation()}
      >
        {/* Modal Header */}
        <div className="p-5 pb-4 border-b border-border-hairline flex items-center justify-between">
          <div className="flex items-center gap-2">
            <h3 className="text-base font-bold text-primary">
              {isSelf ? 'My Profile & Account' : 'Member Profile'}
            </h3>
            <TricolourBadge size="sm" />
          </div>
          <button
            onClick={onClose}
            type="button"
            className="w-8 h-8 rounded-lg hover:bg-surface-container flex items-center justify-center text-on-surface-variant transition-colors cursor-pointer"
            title="Close"
          >
            <span className="material-symbols-outlined text-[20px]">close</span>
          </button>
        </div>

        {/* User Card Hero */}
        <div className="p-5 pb-4 bg-surface-container-low/50 border-b border-border-hairline flex items-center gap-4">
          <div className="relative shrink-0">
            <div className="w-14 h-14 rounded-2xl bg-primary text-white font-bold flex items-center justify-center text-lg shadow-xs select-none">
              {initials}
            </div>
            {presence.isOnline && (
              <span
                className="absolute -bottom-0.5 -right-0.5 w-3.5 h-3.5 bg-india-green rounded-full ring-2 ring-surface-container-lowest"
                title="Active now"
              ></span>
            )}
          </div>

          <div className="min-w-0 flex-1">
            <div className="flex items-center gap-1.5">
              <span className="text-base font-bold text-primary truncate">
                {displayUser.fullName}
              </span>
              {isSelf && (
                <span className="px-1.5 py-0.2 rounded-full bg-saffron/15 text-saffron text-[10px] font-bold">
                  You
                </span>
              )}
            </div>
            <div className="text-xs font-mono text-outline">@{displayUser.username}</div>

            {/* Genuine telemetry presence pill */}
            <div className="mt-1 flex items-center gap-1.5 text-[11px] font-mono text-outline">
              <span
                className={`w-2 h-2 rounded-full ${
                  presence.isOnline ? 'bg-india-green animate-pulse' : 'bg-outline/40'
                }`}
              ></span>
              <span>{presence.label}</span>
            </div>
          </div>
        </div>

        {/* Tab switcher for Self */}
        {isSelf && (
          <div className="flex border-b border-border-hairline bg-surface-container-lowest px-5 pt-2 gap-4">
            <button
              type="button"
              onClick={() => setActiveTab('profile')}
              className={`pb-2.5 text-xs font-semibold border-b-2 transition-all cursor-pointer ${
                activeTab === 'profile'
                  ? 'border-primary text-primary'
                  : 'border-transparent text-on-surface-variant hover:text-on-surface'
              }`}
            >
              General Profile
            </button>
            <button
              type="button"
              onClick={() => setActiveTab('password')}
              className={`pb-2.5 text-xs font-semibold border-b-2 transition-all cursor-pointer ${
                activeTab === 'password'
                  ? 'border-primary text-primary'
                  : 'border-transparent text-on-surface-variant hover:text-on-surface'
              }`}
            >
              Security & Password
            </button>
          </div>
        )}

        {/* Modal Body */}
        <div className="p-5 overflow-y-auto flex-1 space-y-4">
          {/* View Mode (Peer) */}
          {!isSelf && (
            <div className="space-y-4">
              <div>
                <label className="text-[11px] font-semibold text-outline uppercase tracking-wider block mb-1">
                  About / Bio
                </label>
                <div className="p-3 rounded-xl bg-surface-container-low text-xs text-on-surface leading-relaxed min-h-[60px]">
                  {displayUser.bio ? displayUser.bio : <span className="text-outline italic">No bio provided.</span>}
                </div>
              </div>

              <div>
                <label className="text-[11px] font-semibold text-outline uppercase tracking-wider block mb-1">
                  Member Since
                </label>
                <div className="text-xs font-mono text-on-surface-variant">
                  {displayUser.createdAt && parseUtcDate(displayUser.createdAt)
                    ? parseUtcDate(displayUser.createdAt).toLocaleDateString(undefined, {
                        month: 'long',
                        day: 'numeric',
                        year: 'numeric',
                      })
                    : 'Registered Member'}
                </div>
              </div>

              <div className="pt-2 flex justify-end">
                <button
                  type="button"
                  onClick={onClose}
                  className="px-4 py-2 rounded-xl bg-surface-container text-xs font-semibold text-primary hover:bg-surface-container-high transition-colors cursor-pointer"
                >
                  Close
                </button>
              </div>
            </div>
          )}

          {/* Edit Mode: Profile Tab */}
          {isSelf && activeTab === 'profile' && (
            <form onSubmit={handleSaveProfile} className="space-y-3.5">
              {profileError && (
                <AlertBanner type="error" message={profileError} onClose={() => setProfileError(null)} />
              )}
              {profileSuccess && (
                <AlertBanner type="success" message={profileSuccess} onClose={() => setProfileSuccess(null)} />
              )}

              <div>
                <label className="text-xs font-semibold text-primary block mb-1">
                  Display Full Name
                </label>
                <input
                  type="text"
                  value={fullName}
                  onChange={(e) => setFullName(e.target.value)}
                  className="w-full px-3 py-2 bg-surface-container-low focus:bg-surface-container-lowest text-xs text-on-surface rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all shadow-xs"
                  placeholder="Your full name..."
                  required
                />
              </div>

              <div>
                <div className="flex items-center justify-between mb-1">
                  <label className="text-xs font-semibold text-primary">
                    Bio / Status
                  </label>
                  <span className="text-[10px] font-mono text-outline">{bio.length}/300</span>
                </div>
                <textarea
                  value={bio}
                  onChange={(e) => setBio(e.target.value)}
                  maxLength={300}
                  rows={3}
                  className="w-full px-3 py-2 bg-surface-container-low focus:bg-surface-container-lowest text-xs text-on-surface rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all resize-none shadow-xs"
                  placeholder="Share a short bio or technical status..."
                ></textarea>
              </div>

              <div className="flex items-center justify-end gap-2 pt-2">
                <button
                  type="button"
                  onClick={onClose}
                  className="px-4 py-2 rounded-xl text-xs font-semibold text-on-surface-variant hover:bg-surface-container transition-colors cursor-pointer"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={savingProfile}
                  className="px-4 py-2 rounded-xl bg-primary hover:bg-primary-hover active:scale-95 text-white text-xs font-semibold shadow-xs transition-all cursor-pointer disabled:opacity-50 flex items-center gap-1.5"
                >
                  {savingProfile ? (
                    <span className="w-3.5 h-3.5 border-2 border-white border-t-transparent rounded-full animate-spin"></span>
                  ) : (
                    <span className="material-symbols-outlined text-[16px]">check</span>
                  )}
                  <span>Save Profile</span>
                </button>
              </div>
            </form>
          )}

          {/* Edit Mode: Password Tab */}
          {isSelf && activeTab === 'password' && (
            <form onSubmit={handleSavePassword} className="space-y-3.5">
              {passwordError && (
                <AlertBanner type="error" message={passwordError} onClose={() => setPasswordError(null)} />
              )}
              {passwordSuccess && (
                <AlertBanner type="success" message={passwordSuccess} onClose={() => setPasswordSuccess(null)} />
              )}

              <div>
                <label className="text-xs font-semibold text-primary block mb-1">
                  Current Password
                </label>
                <input
                  type="password"
                  value={currentPassword}
                  onChange={(e) => setCurrentPassword(e.target.value)}
                  className="w-full px-3 py-2 bg-surface-container-low focus:bg-surface-container-lowest text-xs text-on-surface rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all shadow-xs"
                  placeholder="Enter current password..."
                  required
                />
              </div>

              <div>
                <label className="text-xs font-semibold text-primary block mb-1">
                  New Password (min 8 characters)
                </label>
                <input
                  type="password"
                  value={newPassword}
                  onChange={(e) => setNewPassword(e.target.value)}
                  className="w-full px-3 py-2 bg-surface-container-low focus:bg-surface-container-lowest text-xs text-on-surface rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all shadow-xs"
                  placeholder="Enter new password..."
                  required
                />
              </div>

              <div>
                <label className="text-xs font-semibold text-primary block mb-1">
                  Confirm New Password
                </label>
                <input
                  type="password"
                  value={confirmPassword}
                  onChange={(e) => setConfirmPassword(e.target.value)}
                  className="w-full px-3 py-2 bg-surface-container-low focus:bg-surface-container-lowest text-xs text-on-surface rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all shadow-xs"
                  placeholder="Re-enter new password..."
                  required
                />
              </div>

              <div className="flex items-center justify-end gap-2 pt-2">
                <button
                  type="button"
                  onClick={onClose}
                  className="px-4 py-2 rounded-xl text-xs font-semibold text-on-surface-variant hover:bg-surface-container transition-colors cursor-pointer"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={savingPassword}
                  className="px-4 py-2 rounded-xl bg-primary hover:bg-primary-hover active:scale-95 text-white text-xs font-semibold shadow-xs transition-all cursor-pointer disabled:opacity-50 flex items-center gap-1.5"
                >
                  {savingPassword ? (
                    <span className="w-3.5 h-3.5 border-2 border-white border-t-transparent rounded-full animate-spin"></span>
                  ) : (
                    <span className="material-symbols-outlined text-[16px]">lock_reset</span>
                  )}
                  <span>Update Password</span>
                </button>
              </div>
            </form>
          )}
        </div>
      </div>
    </div>
  );
}

export default ProfileModal;
