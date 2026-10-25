import React, { useState, useEffect, useRef, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { api } from '../api/client';
import { TricolourBadge } from './TricolourBadge';
import { AlertBanner } from './AlertBanner';
import { ProfileModal } from './ProfileModal';

export function ChatPanel({ conversationId, onConversationActivity, onBack }) {
  const { user } = useAuth();
  const navigate = useNavigate();

  const [conversation, setConversation] = useState(null);
  const [messages, setMessages] = useState([]);
  const [loading, setLoading] = useState(true);
  const [loadingOlder, setLoadingOlder] = useState(false);
  const [hasMoreOlder, setHasMoreOlder] = useState(false);
  const [error, setError] = useState(null);
  const [inputText, setInputText] = useState('');
  const [sending, setSending] = useState(false);
  const [sendError, setSendError] = useState(null);

  // Phase 3: In-Chat Search State
  const [searchOpen, setSearchOpen] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState([]);
  const [searching, setSearching] = useState(false);

  // Phase 3: Peer Profile Modal State
  const [profileModalOpen, setProfileModalOpen] = useState(false);

  // Phase 3: Inline Message Editing & Deletion State
  const [editingMsgId, setEditingMsgId] = useState(null);
  const [editBody, setEditBody] = useState('');
  const [actionError, setActionError] = useState(null);

  const messagesEndRef = useRef(null);
  const messagesContainerRef = useRef(null);
  const pollingTimerRef = useRef(null);
  const activeConvIdRef = useRef(conversationId);
  const latestMessageIdRef = useRef(0);
  const oldestMessageIdRef = useRef(0);

  // Update active conversation ref immediately when prop changes
  activeConvIdRef.current = conversationId;

  const scrollToBottom = (smooth = true) => {
    if (messagesEndRef.current) {
      messagesEndRef.current.scrollIntoView({ behavior: smooth ? 'smooth' : 'auto' });
    }
  };

  // Helper formatting functions
  const formatTime = (isoString) => {
    if (!isoString) return '';
    try {
      const d = new Date(isoString);
      return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    } catch {
      return '';
    }
  };

  const formatDateHeader = (isoString) => {
    if (!isoString) return '';
    try {
      const d = new Date(isoString);
      const today = new Date();
      if (d.toDateString() === today.toDateString()) {
        return 'Today';
      }
      return d.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' });
    } catch {
      return '';
    }
  };

  // Phase 3: Calculate honest presence status
  const getPresence = (lastActiveAt) => {
    if (!lastActiveAt) return { isOnline: false, label: 'Offline' };
    try {
      const activeTime = new Date(lastActiveAt).getTime();
      const now = Date.now();
      const diffSecs = Math.floor((now - activeTime) / 1000);

      if (diffSecs < 120) {
        return { isOnline: true, label: 'Active now' };
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
        label: `Active ${new Date(lastActiveAt).toLocaleDateString(undefined, { month: 'short', day: 'numeric' })}`,
      };
    } catch {
      return { isOnline: false, label: 'Offline' };
    }
  };

  // Initial Conversation Load
  useEffect(() => {
    if (!conversationId) return;

    let ignore = false;
    const targetId = conversationId;
    activeConvIdRef.current = targetId;

    async function loadConversation() {
      try {
        setLoading(true);
        setError(null);
        setSendError(null);
        setActionError(null);
        setEditingMsgId(null);
        setSearchOpen(false);
        setSearchQuery('');
        setSearchResults([]);
        latestMessageIdRef.current = 0;
        oldestMessageIdRef.current = 0;

        const data = await api.getMessages(targetId, { limit: 50 });
        if (ignore || activeConvIdRef.current !== targetId) return;

        const fetchedMessages = data?.messages || [];
        setConversation(data?.conversation || null);
        setMessages(fetchedMessages);

        if (fetchedMessages.length > 0) {
          const maxId = fetchedMessages.reduce((max, m) => Math.max(max, m.id), 0);
          const minId = fetchedMessages.reduce((min, m) => Math.min(min, m.id), fetchedMessages[0].id);
          latestMessageIdRef.current = maxId;
          oldestMessageIdRef.current = minId;
          setHasMoreOlder(fetchedMessages.length >= 50);
        } else {
          setHasMoreOlder(false);
        }

        // Mark as read immediately on open
        api.markConversationAsRead(targetId).catch(() => {});
      } catch (err) {
        if (!ignore && activeConvIdRef.current === targetId) {
          setError(err.message || 'Failed to load conversation history.');
        }
      } finally {
        if (!ignore && activeConvIdRef.current === targetId) {
          setLoading(false);
        }
      }
    }

    loadConversation();

    return () => {
      ignore = true;
    };
  }, [conversationId]);

  // Scroll to bottom when conversation finishes initial loading
  useEffect(() => {
    if (!loading && messages.length > 0 && !searchOpen) {
      scrollToBottom(false);
    }
  }, [loading, conversationId, searchOpen]);

  // Authenticated Incremental Polling (every 2.5 seconds)
  useEffect(() => {
    if (!conversationId) return;

    let pollCounter = 0;

    const poll = async () => {
      const targetId = activeConvIdRef.current;
      if (!targetId || targetId !== conversationId) return;

      try {
        pollCounter++;
        const afterId = latestMessageIdRef.current;
        const data = await api.getMessages(targetId, { after: afterId, limit: 50 });

        if (activeConvIdRef.current !== targetId) return;

        if (data?.messages && data.messages.length > 0) {
          const newOnes = data.messages;
          const latest = newOnes[newOnes.length - 1];

          for (const m of newOnes) {
            if (m.id > latestMessageIdRef.current) {
              latestMessageIdRef.current = m.id;
            }
          }

          setMessages((prev) => {
            const existingIds = new Set(prev.map((m) => m.id));
            const toAdd = newOnes.filter((m) => !existingIds.has(m.id));
            if (toAdd.length === 0) return prev;
            return [...prev, ...toAdd];
          });

          // Mark incoming as read since user is actively viewing
          api.markConversationAsRead(targetId).catch(() => {});

          // Inform parent dashboard list
          onConversationActivity?.({
            conversationId: targetId,
            lastMessage: latest,
            isSentByMe: latest.senderId === user?.id,
          });

          if (!searchOpen) {
            scrollToBottom(true);
          }
        }

        // Every 3rd poll cycle (~7.5s), refresh participant metadata (presence & read receipts)
        if (pollCounter % 3 === 0) {
          const freshConv = await api.getConversation(targetId);
          if (freshConv?.conversation && activeConvIdRef.current === targetId) {
            setConversation(freshConv.conversation);
          }

          // If there are unread outgoing messages, refresh read status
          setMessages((prev) => {
            const hasUnreadOutgoing = prev.some((m) => m.senderId === user?.id && !m.readAt);
            if (hasUnreadOutgoing) {
              api.getMessages(targetId, { limit: 50 }).then((fullData) => {
                if (fullData?.messages && activeConvIdRef.current === targetId) {
                  const readMap = new Map(fullData.messages.map((m) => [m.id, m]));
                  setMessages((current) =>
                    current.map((msg) => {
                      const updated = readMap.get(msg.id);
                      if (updated && (updated.readAt !== msg.readAt || updated.isDeleted !== msg.isDeleted || updated.body !== msg.body)) {
                        return { ...msg, ...updated };
                      }
                      return msg;
                    })
                  );
                }
              }).catch(() => {});
            }
            return prev;
          });
        }
      } catch {
        // Silent recovery on network glitch
      }
    };

    pollingTimerRef.current = setInterval(poll, 2500);

    return () => {
      if (pollingTimerRef.current) {
        clearInterval(pollingTimerRef.current);
      }
    };
  }, [conversationId, user?.id, onConversationActivity, searchOpen]);

  // Load older messages (pagination)
  const handleLoadOlder = async () => {
    if (loadingOlder || !hasMoreOlder || !conversationId) return;
    const targetId = conversationId;
    const beforeId = oldestMessageIdRef.current;
    if (beforeId <= 1) {
      setHasMoreOlder(false);
      return;
    }

    try {
      setLoadingOlder(true);
      const prevScrollHeight = messagesContainerRef.current?.scrollHeight || 0;

      const data = await api.getMessages(targetId, { before: beforeId, limit: 30 });
      if (activeConvIdRef.current !== targetId) return;

      const olderMessages = data?.messages || [];
      if (olderMessages.length === 0) {
        setHasMoreOlder(false);
      } else {
        setMessages((prev) => {
          const existingIds = new Set(prev.map((m) => m.id));
          const newOlder = olderMessages.filter((m) => !existingIds.has(m.id));
          if (newOlder.length === 0) return prev;

          // Update oldestMessageIdRef
          for (const m of newOlder) {
            if (oldestMessageIdRef.current === 0 || m.id < oldestMessageIdRef.current) {
              oldestMessageIdRef.current = m.id;
            }
          }
          return [...newOlder, ...prev];
        });

        if (olderMessages.length < 30) {
          setHasMoreOlder(false);
        }

        // Restore scroll position
        requestAnimationFrame(() => {
          if (messagesContainerRef.current) {
            const newScrollHeight = messagesContainerRef.current.scrollHeight;
            messagesContainerRef.current.scrollTop = newScrollHeight - prevScrollHeight;
          }
        });
      }
    } catch {
      // Ignore pagination errors
    } finally {
      setLoadingOlder(false);
    }
  };

  // Message Send Handler
  const handleSend = async (e) => {
    e?.preventDefault?.();
    const trimmed = inputText.trim();
    if (!trimmed || sending || !conversationId) return;

    const targetId = conversationId;

    try {
      setSending(true);
      setSendError(null);

      const res = await api.sendMessage(targetId, trimmed);
      if (activeConvIdRef.current !== targetId) return;

      if (res?.message) {
        setMessages((prev) => [...prev, res.message]);
        if (res.message.id > latestMessageIdRef.current) {
          latestMessageIdRef.current = res.message.id;
        }
        setInputText(''); // Clear only on confirmed success

        onConversationActivity?.({
          conversationId: targetId,
          lastMessage: res.message,
          isSentByMe: true,
        });

        scrollToBottom(true);
      } else {
        throw new Error('Server did not confirm message delivery.');
      }
    } catch (err) {
      // Preserve user input on failure
      setSendError(err.message || 'Failed to send message. Please try again.');
    } finally {
      setSending(false);
    }
  };

  // Phase 3: Message Editing Handler
  const handleStartEdit = (msg) => {
    if (msg.isDeleted || msg.senderId !== user?.id) return;
    setEditingMsgId(msg.id);
    setEditBody(msg.body);
    setActionError(null);
  };

  const handleCancelEdit = () => {
    setEditingMsgId(null);
    setEditBody('');
    setActionError(null);
  };

  const handleSaveEdit = async (messageId) => {
    const trimmed = editBody.trim();
    if (!trimmed) {
      setActionError('Message cannot be empty.');
      return;
    }

    try {
      setActionError(null);
      const res = await api.editMessage(messageId, trimmed);
      if (res?.message) {
        setMessages((prev) =>
          prev.map((m) => (m.id === messageId ? { ...m, ...res.message } : m))
        );
        setEditingMsgId(null);
        setEditBody('');
      } else {
        throw new Error('Failed to update message.');
      }
    } catch (err) {
      setActionError(err.message || 'Could not update message.');
    }
  };

  // Phase 3: Message Deletion Handler
  const handleDeleteMessage = async (messageId) => {
    if (!window.confirm('Are you sure you want to delete this message?')) return;

    try {
      setActionError(null);
      const res = await api.deleteMessage(messageId);
      if (res?.deletedMessage) {
        setMessages((prev) =>
          prev.map((m) => (m.id === messageId ? { ...m, ...res.deletedMessage } : m))
        );
      } else {
        throw new Error('Failed to delete message.');
      }
    } catch (err) {
      setActionError(err.message || 'Could not delete message.');
    }
  };

  // Phase 3: Search Messages in Conversation
  const handleSearchSubmit = async (e) => {
    e?.preventDefault?.();
    const query = searchQuery.trim();
    if (!query || !conversationId) return;

    try {
      setSearching(true);
      const res = await api.searchConversationMessages(conversationId, query);
      setSearchResults(res?.messages || []);
    } catch {
      setSearchResults([]);
    } finally {
      setSearching(false);
    }
  };

  const handleClearSearch = () => {
    setSearchQuery('');
    setSearchResults([]);
    setSearchOpen(false);
  };

  const otherUser = conversation?.participant;
  const presence = getPresence(otherUser?.lastActiveAt);

  const initials = (otherUser?.fullName || otherUser?.username || 'U')
    .split(' ')
    .map((n) => n[0])
    .join('')
    .substring(0, 2)
    .toUpperCase();

  // Determine messages to display (filtered if search active with query, or full stream)
  const displayMessages = searchOpen && searchQuery.trim() ? searchResults : messages;

  return (
    <div className="flex-1 flex flex-col h-full bg-[#fafaf7] min-w-0 overflow-hidden relative">
      {/* Chat Top Header */}
      <header className="h-16 px-4 sm:px-6 bg-surface-container-lowest flex items-center justify-between border-b border-border-hairline z-10 shrink-0 shadow-xs">
        <div className="flex items-center gap-3 min-w-0">
          {/* Back button for mobile view or quick exit */}
          <button
            onClick={onBack || (() => navigate('/dashboard'))}
            className="md:hidden w-8 h-8 rounded-lg hover:bg-surface-container flex items-center justify-center text-on-surface-variant transition-colors cursor-pointer shrink-0"
            title="Back to conversation list"
            type="button"
          >
            <span className="material-symbols-outlined text-[22px]">arrow_back</span>
          </button>

          {/* Participant Info - Click to open Profile */}
          <div
            className="flex items-center gap-3 cursor-pointer group min-w-0"
            onClick={() => setProfileModalOpen(true)}
            title="View member profile"
          >
            <div className="relative shrink-0">
              <div className="w-10 h-10 rounded-full bg-primary-container text-white flex items-center justify-center text-xs font-bold shadow-xs select-none group-hover:ring-2 group-hover:ring-secondary/40 transition-all">
                {initials}
              </div>
              {presence.isOnline && (
                <span
                  className="absolute bottom-0 right-0 w-2.5 h-2.5 bg-india-green rounded-full ring-2 ring-surface-container-lowest"
                  title="Active now"
                ></span>
              )}
            </div>

            <div className="flex flex-col min-w-0">
              <div className="flex items-center gap-2">
                <span className="text-sm font-bold text-primary truncate group-hover:text-secondary transition-colors">
                  {otherUser?.fullName || 'Conversation'}
                </span>
                <TricolourBadge size="sm" />
              </div>
              <div className="flex items-center gap-2 text-xs font-mono text-outline leading-tight">
                <span>@{otherUser?.username || 'member'}</span>
                <span className="text-outline/40">•</span>
                <span
                  className={`font-sans text-[11px] ${
                    presence.isOnline ? 'text-india-green font-semibold' : 'text-on-surface-variant/80'
                  }`}
                >
                  {presence.label}
                </span>
              </div>
            </div>
          </div>
        </div>

        {/* Header Utility Actions */}
        <div className="flex items-center gap-1.5 text-on-surface-variant">
          {/* Search inside chat button */}
          <button
            type="button"
            onClick={() => setSearchOpen((prev) => !prev)}
            className={`w-9 h-9 rounded-xl flex items-center justify-center transition-colors cursor-pointer ${
              searchOpen
                ? 'bg-primary text-white shadow-xs'
                : 'hover:bg-surface-container hover:text-on-surface'
            }`}
            title="Search inside conversation"
            aria-label="Search inside chat"
          >
            <span className="material-symbols-outlined text-[20px]">search</span>
          </button>

          {/* View contact info button */}
          <button
            type="button"
            onClick={() => setProfileModalOpen(true)}
            className="w-9 h-9 rounded-xl flex items-center justify-center hover:bg-surface-container hover:text-on-surface transition-colors cursor-pointer"
            title="View member info"
            aria-label="View contact info"
          >
            <span className="material-symbols-outlined text-[20px]">info</span>
          </button>

          <button
            onClick={() => navigate('/find-people')}
            className="hidden sm:flex items-center gap-1.5 px-3 py-1.5 ml-1 rounded-lg bg-surface-container-low hover:bg-surface-container text-xs font-semibold text-primary border border-border-hairline transition-colors cursor-pointer shadow-xs"
            title="Find other members"
            type="button"
          >
            <span className="material-symbols-outlined text-[16px]">group_add</span>
            <span>Directory</span>
          </button>
        </div>
      </header>

      {/* Phase 3: In-Chat Search Bar Overlay */}
      {searchOpen && (
        <div className="bg-surface-container-lowest border-b border-border-hairline px-4 sm:px-6 py-2.5 flex items-center justify-between gap-3 shadow-xs animate-fade-in z-20">
          <form onSubmit={handleSearchSubmit} className="flex-1 flex items-center gap-2">
            <span className="material-symbols-outlined text-outline text-[18px]">search</span>
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => {
                setSearchQuery(e.target.value);
                if (!e.target.value.trim()) {
                  setSearchResults([]);
                }
              }}
              placeholder="Search in this conversation..."
              className="flex-1 text-xs bg-transparent border-0 outline-none text-on-surface placeholder:text-outline"
              autoFocus
            />
            {searching && (
              <span className="w-3.5 h-3.5 border-2 border-primary border-t-transparent rounded-full animate-spin"></span>
            )}
            {searchQuery && (
              <button
                type="submit"
                className="px-2.5 py-1 text-xs font-semibold rounded-md bg-primary text-white cursor-pointer shadow-xs"
              >
                Search
              </button>
            )}
          </form>

          <div className="flex items-center gap-2">
            {searchQuery.trim() && (
              <span className="text-[11px] font-mono text-outline">
                {searchResults.length} {searchResults.length === 1 ? 'match' : 'matches'}
              </span>
            )}
            <button
              type="button"
              onClick={handleClearSearch}
              className="w-7 h-7 rounded-lg hover:bg-surface-container flex items-center justify-center text-outline hover:text-on-surface transition-colors cursor-pointer"
              title="Close search"
            >
              <span className="material-symbols-outlined text-[16px]">close</span>
            </button>
          </div>
        </div>
      )}

      {/* Action / Error Banner */}
      {actionError && (
        <div className="p-3 bg-red-50 border-b border-red-200">
          <AlertBanner type="error" message={actionError} onClose={() => setActionError(null)} />
        </div>
      )}

      {/* Message Stream Area */}
      <section
        ref={messagesContainerRef}
        className="flex-1 overflow-y-auto p-4 sm:p-6 space-y-4"
      >
        {/* Error Banner */}
        {error && (
          <div className="mb-4">
            <AlertBanner type="error" message={error} onClose={() => setError(null)} />
          </div>
        )}

        {/* Older messages pagination button */}
        {hasMoreOlder && !searchOpen && (
          <div className="flex justify-center pb-2">
            <button
              onClick={handleLoadOlder}
              disabled={loadingOlder}
              className="px-3.5 py-1 text-xs font-medium text-secondary bg-surface-container-lowest hover:bg-surface-container border border-border-hairline rounded-full shadow-xs transition-colors flex items-center gap-1.5 cursor-pointer disabled:opacity-50"
              type="button"
            >
              {loadingOlder ? (
                <>
                  <span className="w-3.5 h-3.5 border-2 border-secondary border-t-transparent rounded-full animate-spin"></span>
                  <span>Loading earlier messages...</span>
                </>
              ) : (
                <>
                  <span className="material-symbols-outlined text-[15px]">history</span>
                  <span>Load earlier messages</span>
                </>
              )}
            </button>
          </div>
        )}

        {/* Loading Skeletons */}
        {loading && (
          <div className="space-y-4 pt-4">
            <div className="flex items-end gap-2.5 max-w-[70%]">
              <div className="w-8 h-8 rounded-full bg-surface-container-high animate-pulse"></div>
              <div className="h-12 w-52 bg-surface-container rounded-2xl animate-pulse"></div>
            </div>
            <div className="flex items-end justify-end gap-2.5 max-w-[70%] ml-auto">
              <div className="h-10 w-44 bg-primary/20 rounded-2xl animate-pulse"></div>
            </div>
            <div className="flex items-end gap-2.5 max-w-[70%]">
              <div className="w-8 h-8 rounded-full bg-surface-container-high animate-pulse"></div>
              <div className="h-14 w-60 bg-surface-container rounded-2xl animate-pulse"></div>
            </div>
          </div>
        )}

        {/* Search Results Empty State */}
        {!loading && searchOpen && searchQuery.trim() && searchResults.length === 0 && (
          <div className="h-full flex flex-col items-center justify-center text-center p-8 my-auto select-none">
            <div className="w-12 h-12 rounded-xl bg-surface-container-lowest border border-border-hairline flex items-center justify-center text-outline mb-2">
              <span className="material-symbols-outlined text-[24px]">search_off</span>
            </div>
            <h4 className="text-sm font-semibold text-primary mb-1">No matching messages found</h4>
            <p className="text-xs text-on-surface-variant max-w-xs">
              No messages in this chat contain "{searchQuery}".
            </p>
          </div>
        )}

        {/* Empty State: No messages yet */}
        {!loading && !error && messages.length === 0 && (
          <div className="h-full flex flex-col items-center justify-center text-center p-8 my-auto select-none">
            <div className="w-14 h-14 rounded-2xl bg-surface-container-lowest border border-border-hairline flex items-center justify-center text-primary mb-3 shadow-xs">
              <span className="material-symbols-outlined text-[30px] text-secondary">forum</span>
            </div>
            <h3 className="text-base font-bold text-primary mb-1">
              Start your conversation with {otherUser?.fullName || 'this member'}
            </h3>
            <p className="text-xs text-on-surface-variant max-w-sm leading-relaxed mb-4">
              Send a message below to connect. Messages are persisted securely in your private chat history.
            </p>
            <div className="inline-flex items-center gap-1.5 px-3 py-1 rounded-full bg-surface-container-lowest border border-border-hairline text-[11px] font-mono text-outline shadow-xs">
              <span className="w-2 h-2 rounded-full bg-india-green"></span>
              <span>Secure Authenticated Session</span>
            </div>
          </div>
        )}

        {/* Message Stream */}
        {!loading && displayMessages.length > 0 && (
          <>
            <div className="flex items-center justify-center my-3">
              <span className="px-3 py-1 rounded-full bg-surface-container text-[11px] font-medium text-on-surface-variant shadow-xs">
                {formatDateHeader(displayMessages[0]?.createdAt)}
              </span>
            </div>

            {displayMessages.map((msg) => {
              const isSelf = msg.senderId === user?.id;
              const isEditing = editingMsgId === msg.id;

              return (
                <div
                  key={msg.id}
                  className={`group relative flex items-end gap-2.5 max-w-[85%] sm:max-w-xl ${
                    isSelf ? 'ml-auto justify-end' : 'justify-start'
                  }`}
                >
                  {/* Peer Avatar on left */}
                  {!isSelf && (
                    <div
                      className="w-7 h-7 rounded-full bg-primary-container text-white flex items-center justify-center text-[10px] font-semibold shrink-0 shadow-xs select-none cursor-pointer"
                      title={otherUser?.fullName}
                      onClick={() => setProfileModalOpen(true)}
                    >
                      {initials}
                    </div>
                  )}

                  {/* Actions Bar for outgoing messages on hover */}
                  {isSelf && !msg.isDeleted && !isEditing && (
                    <div className="opacity-0 group-hover:opacity-100 transition-opacity flex items-center gap-0.5 bg-surface-container-lowest border border-border-hairline rounded-lg shadow-xs px-1 py-0.5 mr-1 self-center">
                      <button
                        type="button"
                        onClick={() => handleStartEdit(msg)}
                        className="w-6 h-6 rounded hover:bg-surface-container flex items-center justify-center text-outline hover:text-on-surface transition-colors cursor-pointer"
                        title="Edit message"
                      >
                        <span className="material-symbols-outlined text-[14px]">edit</span>
                      </button>
                      <button
                        type="button"
                        onClick={() => handleDeleteMessage(msg.id)}
                        className="w-6 h-6 rounded hover:bg-red-50 flex items-center justify-center text-outline hover:text-error transition-colors cursor-pointer"
                        title="Delete message"
                      >
                        <span className="material-symbols-outlined text-[14px]">delete</span>
                      </button>
                    </div>
                  )}

                  {/* Message Bubble */}
                  <div className="flex flex-col gap-1 max-w-full">
                    {isEditing ? (
                      /* Inline Message Editor */
                      <div className="p-3 bg-surface-container-lowest border-2 border-secondary rounded-2xl shadow-sm min-w-[280px]">
                        <textarea
                          value={editBody}
                          onChange={(e) => setEditBody(e.target.value)}
                          onKeyDown={(e) => {
                            if (e.key === 'Enter' && !e.shiftKey) {
                              e.preventDefault();
                              handleSaveEdit(msg.id);
                            }
                            if (e.key === 'Escape') {
                              handleCancelEdit();
                            }
                          }}
                          rows={2}
                          className="w-full text-xs text-on-surface bg-surface-container-low rounded-lg p-2 outline-none resize-none"
                          autoFocus
                        />
                        <div className="flex items-center justify-end gap-1.5 mt-2">
                          <button
                            type="button"
                            onClick={handleCancelEdit}
                            className="px-2.5 py-1 text-[11px] font-semibold text-on-surface-variant hover:bg-surface-container rounded-md cursor-pointer"
                          >
                            Cancel
                          </button>
                          <button
                            type="button"
                            onClick={() => handleSaveEdit(msg.id)}
                            className="px-2.5 py-1 text-[11px] font-semibold bg-primary hover:bg-primary-hover text-white rounded-md cursor-pointer shadow-xs"
                          >
                            Save
                          </button>
                        </div>
                      </div>
                    ) : (
                      /* Standard Message Bubble */
                      <div
                        className={`px-4 py-2.5 rounded-2xl text-sm leading-relaxed shadow-xs transition-all ${
                          msg.isDeleted
                            ? 'bg-surface-container-low text-outline italic rounded-2xl border border-dashed border-border-hairline'
                            : isSelf
                            ? 'bg-primary text-white rounded-br-xs'
                            : 'bg-[#f4f5f2] text-[#202124] rounded-bl-xs border border-border-hairline/60'
                        }`}
                      >
                        <p className="whitespace-pre-wrap break-words">{msg.body}</p>
                      </div>
                    )}

                    <div
                      className={`text-[10px] flex items-center gap-1.5 font-mono px-1 ${
                        isSelf ? 'justify-end text-outline' : 'justify-start text-outline'
                      }`}
                    >
                      <span>{formatTime(msg.createdAt)}</span>
                      {msg.editedAt && !msg.isDeleted && (
                        <span className="italic text-[10px] opacity-75">(edited)</span>
                      )}
                      {isSelf && !msg.isDeleted && (
                        <span
                          className={`material-symbols-outlined text-[14px] leading-none ${
                            msg.readAt ? 'text-secondary font-bold' : 'text-outline'
                          }`}
                          title={msg.readAt ? 'Read' : 'Delivered'}
                        >
                          {msg.readAt ? 'done_all' : 'done'}
                        </span>
                      )}
                    </div>
                  </div>
                </div>
              );
            })}
          </>
        )}

        {/* Auto-scroll anchor */}
        <div ref={messagesEndRef} />
      </section>

      {/* Message Composer Footer */}
      <footer className="p-3 sm:p-4 bg-surface-container-lowest border-t border-border-hairline shrink-0 shadow-xs">
        {sendError && (
          <div className="mb-3 max-w-3xl mx-auto">
            <AlertBanner type="error" message={sendError} onClose={() => setSendError(null)} />
          </div>
        )}

        <form onSubmit={handleSend} className="max-w-3xl mx-auto flex flex-col gap-1.5">
          <div className="flex items-center gap-2 bg-[#fafaf7] rounded-xl px-3 py-1.5 border border-border-hairline focus-within:border-secondary transition-all shadow-xs">
            <input
              type="text"
              value={inputText}
              onChange={(e) => setInputText(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && !e.shiftKey) {
                  e.preventDefault();
                  handleSend();
                }
              }}
              disabled={sending || loading}
              placeholder={
                otherUser
                  ? `Message @${otherUser.username}...`
                  : 'Type a message...'
              }
              className="flex-1 bg-transparent border-0 outline-none py-1.5 text-sm text-on-surface placeholder:text-outline disabled:opacity-50"
            />

            <button
              type="submit"
              disabled={!inputText.trim() || sending || loading}
              className="w-9 h-9 rounded-full bg-primary hover:bg-primary-hover active:scale-90 text-white flex items-center justify-center shadow-xs transition-transform cursor-pointer disabled:opacity-40 disabled:cursor-not-allowed disabled:transform-none shrink-0"
              title="Send message (Enter)"
            >
              {sending ? (
                <span className="w-4 h-4 border-2 border-white border-t-transparent rounded-full animate-spin"></span>
              ) : (
                <span className="material-symbols-outlined text-[18px]">send</span>
              )}
            </button>
          </div>

          <div className="flex items-center justify-between px-1 text-[11px] text-outline font-mono">
            <span>Press Enter to send</span>
            <span>Secure session</span>
          </div>
        </form>
      </footer>

      {/* Phase 3: Peer Profile Modal */}
      {profileModalOpen && (
        <ProfileModal
          isOpen={profileModalOpen}
          onClose={() => setProfileModalOpen(false)}
          mode="view"
          targetUser={otherUser}
        />
      )}
    </div>
  );
}

export default ChatPanel;
