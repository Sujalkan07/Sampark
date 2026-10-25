import React, { useState, useEffect, useRef, useCallback } from 'react';
import { useNavigate, useParams, Link } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { api } from '../api/client';
import { Sidebar } from '../components/Sidebar';
import { TricolourBadge } from '../components/TricolourBadge';
import { AlertBanner } from '../components/AlertBanner';

export function ConversationPage() {
  const { user } = useAuth();
  const navigate = useNavigate();
  const { conversationId } = useParams();

  const [conversation, setConversation] = useState(null);
  const [messages, setMessages] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [inputText, setInputText] = useState('');
  const [sending, setSending] = useState(false);
  const [sendError, setSendError] = useState(null);

  const messagesEndRef = useRef(null);
  const messagesContainerRef = useRef(null);
  const pollingTimerRef = useRef(null);
  const activeConvIdRef = useRef(conversationId);

  activeConvIdRef.current = conversationId;

  const scrollToBottom = (smooth = true) => {
    if (messagesEndRef.current) {
      messagesEndRef.current.scrollIntoView({ behavior: smooth ? 'smooth' : 'auto' });
    }
  };

  // Format timestamp helper
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

  // Initial Load & Fallback Resolution
  useEffect(() => {
    let ignore = false;

    async function loadInitial() {
      try {
        setLoading(true);
        setError(null);

        // If no conversationId passed in URL, attempt to find latest or redirect to dashboard
        let targetId = conversationId;
        if (!targetId) {
          const listRes = await api.getConversations();
          if (listRes?.conversations?.length > 0) {
            navigate(`/conversation/${listRes.conversations[0].id}`, { replace: true });
            return;
          } else {
            navigate('/dashboard', { replace: true });
            return;
          }
        }

        const data = await api.getMessages(targetId, { limit: 50 });
        if (ignore) return;

        setConversation(data?.conversation || null);
        setMessages(data?.messages || []);

        // Mark as read immediately on load
        api.markConversationAsRead(targetId).catch(() => {});
      } catch (err) {
        if (!ignore) {
          setError(err.message || 'Failed to load conversation history.');
        }
      } finally {
        if (!ignore) {
          setLoading(false);
        }
      }
    }

    loadInitial();

    return () => {
      ignore = true;
    };
  }, [conversationId, navigate]);

  // Scroll to bottom after initial load
  useEffect(() => {
    if (!loading && messages.length > 0) {
      scrollToBottom(false);
    }
  }, [loading]);

  // Authenticated Incremental Polling (every 2.5 seconds)
  useEffect(() => {
    if (!conversationId) return;

    const poll = async () => {
      if (!activeConvIdRef.current) return;
      try {
        // Calculate max message ID currently in state
        let maxId = 0;
        setMessages((prevMsgs) => {
          if (prevMsgs.length > 0) {
            maxId = prevMsgs[prevMsgs.length - 1].id;
          }
          return prevMsgs;
        });

        const data = await api.getMessages(activeConvIdRef.current, { after: maxId, limit: 50 });
        if (data?.messages && data.messages.length > 0) {
          setMessages((prev) => {
            const existingIds = new Set(prev.map((m) => m.id));
            const newOnes = data.messages.filter((m) => !existingIds.has(m.id));
            if (newOnes.length === 0) return prev;
            return [...prev, ...newOnes];
          });

          // Mark incoming as read
          api.markConversationAsRead(activeConvIdRef.current).catch(() => {});
          scrollToBottom(true);
        }
      } catch {
        // Silent catch for background polling recovery
      }
    };

    pollingTimerRef.current = setInterval(poll, 2500);

    return () => {
      if (pollingTimerRef.current) {
        clearInterval(pollingTimerRef.current);
      }
    };
  }, [conversationId]);

  // Message Send Handler
  const handleSend = async (e) => {
    e?.preventDefault?.();
    const trimmed = inputText.trim();
    if (!trimmed || sending || !conversationId) return;

    try {
      setSending(true);
      setSendError(null);

      const res = await api.sendMessage(conversationId, trimmed);
      if (res?.message) {
        setMessages((prev) => [...prev, res.message]);
        setInputText('');
        scrollToBottom(true);
      } else {
        throw new Error('Server did not confirm message delivery.');
      }
    } catch (err) {
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
    <div className="min-h-screen bg-surface flex text-on-surface">
      <Sidebar />

      <div className="pl-[68px] flex-1 flex flex-col h-screen overflow-hidden">
        {/* Top Header Bar */}
        <header className="h-14 bg-surface-container-lowest border-b border-border-hairline z-40 flex items-center justify-between px-6 shrink-0">
          <div className="flex items-center gap-3">
            <button
              onClick={() => navigate('/dashboard')}
              className="w-8 h-8 rounded-lg hover:bg-surface-container flex items-center justify-center text-on-surface-variant transition-colors"
              title="Back to Dashboard"
            >
              <span className="material-symbols-outlined text-[20px]">arrow_back</span>
            </button>

            {otherUser ? (
              <div className="flex items-center gap-3">
                <div className="relative">
                  <div className="w-8 h-8 rounded-full bg-primary-container text-white flex items-center justify-center text-xs font-bold shadow-xs">
                    {initials}
                  </div>
                  <span className="absolute bottom-0 right-0 w-2.5 h-2.5 bg-india-green rounded-full ring-2 ring-surface-container-lowest"></span>
                </div>
                <div>
                  <div className="text-sm font-bold text-primary flex items-center gap-1.5">
                    <span>{otherUser.fullName}</span>
                    <TricolourBadge size="sm" />
                  </div>
                  <div className="text-[11px] font-mono text-outline leading-none">
                    @{otherUser.username}
                  </div>
                </div>
              </div>
            ) : (
              <div className="flex items-center gap-2">
                <img
                  src="/sampark-logo.png"
                  alt="Sampark"
                  className="h-7 w-auto object-contain cursor-pointer"
                  onClick={() => navigate('/dashboard')}
                />
                <span className="text-outline/50">•</span>
                <span className="text-sm font-medium text-on-surface-variant">Conversation</span>
              </div>
            )}
          </div>

          <div className="flex items-center gap-3">
            <button
              onClick={() => navigate('/find-people')}
              className="px-3 py-1 rounded-lg bg-surface-container-low hover:bg-surface-container text-xs font-semibold text-primary border border-border-hairline transition-colors flex items-center gap-1.5"
            >
              <span className="material-symbols-outlined text-[16px]">person_search</span>
              <span className="hidden sm:inline">Find People</span>
            </button>
            <div className="w-8 h-8 rounded-full bg-primary flex items-center justify-center text-white text-xs font-semibold shadow-xs">
              {(user?.fullName || user?.username || 'U')[0].toUpperCase()}
            </div>
          </div>
        </header>

        {/* Main Conversation Canvas */}
        <div className="flex-1 flex flex-col justify-between overflow-hidden bg-surface-container-low/30 max-w-4xl mx-auto w-full">
          {/* Messages Stream Container */}
          <div
            ref={messagesContainerRef}
            className="flex-1 overflow-y-auto p-4 sm:p-6 space-y-4"
          >
            {/* Error Banner */}
            {error && (
              <div className="mb-4">
                <AlertBanner type="error" message={error} onClose={() => setError(null)} />
                <button
                  onClick={() => navigate('/dashboard')}
                  className="mt-2 text-xs font-semibold text-secondary hover:underline"
                >
                  Return to Dashboard
                </button>
              </div>
            )}

            {/* Loading Skeletons */}
            {loading && (
              <div className="space-y-4 pt-6">
                <div className="flex items-end gap-2.5 max-w-[70%]">
                  <div className="w-8 h-8 rounded-full bg-surface-container-high animate-pulse"></div>
                  <div className="h-14 w-56 bg-surface-container rounded-2xl animate-pulse"></div>
                </div>
                <div className="flex items-end justify-end gap-2.5 max-w-[70%] ml-auto">
                  <div className="h-10 w-44 bg-primary/20 rounded-2xl animate-pulse"></div>
                </div>
                <div className="flex items-end gap-2.5 max-w-[70%]">
                  <div className="w-8 h-8 rounded-full bg-surface-container-high animate-pulse"></div>
                  <div className="h-12 w-64 bg-surface-container rounded-2xl animate-pulse"></div>
                </div>
              </div>
            )}

            {/* Empty State: No messages in this conversation yet */}
            {!loading && !error && messages.length === 0 && (
              <div className="h-full flex flex-col items-center justify-center text-center p-8 my-auto select-none">
                <div className="w-16 h-16 rounded-2xl bg-surface-container-lowest border border-border-hairline flex items-center justify-center text-primary mb-3 shadow-xs">
                  <span className="material-symbols-outlined text-[32px] text-secondary">forum</span>
                </div>
                <h3 className="text-base font-bold text-primary mb-1">
                  Start your conversation with {otherUser?.fullName || 'this member'}
                </h3>
                <p className="text-xs text-on-surface-variant max-w-sm leading-relaxed mb-4">
                  Send a private message to begin chatting. Messages are securely stored and delivered in real time.
                </p>
                <div className="inline-flex items-center gap-1.5 px-3 py-1 rounded-full bg-surface-container-low border border-border-hairline text-[11px] font-mono text-outline">
                  <span className="w-2 h-2 rounded-full bg-india-green"></span>
                  <span>End-to-End Private Channel</span>
                </div>
              </div>
            )}

            {/* Message Stream */}
            {!loading && messages.length > 0 && (
              <>
                <div className="text-center my-4">
                  <span className="px-3 py-1 rounded-full bg-surface-container-high text-[11px] font-medium text-outline">
                    {formatDateHeader(messages[0]?.createdAt)}
                  </span>
                </div>

                {messages.map((msg) => {
                  const isSelf = msg.senderId === user?.id;

                  return (
                    <div
                      key={msg.id}
                      className={`flex items-end gap-2.5 max-w-[85%] sm:max-w-[75%] ${
                        isSelf ? 'ml-auto justify-end' : 'justify-start'
                      }`}
                    >
                      {/* Peer Avatar on left */}
                      {!isSelf && (
                        <div
                          className="w-8 h-8 rounded-full bg-primary-container text-white flex items-center justify-center text-xs font-semibold shrink-0 shadow-xs select-none"
                          title={otherUser?.fullName}
                        >
                          {initials}
                        </div>
                      )}

                      {/* Message Bubble */}
                      <div
                        className={`p-3.5 rounded-2xl text-sm leading-relaxed shadow-xs transition-all ${
                          isSelf
                            ? 'bg-primary text-white rounded-br-xs'
                            : 'bg-surface-container-lowest text-on-surface border border-border-hairline rounded-bl-xs'
                        }`}
                      >
                        <p className="whitespace-pre-wrap break-words">{msg.body}</p>
                        <div
                          className={`text-[10px] mt-1.5 flex items-center justify-end gap-1 font-mono ${
                            isSelf ? 'text-white/70' : 'text-outline'
                          }`}
                        >
                          <span>{formatTime(msg.createdAt)}</span>
                          {isSelf && (
                            <span className="material-symbols-outlined text-[13px] leading-none">
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
          </div>

          {/* Composer Footer */}
          <div className="p-4 bg-surface-container-lowest border-t border-border-hairline">
            {sendError && (
              <div className="mb-3">
                <AlertBanner type="error" message={sendError} onClose={() => setSendError(null)} />
              </div>
            )}

            <form onSubmit={handleSend} className="flex items-center gap-2 max-w-3xl mx-auto">
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
                className="flex-1 py-2.5 px-4 bg-surface-container-low focus:bg-surface-container-lowest text-sm text-on-surface placeholder:text-outline rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all shadow-xs disabled:opacity-50"
              />

              <button
                type="submit"
                disabled={!inputText.trim() || sending || loading}
                className="px-4 py-2.5 rounded-xl bg-primary hover:bg-primary-hover active:scale-95 text-white text-sm font-semibold flex items-center gap-1.5 shadow-xs transition-all cursor-pointer disabled:opacity-40 disabled:cursor-not-allowed disabled:transform-none shrink-0"
              >
                {sending ? (
                  <span className="w-4 h-4 border-2 border-white border-t-transparent rounded-full animate-spin"></span>
                ) : (
                  <span className="material-symbols-outlined text-[18px]">send</span>
                )}
                <span className="hidden sm:inline">Send</span>
              </button>
            </form>
          </div>
        </div>
      </div>
    </div>
  );
}
