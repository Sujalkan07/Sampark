import React, { useState, useEffect, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { usePreferences } from '../context/PreferencesContext';
import { api } from '../api/client';
import { Sidebar } from '../components/Sidebar';
import { TricolourBadge } from '../components/TricolourBadge';
import { AlertBanner } from '../components/AlertBanner';
import { ProfileModal } from '../components/ProfileModal';
import { formatJoinedDate } from '../utils/date';

export function FindPeoplePage() {
  const { user } = useAuth();
  const { isDarkMode, toggleTheme } = usePreferences();
  const navigate = useNavigate();

  const [searchQuery, setSearchQuery] = useState('');
  const [users, setUsers] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [notice, setNotice] = useState(null);
  const [connectingUserId, setConnectingUserId] = useState(null);

  const [selfProfileOpen, setSelfProfileOpen] = useState(false);
  const [viewingPeerId, setViewingPeerId] = useState(null);

  const fetchUsers = useCallback(async (query = '') => {
    try {
      setLoading(true);
      setError(null);
      const data = await api.searchUsers(query.trim());
      setUsers(data?.users || []);
    } catch (err) {
      setError(err.message || 'Failed to search members. Please check your connection.');
      setUsers([]);
    } finally {
      setLoading(false);
    }
  }, []);

  // Debounced search effect
  useEffect(() => {
    let ignore = false;
    const timer = setTimeout(() => {
      if (!ignore) {
        fetchUsers(searchQuery);
      }
    }, 250);

    return () => {
      ignore = true;
      clearTimeout(timer);
    };
  }, [searchQuery, fetchUsers]);

  const handleMessageClick = async (targetUser) => {
    if (connectingUserId) return;
    try {
      setConnectingUserId(targetUser.id);
      setError(null);
      const res = await api.createOrGetConversation(targetUser.id);
      if (res && res.conversation && res.conversation.id) {
        navigate(`/dashboard/${res.conversation.id}`);
      } else {
        throw new Error('Failed to open conversation. Please try again.');
      }
    } catch (err) {
      setError(err.message || 'Unable to open conversation with this member.');
    } finally {
      setConnectingUserId(null);
    }
  };

  return (
    <div className="min-h-screen bg-surface flex text-on-surface">
      <Sidebar />

      <div className="pl-[68px] flex-1 flex flex-col h-screen overflow-hidden">
        {/* Top Header */}
        <header className="h-14 bg-surface-container-lowest border-b border-border-hairline z-40 flex items-center justify-between px-6 shrink-0 transition-colors">
          <div className="flex items-center gap-3">
            <div
              className="h-8 px-2 py-0.5 rounded-lg bg-white border border-border-hairline flex items-center shadow-xs cursor-pointer"
              onClick={() => navigate('/dashboard')}
              title="Sampark Dashboard"
            >
              <img
                src="/sampark-logo.png"
                alt="Sampark"
                className="h-6 w-auto object-contain"
              />
            </div>
            <span className="text-outline/50">•</span>
            <span className="text-sm font-medium text-on-surface-variant">Find People</span>
          </div>

          <div className="flex items-center gap-3">
            {/* Quick Theme Toggle Button */}
            <button
              type="button"
              onClick={toggleTheme}
              title={isDarkMode ? 'Switch to Light Mode' : 'Switch to Dark Mode'}
              aria-label={isDarkMode ? 'Switch to Light Mode' : 'Switch to Dark Mode'}
              className="w-8 h-8 rounded-lg bg-surface-container-low hover:bg-surface-container text-on-surface-variant hover:text-on-surface transition-all flex items-center justify-center cursor-pointer shadow-xs active:scale-95"
            >
              <span className={`material-symbols-outlined text-[19px] ${isDarkMode ? 'text-amber-400' : 'text-slate-700'}`}>
                {isDarkMode ? 'light_mode' : 'dark_mode'}
              </span>
            </button>

            <div className="w-8 h-8 rounded-full bg-primary flex items-center justify-center text-white text-xs font-semibold shadow-xs">
              {(user?.fullName || user?.username || 'U')[0].toUpperCase()}
            </div>
          </div>
        </header>

        {/* Directory Container */}
        <div className="flex-1 overflow-y-auto p-4 sm:p-6 lg:p-8 max-w-5xl mx-auto w-full">
          {/* Header Bar */}
          <div className="mb-6">
            <div className="flex items-center gap-2 mb-1">
              <span className="text-xs uppercase tracking-widest font-mono text-on-surface-variant">
                Member Directory
              </span>
              <TricolourBadge size="sm" />
            </div>
            <h1 className="text-2xl font-bold text-primary tracking-tight">Find People</h1>
            <p className="text-sm text-on-surface-variant mt-1">
              Discover and connect with registered members on the Sampark network.
            </p>
          </div>

          {/* Search Bar */}
          <div className="relative w-full mb-6">
            <span className="material-symbols-outlined absolute left-3.5 top-1/2 -translate-y-1/2 text-outline text-[20px] pointer-events-none">
              search
            </span>
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search by name or @username..."
              className="w-full pl-11 pr-10 py-2.5 bg-surface-container-lowest text-sm text-on-surface placeholder:text-outline rounded-xl border border-border-hairline focus:border-secondary focus:outline-none shadow-xs transition-all"
            />
            {searchQuery && (
              <button
                type="button"
                onClick={() => setSearchQuery('')}
                className="absolute right-3 top-1/2 -translate-y-1/2 text-on-surface-variant hover:text-on-surface p-1"
                title="Clear search"
              >
                <span className="material-symbols-outlined text-[16px]">close</span>
              </button>
            )}
          </div>

          {/* Action Notice Modal / Banner */}
          {notice && (
            <div className="mb-6">
              <div className="p-4 rounded-xl bg-blue-50 border border-blue-200 text-xs text-blue-900 flex items-start justify-between gap-3 shadow-xs">
                <div className="flex items-start gap-2.5">
                  <span className="material-symbols-outlined text-[18px] text-secondary shrink-0 mt-0.5">
                    info
                  </span>
                  <div>
                    <p className="font-semibold">{notice}</p>
                    <button
                      type="button"
                      onClick={() => navigate('/dashboard')}
                      className="text-secondary hover:underline font-semibold mt-1 inline-block"
                    >
                      View Messages in Dashboard →
                    </button>
                  </div>
                </div>
                <button
                  type="button"
                  onClick={() => setNotice(null)}
                  className="text-blue-700 hover:text-blue-900 font-bold p-1"
                >
                  ×
                </button>
              </div>
            </div>
          )}

          {/* Current User Record (Your Profile) */}
          <div className="mb-6">
            <h2 className="text-xs font-semibold uppercase tracking-wider text-outline mb-3">
              Your Profile
            </h2>
            <div className="p-4 rounded-xl bg-surface-container-lowest border border-border-hairline shadow-xs flex items-center justify-between">
              <div className="flex items-center gap-3.5">
                <div className="relative">
                  <div className="w-12 h-12 rounded-full bg-primary text-white font-bold flex items-center justify-center text-sm shadow-xs">
                    {(user?.fullName || user?.username || 'U')
                      .split(' ')
                      .map((n) => n[0])
                      .join('')
                      .substring(0, 2)
                      .toUpperCase()}
                  </div>
                  <span className="absolute bottom-0 right-0 w-3 h-3 bg-india-green rounded-full ring-2 ring-surface-container-lowest"></span>
                </div>
                <div>
                  <div className="flex items-center gap-2">
                    <span className="text-sm font-bold text-primary">{user?.fullName}</span>
                    <span className="text-xs font-mono text-outline">@{user?.username}</span>
                    <span className="px-2 py-0.5 rounded-full bg-saffron/15 text-saffron text-[10px] font-semibold">
                      You
                    </span>
                  </div>
                  <div className="text-xs text-on-surface-variant mt-0.5">
                    {user?.email}
                  </div>
                  {user?.bio && (
                    <div className="text-xs text-on-surface mt-1.5 italic line-clamp-2 max-w-md">
                      "{user.bio}"
                    </div>
                  )}
                </div>
              </div>

              <div className="flex items-center gap-2">
                <button
                  type="button"
                  onClick={() => setSelfProfileOpen(true)}
                  className="px-3.5 py-1.5 text-xs font-semibold rounded-lg bg-surface-container-high hover:bg-surface-container text-primary border border-border-hairline transition-colors flex items-center gap-1.5 cursor-pointer"
                >
                  <span className="material-symbols-outlined text-[16px]">edit</span>
                  <span>Edit Profile</span>
                </button>
                <button
                  onClick={() => navigate('/dashboard')}
                  className="px-3.5 py-1.5 text-xs font-semibold rounded-lg bg-surface-container-low hover:bg-surface-container text-primary border border-border-hairline transition-colors cursor-pointer"
                >
                  Back to Chats
                </button>
              </div>
            </div>
          </div>

          {/* Discovered Users Section */}
          <div className="mb-6">
            <div className="flex items-center justify-between mb-3">
              <h2 className="text-xs font-semibold uppercase tracking-wider text-outline">
                Registered Members
              </h2>
              {!loading && (
                <span className="text-xs font-mono text-outline">
                  {users.length} {users.length === 1 ? 'member' : 'members'} found
                </span>
              )}
            </div>

            {/* Error State */}
            {error && (
              <div className="mb-4">
                <AlertBanner
                  type="error"
                  message={error}
                  onClose={() => setError(null)}
                />
                <button
                  onClick={() => fetchUsers(searchQuery)}
                  className="mt-2 text-xs font-semibold text-secondary hover:underline"
                >
                  Click here to retry
                </button>
              </div>
            )}

            {/* Loading State: Skeletons */}
            {loading && (
              <div className="space-y-3">
                {[1, 2, 3].map((n) => (
                  <div
                    key={n}
                    className="p-4 rounded-xl bg-surface-container-lowest border border-border-hairline flex items-center justify-between animate-pulse"
                  >
                    <div className="flex items-center gap-3">
                      <div className="w-11 h-11 rounded-full bg-surface-container-high"></div>
                      <div className="space-y-2">
                        <div className="h-4 w-32 bg-surface-container-high rounded"></div>
                        <div className="h-3 w-24 bg-surface-container rounded"></div>
                      </div>
                    </div>
                    <div className="h-8 w-20 bg-surface-container-high rounded-lg"></div>
                  </div>
                ))}
              </div>
            )}

            {/* Empty State: No results */}
            {!loading && !error && users.length === 0 && (
              <div className="p-8 rounded-2xl bg-surface-container-low border border-border-hairline text-center flex flex-col items-center">
                <div className="w-12 h-12 rounded-2xl bg-surface-container-lowest border border-border-hairline flex items-center justify-center text-primary mb-3 shadow-xs">
                  <span className="material-symbols-outlined text-[24px]">person_search</span>
                </div>
                {searchQuery ? (
                  <>
                    <h3 className="text-base font-bold text-primary mb-1">
                      No members matching "{searchQuery}"
                    </h3>
                    <p className="text-xs text-on-surface-variant max-w-md leading-relaxed mb-4">
                      Check your spelling or try searching for another name or handle.
                    </p>
                    <button
                      type="button"
                      onClick={() => setSearchQuery('')}
                      className="px-3.5 py-1.5 text-xs font-semibold rounded-lg bg-surface-container-lowest border border-border-hairline hover:bg-surface-container text-primary transition-colors shadow-xs"
                    >
                      Clear Search
                    </button>
                  </>
                ) : (
                  <>
                    <h3 className="text-base font-bold text-primary mb-1">
                      No other registered members yet
                    </h3>
                    <p className="text-xs text-on-surface-variant max-w-md leading-relaxed">
                      As soon as other members register on Sampark, they will automatically appear here.
                    </p>
                  </>
                )}
              </div>
            )}

            {/* Populated List: Real Registered Accounts */}
            {!loading && !error && users.length > 0 && (
              <div className="space-y-3">
                {users.map((peer) => {
                  const initials = (peer.fullName || peer.username || 'U')
                    .split(' ')
                    .map((n) => n[0])
                    .join('')
                    .substring(0, 2)
                    .toUpperCase();

                  const joinedDate = formatJoinedDate(peer.createdAt);

                  return (
                    <div
                      key={peer.id}
                      className="p-4 rounded-xl bg-surface-container-lowest border border-border-hairline hover:border-border-hairline/80 shadow-xs hover:shadow-sm transition-all flex items-center justify-between"
                    >
                      <div
                        className="flex items-center gap-3.5 min-w-0 cursor-pointer group/peer"
                        onClick={() => setViewingPeerId(peer.id)}
                        title="Click to view profile"
                      >
                        <div className="relative shrink-0">
                          <div className="w-11 h-11 rounded-full bg-primary-container text-white font-bold flex items-center justify-center text-xs shadow-xs select-none group-hover/peer:ring-2 group-hover/peer:ring-primary/40 transition-all">
                            {initials}
                          </div>
                        </div>
                        <div className="min-w-0">
                          <div className="flex items-center gap-2">
                            <span className="text-sm font-bold text-primary truncate group-hover/peer:underline">
                              {peer.fullName}
                            </span>
                            <span className="text-xs font-mono text-outline shrink-0">
                              @{peer.username}
                            </span>
                          </div>
                          <div className="text-xs text-on-surface-variant mt-0.5">
                            Member since {joinedDate}
                          </div>
                          {peer.bio && (
                            <div className="text-xs text-on-surface-variant mt-1 line-clamp-1 italic">
                              "{peer.bio}"
                            </div>
                          )}
                        </div>
                      </div>

                      <div className="shrink-0 ml-4 flex items-center gap-2">
                        <button
                          type="button"
                          onClick={() => setViewingPeerId(peer.id)}
                          className="px-2.5 py-1.5 rounded-lg bg-surface-container hover:bg-surface-container-high text-on-surface-variant text-xs font-semibold flex items-center gap-1 border border-border-hairline transition-all cursor-pointer"
                          title="View Profile"
                        >
                          <span className="material-symbols-outlined text-[15px]">badge</span>
                          <span className="hidden sm:inline">Profile</span>
                        </button>
                        <button
                          type="button"
                          disabled={connectingUserId === peer.id}
                          onClick={() => handleMessageClick(peer)}
                          className="px-3.5 py-1.5 rounded-lg bg-primary hover:bg-primary-hover active:scale-95 text-white text-xs font-semibold flex items-center gap-1.5 shadow-xs transition-all cursor-pointer disabled:opacity-60 disabled:cursor-not-allowed"
                        >
                          {connectingUserId === peer.id ? (
                            <>
                              <span className="w-3.5 h-3.5 border-2 border-white border-t-transparent rounded-full animate-spin"></span>
                              <span>Connecting...</span>
                            </>
                          ) : (
                            <>
                              <span className="material-symbols-outlined text-[16px]">chat</span>
                              <span>Message</span>
                            </>
                          )}
                        </button>
                      </div>
                    </div>
                  );
                })}
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Self Profile Modal (Edit Mode) */}
      <ProfileModal
        isOpen={selfProfileOpen}
        onClose={() => setSelfProfileOpen(false)}
        mode="edit"
      />

      {/* Peer Profile Modal (View Mode) */}
      <ProfileModal
        isOpen={Boolean(viewingPeerId)}
        onClose={() => setViewingPeerId(null)}
        mode="view"
        userId={viewingPeerId}
      />
    </div>
  );
}
