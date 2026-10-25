import React, { useState, useEffect, useRef, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { api } from '../api/client';
import { TricolourBadge } from './TricolourBadge';
import { AlertBanner } from './AlertBanner';

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
    if (!loading && messages.length > 0) {
      scrollToBottom(false);
    }
  }, [loading, conversationId]);

  // Authenticated Incremental Polling
  useEffect(() => {
    if (!conversationId) return;

    const poll = async () => {
      const targetId = activeConvIdRef.current;
      if (!targetId || targetId !== conversationId) return;

      try {
        const afterId = latestMessageIdRef.current;
        const data = await api.getMessages(targetId, { after: afterId, limit: 50 });

        if (activeConvIdRef.current !== targetId) return;

        if (data?.messages && data.messages.length > 0) {
          let lastNewMsg = null;
          setMessages((prev) => {
            const existingIds = new Set(prev.map((m) => m.id));
            const newOnes = data.messages.filter((m) => !existingIds.has(m.id));
            if (newOnes.length === 0) return prev;

            // Update latestMessageIdRef
            for (const m of newOnes) {
              if (m.id > latestMessageIdRef.current) {
                latestMessageIdRef.current = m.id;
              }
            }
            lastNewMsg = newOnes[newOnes.length - 1];
            return [...prev, ...newOnes];
          });

          // Mark incoming as read since user is actively viewing
          api.markConversationAsRead(targetId).catch(() => {});

          // Inform parent dashboard list
          if (lastNewMsg) {
            onConversationActivity?.({
              conversationId: targetId,
              lastMessage: lastNewMsg,
              isSentByMe: lastNewMsg.senderId === user?.id,
            });
            scrollToBottom(true);
          }
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
  }, [conversationId, user?.id, onConversationActivity]);

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

  const otherUser = conversation?.participant;
  const initials = (otherUser?.fullName || otherUser?.username || 'U')
    .split(' ')
    .map((n) => n[0])
    .join('')
    .substring(0, 2)
    .toUpperCase();

  return (
    <div className="flex-1 flex flex-col h-full bg-[#fafaf7] min-w-0 overflow-hidden">
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

          {/* Participant Info */}
          <div className="relative shrink-0">
            <div className="w-10 h-10 rounded-full bg-primary-container text-white flex items-center justify-center text-xs font-bold shadow-xs select-none">
              {initials}
            </div>
          </div>

          <div className="flex flex-col min-w-0">
            <div className="flex items-center gap-2">
              <span className="text-sm font-bold text-primary truncate">
                {otherUser?.fullName || 'Conversation'}
              </span>
              <TricolourBadge size="sm" />
            </div>
            <div className="flex items-center gap-2 text-xs font-mono text-outline leading-tight">
              <span>@{otherUser?.username || 'member'}</span>
              <span className="text-outline/40">•</span>
              <span className="text-on-surface-variant/80 font-sans text-[11px]">
                Secure Authenticated Session
              </span>
            </div>
          </div>
        </div>

        {/* Header Utility Actions */}
        <div className="flex items-center gap-2">
          <button
            onClick={() => navigate('/find-people')}
            className="hidden sm:flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-surface-container-low hover:bg-surface-container text-xs font-semibold text-primary border border-border-hairline transition-colors cursor-pointer shadow-xs"
            title="Find other members"
            type="button"
          >
            <span className="material-symbols-outlined text-[16px]">person_search</span>
            <span>Find People</span>
          </button>
        </div>
      </header>

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
        {hasMoreOlder && (
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
        {!loading && messages.length > 0 && (
          <>
            <div className="flex items-center justify-center my-3">
              <span className="px-3 py-1 rounded-full bg-surface-container text-[11px] font-medium text-on-surface-variant shadow-xs">
                {formatDateHeader(messages[0]?.createdAt)}
              </span>
            </div>

            {messages.map((msg) => {
              const isSelf = msg.senderId === user?.id;

              return (
                <div
                  key={msg.id}
                  className={`flex items-end gap-2.5 max-w-[85%] sm:max-w-xl ${
                    isSelf ? 'ml-auto justify-end' : 'justify-start'
                  }`}
                >
                  {/* Peer Avatar on left */}
                  {!isSelf && (
                    <div
                      className="w-7 h-7 rounded-full bg-primary-container text-white flex items-center justify-center text-[10px] font-semibold shrink-0 shadow-xs select-none"
                      title={otherUser?.fullName}
                    >
                      {initials}
                    </div>
                  )}

                  {/* Message Bubble */}
                  <div className="flex flex-col gap-1 max-w-full">
                    <div
                      className={`px-4 py-2.5 rounded-2xl text-sm leading-relaxed shadow-xs transition-all ${
                        isSelf
                          ? 'bg-primary text-white rounded-br-xs'
                          : 'bg-[#f4f5f2] text-[#202124] rounded-bl-xs border border-border-hairline/60'
                      }`}
                    >
                      <p className="whitespace-pre-wrap break-words">{msg.body}</p>
                    </div>

                    <div
                      className={`text-[10px] flex items-center gap-1 font-mono px-1 ${
                        isSelf ? 'justify-end text-outline' : 'justify-start text-outline'
                      }`}
                    >
                      <span>{formatTime(msg.createdAt)}</span>
                      {isSelf && (
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
    </div>
  );
}

export default ChatPanel;
