import React, { useState, useEffect, useRef, useMemo, useCallback } from 'react';
import { useNavigate, useParams } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { api } from '../api/client';
import { Sidebar } from '../components/Sidebar';
import { TricolourBadge } from '../components/TricolourBadge';
import { AlertBanner } from '../components/AlertBanner';
import { ChatPanel } from '../components/ChatPanel';
import { ProfileModal } from '../components/ProfileModal';

export function DashboardPage() {
  const { user } = useAuth();
  const navigate = useNavigate();
  const { conversationId } = useParams();

  const [conversations, setConversations] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [activeTab, setActiveTab] = useState('all'); // 'all', 'unread', 'direct', 'messages'

  const [selfProfileOpen, setSelfProfileOpen] = useState(false);
  const [globalResults, setGlobalResults] = useState([]);
  const [globalLoading, setGlobalLoading] = useState(false);

  const pollIntervalRef = useRef(null);
  const activeConversationIdRef = useRef(conversationId);
  activeConversationIdRef.current = conversationId;

  const fetchConversations = useCallback(async (isInitial = false) => {
    try {
      if (isInitial) setLoading(true);
      const data = await api.getConversations();
      const rawList = data?.conversations || [];

      // If a conversation is currently open, force its unreadCount to 0 locally
      const currentActiveId = activeConversationIdRef.current;
      const normalizedList = rawList.map((c) => {
        if (currentActiveId && String(c.id) === String(currentActiveId)) {
          return { ...c, unreadCount: 0 };
        }
        return c;
      });

      setConversations(normalizedList);
      setError(null);
    } catch (err) {
      if (isInitial) {
        setError(err.message || 'Failed to load conversations.');
      }
    } finally {
      if (isInitial) setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchConversations(true);

    // Poll for new incoming conversations or activity updates every 4 seconds
    pollIntervalRef.current = setInterval(() => {
      fetchConversations(false);
    }, 4000);

    return () => {
      if (pollIntervalRef.current) {
        clearInterval(pollIntervalRef.current);
      }
    };
  }, [fetchConversations]);

  // Debounced global message search effect when 'messages' tab is selected
  useEffect(() => {
    if (activeTab !== 'messages') return;
    if (!searchQuery.trim()) {
      setGlobalResults([]);
      setGlobalLoading(false);
      return;
    }

    let ignore = false;
    setGlobalLoading(true);
    const timer = setTimeout(async () => {
      try {
        const res = await api.searchGlobalMessages(searchQuery.trim());
        if (!ignore) {
          setGlobalResults(res?.messages || []);
        }
      } catch (err) {
        if (!ignore) setGlobalResults([]);
      } finally {
        if (!ignore) setGlobalLoading(false);
      }
    }, 250);

    return () => {
      ignore = true;
      clearTimeout(timer);
    };
  }, [searchQuery, activeTab]);

  // When conversationId changes in URL, clear unread count for that conversation in state
  useEffect(() => {
    if (conversationId) {
      setConversations((prev) =>
        prev.map((c) =>
          String(c.id) === String(conversationId) ? { ...c, unreadCount: 0 } : c
        )
      );
    }
  }, [conversationId]);

  // Callback from ChatPanel whenever a message is sent or received
  const handleConversationActivity = useCallback(
    ({ conversationId: activityId, lastMessage }) => {
      setConversations((prev) => {
        const index = prev.findIndex((c) => String(c.id) === String(activityId));
        if (index === -1) {
          fetchConversations(false);
          return prev;
        }

        const current = prev[index];
        const isCurrentOpen =
          activeConversationIdRef.current &&
          String(activeConversationIdRef.current) === String(activityId);

        const updated = {
          ...current,
          lastMessage,
          updatedAt: lastMessage.createdAt,
          unreadCount: isCurrentOpen ? 0 : (current.unreadCount || 0) + 1,
        };

        const remaining = prev.filter((_, i) => i !== index);
        return [updated, ...remaining];
      });
    },
    [fetchConversations]
  );

  // Format preview timestamp
  const formatPreviewTime = (isoString) => {
    if (!isoString) return '';
    try {
      const d = new Date(isoString);
      const today = new Date();
      if (d.toDateString() === today.toDateString()) {
        return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
      }
      return d.toLocaleDateString(undefined, { month: 'short', day: 'numeric' });
    } catch {
      return '';
    }
  };

  // Filter conversations
  const filteredConversations = useMemo(() => {
    return conversations.filter((c) => {
      const other = c.participant;
      const name = other?.fullName?.toLowerCase() || '';
      const handle = other?.username?.toLowerCase() || '';
      const q = searchQuery.toLowerCase().trim();

      const matchesSearch = !q || name.includes(q) || handle.includes(q);
      if (!matchesSearch) return false;

      if (activeTab === 'unread') {
        return c.unreadCount > 0;
      }
      return true;
    });
  }, [conversations, searchQuery, activeTab]);

  const totalUnread = useMemo(() => {
    return conversations.reduce((sum, c) => sum + (c.unreadCount || 0), 0);
  }, [conversations]);

  return (
    <div className="min-h-screen bg-surface flex text-on-surface">
      {/* 68px Fixed Sidebar */}
      <Sidebar />

      {/* Main Content Area offset by 68px */}
      <div className="pl-[68px] flex-1 flex flex-col h-screen overflow-hidden">
        {/* Top Header Bar */}
        <header className="h-14 bg-surface-container-lowest border-b border-border-hairline z-40 flex items-center justify-between px-6 shrink-0">
          <div className="flex items-center gap-3">
            <img
              src="/sampark-logo.png"
              alt="Sampark"
              className="h-7 w-auto object-contain cursor-pointer"
              onClick={() => navigate('/dashboard')}
            />
            <span className="text-outline/50">•</span>
            <span className="text-sm font-medium text-on-surface-variant">Messages</span>
          </div>

          <div className="flex items-center gap-4">
            <div className="hidden sm:flex items-center gap-2 px-2.5 py-1 rounded-lg bg-surface-container-low border border-border-hairline text-xs font-mono text-on-surface-variant">
              <span className="w-2 h-2 rounded-full bg-india-green"></span>
              <span>Logged in as @{user?.username}</span>
            </div>

            <button
              type="button"
              onClick={() => setSelfProfileOpen(true)}
              className="w-8 h-8 rounded-full bg-primary flex items-center justify-center text-white text-xs font-semibold shadow-xs hover:ring-2 hover:ring-primary/40 transition-all cursor-pointer"
              title="Edit Profile"
            >
              {(user?.fullName || user?.username || 'U')[0].toUpperCase()}
            </button>
          </div>
        </header>

        {/* Dashboard Body: Split View */}
        <div className="flex-1 flex overflow-hidden">
          {/* Conversation List Sidebar: visible always on desktop, hidden on mobile if chat open */}
          <aside
            className={`${
              conversationId ? 'hidden md:flex' : 'flex'
            } w-full md:w-[340px] shrink-0 bg-surface-container-lowest border-r border-border-hairline flex-col h-full z-10`}
          >
            {/* Header */}
            <div className="p-4 sm:p-5 pb-3">
              <div className="flex items-center justify-between mb-4">
                <div className="flex items-center gap-2.5">
                  <h1 className="text-lg font-bold text-primary tracking-tight">Messages</h1>
                  <span className="inline-flex items-center justify-center px-2 py-0.5 rounded-full bg-surface-container-high text-on-surface-variant text-xs font-semibold">
                    {conversations.length}
                  </span>
                </div>

                <button
                  onClick={() => navigate('/find-people')}
                  className="w-8 h-8 rounded-lg bg-surface-container-low hover:bg-surface-container text-primary flex items-center justify-center transition-all active:scale-95 group shadow-xs cursor-pointer"
                  title="Find People to message"
                  type="button"
                >
                  <span className="material-symbols-outlined text-[19px] group-hover:rotate-90 transition-transform">
                    add
                  </span>
                </button>
              </div>

              {/* Search Bar */}
              <div className="relative w-full">
                <span className="material-symbols-outlined absolute left-3 top-1/2 -translate-y-1/2 text-outline text-[18px] pointer-events-none">
                  search
                </span>
                <input
                  type="text"
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  placeholder={
                    activeTab === 'messages'
                      ? 'Search all message history...'
                      : 'Search conversations...'
                  }
                  className="w-full pl-9 pr-3 py-2 bg-surface-container-low hover:bg-surface-container focus:bg-surface-container-lowest text-xs text-on-surface placeholder:text-outline rounded-lg border border-border-hairline focus:border-secondary focus:outline-none transition-all"
                />
              </div>

              {/* Filter Chips */}
              <div className="flex items-center gap-1.5 mt-3 pt-1 overflow-x-auto no-scrollbar">
                <button
                  type="button"
                  onClick={() => setActiveTab('all')}
                  className={`px-3 py-1 rounded-full text-xs font-semibold transition-colors cursor-pointer shrink-0 ${
                    activeTab === 'all'
                      ? 'bg-primary text-white shadow-xs'
                      : 'text-on-surface-variant hover:bg-surface-container'
                  }`}
                >
                  All
                </button>
                <button
                  type="button"
                  onClick={() => setActiveTab('unread')}
                  className={`px-3 py-1 rounded-full text-xs font-semibold transition-colors flex items-center gap-1.5 cursor-pointer shrink-0 ${
                    activeTab === 'unread'
                      ? 'bg-primary text-white shadow-xs'
                      : 'text-on-surface-variant hover:bg-surface-container'
                  }`}
                >
                  <span>Unread</span>
                  {totalUnread > 0 && (
                    <span className="px-1.5 py-0.2 rounded-full bg-saffron text-[10px] text-white font-bold">
                      {totalUnread}
                    </span>
                  )}
                </button>
                <button
                  type="button"
                  onClick={() => setActiveTab('direct')}
                  className={`px-3 py-1 rounded-full text-xs font-semibold transition-colors cursor-pointer shrink-0 ${
                    activeTab === 'direct'
                      ? 'bg-primary text-white shadow-xs'
                      : 'text-on-surface-variant hover:bg-surface-container'
                  }`}
                >
                  Direct
                </button>
                <button
                  type="button"
                  onClick={() => setActiveTab('messages')}
                  className={`px-3 py-1 rounded-full text-xs font-semibold transition-colors flex items-center gap-1 cursor-pointer shrink-0 ${
                    activeTab === 'messages'
                      ? 'bg-primary text-white shadow-xs'
                      : 'text-on-surface-variant hover:bg-surface-container'
                  }`}
                  title="Search message history across all conversations"
                >
                  <span className="material-symbols-outlined text-[14px]">search</span>
                  <span>Messages</span>
                </button>
              </div>
            </div>

            {/* Error Banner */}
            {error && (
              <div className="p-3">
                <AlertBanner type="error" message={error} onClose={() => setError(null)} />
              </div>
            )}

            {/* Global Message Search Results View */}
            {activeTab === 'messages' && (
              <div className="flex-1 overflow-y-auto">
                {!searchQuery.trim() ? (
                  <div className="px-4 py-8 text-center flex flex-col items-center">
                    <div className="w-10 h-10 rounded-full bg-surface-container-high flex items-center justify-center text-on-surface-variant mb-2">
                      <span className="material-symbols-outlined text-[20px]">search</span>
                    </div>
                    <div className="text-xs font-semibold text-primary mb-1">Search Message History</div>
                    <p className="text-[11px] text-on-surface-variant max-w-[200px]">
                      Enter keywords in the search bar above to find matching messages across all your chats.
                    </p>
                  </div>
                ) : globalLoading ? (
                  <div className="p-3 space-y-2">
                    {[1, 2, 3].map((n) => (
                      <div key={n} className="p-3 rounded-xl bg-surface-container-low animate-pulse space-y-2">
                        <div className="h-3 w-28 bg-surface-container-high rounded"></div>
                        <div className="h-3.5 w-44 bg-surface-container rounded"></div>
                      </div>
                    ))}
                  </div>
                ) : globalResults.length === 0 ? (
                  <div className="px-4 py-8 text-center flex flex-col items-center">
                    <span className="material-symbols-outlined text-[24px] text-outline mb-2">search_off</span>
                    <div className="text-xs font-semibold text-primary mb-1">No messages found</div>
                    <p className="text-[11px] text-on-surface-variant">
                      No messages matched "{searchQuery}".
                    </p>
                  </div>
                ) : (
                  <div className="divide-y divide-border-hairline/60">
                    <div className="px-3.5 py-2 bg-surface-container-low/50 text-[11px] font-mono text-outline flex items-center justify-between">
                      <span>Found {globalResults.length} {globalResults.length === 1 ? 'match' : 'matches'}</span>
                    </div>
                    {globalResults.map((msg) => {
                      const isMe = user && String(msg.senderId) === String(user.id);
                      const displayName = isMe ? 'You' : (msg.senderFullName || `@${msg.senderUsername}`);
                      const dateStr = formatPreviewTime(msg.createdAt);
                      return (
                        <div
                          key={msg.id}
                          onClick={() => navigate(`/dashboard/${msg.conversationId}`)}
                          className="p-3 hover:bg-surface-container-low cursor-pointer transition-colors"
                        >
                          <div className="flex items-center justify-between mb-1">
                            <span className="text-xs font-semibold text-primary truncate">
                              {displayName}
                            </span>
                            <span className="text-[10px] text-outline font-mono shrink-0">
                              {dateStr}
                            </span>
                          </div>
                          <p className={`text-xs ${msg.isDeleted ? 'italic text-outline' : 'text-on-surface-variant'} line-clamp-2`}>
                            {msg.isDeleted ? 'This message was deleted' : msg.body}
                          </p>
                        </div>
                      );
                    })}
                  </div>
                )}
              </div>
            )}

            {/* Loading Skeletons */}
            {activeTab !== 'messages' && loading && (
              <div className="p-3 space-y-2">
                {[1, 2, 3].map((n) => (
                  <div key={n} className="p-3 rounded-xl bg-surface-container-low animate-pulse flex items-center gap-3">
                    <div className="w-10 h-10 rounded-full bg-surface-container-high shrink-0"></div>
                    <div className="space-y-1.5 flex-1">
                      <div className="h-3.5 w-24 bg-surface-container-high rounded"></div>
                      <div className="h-3 w-36 bg-surface-container rounded"></div>
                    </div>
                  </div>
                ))}
              </div>
            )}

            {/* Conversation Empty State List */}
            {activeTab !== 'messages' && !loading && filteredConversations.length === 0 && (
              <div className="flex-1 overflow-y-auto px-4 py-6 flex flex-col items-center justify-center text-center">
                <div className="w-12 h-12 rounded-full bg-surface-container-high flex items-center justify-center text-on-surface-variant mb-3">
                  <span className="material-symbols-outlined text-[24px]">chat_bubble_outline</span>
                </div>
                <h2 className="text-sm font-semibold text-primary mb-1">
                  {searchQuery ? 'No matching conversations' : 'No conversations yet'}
                </h2>
                <p className="text-xs text-on-surface-variant max-w-[240px] leading-relaxed mb-4">
                  {searchQuery
                    ? 'Try searching for another member name or handle.'
                    : 'Start by finding someone to connect with. New chats will appear here.'}
                </p>
                <button
                  onClick={() => navigate('/find-people')}
                  className="px-3.5 py-1.5 text-xs font-semibold rounded-lg bg-surface-container text-primary hover:bg-surface-container-high transition-colors border border-border-hairline cursor-pointer"
                  type="button"
                >
                  Find People
                </button>
              </div>
            )}

            {/* Populated Conversation List */}
            {activeTab !== 'messages' && !loading && filteredConversations.length > 0 && (
              <div className="flex-1 overflow-y-auto divide-y divide-border-hairline/60">
                {filteredConversations.map((conv) => {
                  const peer = conv.participant;
                  const initials = (peer?.fullName || peer?.username || 'U')
                    .split(' ')
                    .map((n) => n[0])
                    .join('')
                    .substring(0, 2)
                    .toUpperCase();

                  const lastMsg = conv.lastMessage;
                  const snippet = lastMsg?.body || 'No messages yet';
                  const timestamp = formatPreviewTime(lastMsg?.createdAt || conv.updatedAt);
                  const hasUnread = conv.unreadCount > 0;
                  const isActive = String(conv.id) === String(conversationId);

                  const isPeerOnline =
                    peer?.lastActiveAt &&
                    (Date.now() - new Date(peer.lastActiveAt).getTime()) / 1000 < 120;

                  return (
                    <div
                      key={conv.id}
                      onClick={() => navigate(`/dashboard/${conv.id}`)}
                      className={`relative p-3.5 flex items-center justify-between transition-colors cursor-pointer group ${
                        isActive
                          ? 'bg-[#fff9f2]'
                          : hasUnread
                          ? 'bg-surface-container-low/60 hover:bg-surface-container-low'
                          : 'hover:bg-surface-container-low'
                      }`}
                    >
                      {/* Active indicator bar */}
                      {isActive && (
                        <span className="absolute left-0 top-2 bottom-2 w-1 bg-saffron rounded-r-full"></span>
                      )}

                      <div className="flex items-center gap-3 min-w-0 flex-1">
                        <div className="relative shrink-0">
                          <div className="w-10 h-10 rounded-full bg-primary-container text-white font-bold flex items-center justify-center text-xs shadow-xs select-none">
                            {initials}
                          </div>
                          {isPeerOnline && (
                            <span
                              className="absolute bottom-0 right-0 w-2.5 h-2.5 bg-india-green rounded-full ring-2 ring-surface-container-lowest"
                              title="Active now"
                            ></span>
                          )}
                        </div>

                        <div className="min-w-0 flex-1 pr-2">
                          <div className="flex items-center justify-between mb-0.5">
                            <span
                              className={`text-xs truncate ${
                                isActive || hasUnread ? 'text-primary font-bold' : 'text-on-surface'
                              }`}
                            >
                              {peer?.fullName || `@${peer?.username}`}
                            </span>
                            <span className="text-[10px] text-outline font-mono shrink-0 ml-1">
                              {timestamp}
                            </span>
                          </div>

                          <div className="flex items-center justify-between">
                            <p
                              className={`text-xs truncate ${
                                hasUnread
                                  ? 'text-primary font-semibold'
                                  : 'text-on-surface-variant'
                              }`}
                            >
                              {snippet}
                            </p>
                            {hasUnread && !isActive && (
                              <span className="px-1.5 py-0.2 rounded-full bg-saffron text-white text-[10px] font-bold shrink-0 ml-1">
                                {conv.unreadCount}
                              </span>
                            )}
                          </div>
                        </div>
                      </div>
                    </div>
                  );
                })}
              </div>
            )}

            {/* Directory Link Card */}
            <div className="p-3 m-3 bg-surface-container-low border border-border-hairline rounded-xl flex items-center justify-between">
              <div className="flex items-center gap-2.5">
                <div className="w-8 h-8 rounded-lg bg-surface-container-lowest flex items-center justify-center text-primary shadow-xs">
                  <span className="material-symbols-outlined text-[18px]">hub</span>
                </div>
                <div>
                  <div className="text-xs font-semibold text-on-surface">Member Directory</div>
                  <div className="text-[10px] text-outline font-mono">Connect with peers</div>
                </div>
              </div>
              <button
                onClick={() => navigate('/find-people')}
                className="w-7 h-7 rounded-lg hover:bg-surface-container flex items-center justify-center text-on-surface-variant transition-colors cursor-pointer"
                title="Open Directory"
                type="button"
              >
                <span className="material-symbols-outlined text-[16px]">arrow_forward</span>
              </button>
            </div>
          </aside>

          {/* Right Stage: In-place Active Chat or Empty State */}
          <section
            className={`${
              conversationId ? 'flex' : 'hidden md:flex'
            } flex-1 flex-col h-full bg-[#fafaf7] min-w-0 overflow-hidden relative`}
          >
            {conversationId ? (
              <ChatPanel
                conversationId={conversationId}
                onConversationActivity={handleConversationActivity}
                onBack={() => navigate('/dashboard')}
              />
            ) : (
              /* Approved Stitch Empty State Stage */
              <div className="flex-1 flex flex-col justify-between items-center relative overflow-hidden select-none p-6">
                {/* Ambient Decorative Motif */}
                <div className="absolute -top-32 right-12 w-96 h-96 rounded-full bg-gradient-to-br from-secondary/5 via-primary/5 to-transparent blur-3xl pointer-events-none"></div>
                <div className="absolute -bottom-24 -left-20 w-80 h-80 rounded-full bg-gradient-to-tr from-saffron/5 via-india-green/5 to-transparent blur-3xl pointer-events-none"></div>

                {/* Top Stage Header */}
                <header className="w-full flex items-center justify-between z-10">
                  <div className="flex items-center gap-2">
                    <span className="text-xs font-semibold uppercase tracking-wider text-outline font-mono">
                      Direct Communication Fabric
                    </span>
                    <span className="w-1 h-1 rounded-full bg-outline-variant"></span>
                    <span className="text-xs text-on-surface-variant">Authenticated Session</span>
                  </div>
                  <div className="flex items-center gap-2">
                    <TricolourBadge size="sm" />
                  </div>
                </header>

                {/* Centered Minimal Hero Panel */}
                <div className="w-full max-w-lg px-6 flex flex-col items-center text-center my-auto z-10">
                  <div className="relative mb-6 flex items-center justify-center">
                    <div className="w-16 h-16 p-3.5 rounded-2xl bg-surface-container-lowest border border-border-hairline shadow-sm flex items-center justify-center relative">
                      <svg className="w-10 h-10" fill="none" viewBox="0 0 40 40" xmlns="http://www.w3.org/2000/svg">
                        <circle cx="29" cy="11" fill="#F58220" r="3.5" />
                        <circle cx="11" cy="29" fill="#138808" r="2.5" />
                        <path d="M12 28L28 12" stroke="#0A1128" strokeDasharray="2 2" strokeLinecap="round" strokeWidth="2" />
                        <rect fill="#0A1128" height="14" rx="4" width="16" x="8" y="10" />
                        <path d="M14 24L11 27V24H14Z" fill="#0A1128" />
                        <rect fill="#2D4FCF" height="12" rx="3.5" width="14" x="18" y="18" />
                        <circle cx="13.5" cy="17" fill="#FFFFFF" r="1.25" />
                        <circle cx="18.5" cy="17" fill="#FFFFFF" r="1.25" />
                        <path d="M23 24H27" stroke="#FFFFFF" strokeLinecap="round" strokeWidth="1.5" />
                      </svg>
                    </div>
                  </div>

                  <h2 className="text-2xl font-bold text-primary tracking-tight mb-2">
                    Your conversations start here.
                  </h2>

                  <p className="text-sm text-on-surface-variant max-w-sm mb-6 leading-relaxed">
                    Choose someone from your chats or find a person on the Sampark network to send your first message.
                  </p>

                  <div className="flex flex-col sm:flex-row items-center gap-3 w-full sm:w-auto">
                    <button
                      type="button"
                      onClick={() => navigate('/find-people')}
                      className="w-full sm:w-auto px-5 py-2.5 rounded-xl bg-primary text-white hover:bg-primary-hover active:scale-[0.98] transition-all font-semibold text-sm shadow-xs flex items-center justify-center gap-2 group cursor-pointer"
                    >
                      <span className="material-symbols-outlined text-[19px] group-hover:scale-110 transition-transform">
                        person_search
                      </span>
                      <span>Find People</span>
                    </button>
                  </div>

                  <div className="mt-8 flex items-center gap-2 text-outline text-xs">
                    <span>Welcome to Sampark,</span>
                    <span className="font-semibold text-primary">{user?.fullName || user?.username}</span>
                  </div>
                </div>

                {/* Bottom Status Footer */}
                <footer className="w-full flex items-center justify-center z-10">
                  <div className="flex items-center gap-2 px-3 py-1.5 rounded-full bg-surface-container-low border border-border-hairline text-xs font-medium text-on-surface-variant">
                    <span className="w-2 h-2 rounded-full bg-india-green"></span>
                    <span>Connected to Sampark Server • Active</span>
                  </div>
                </footer>
              </div>
            )}
          </section>
        </div>
      </div>

      {/* Self Profile Modal (Edit Mode) */}
      <ProfileModal
        isOpen={selfProfileOpen}
        onClose={() => setSelfProfileOpen(false)}
        mode="edit"
      />
    </div>
  );
}

export default DashboardPage;
