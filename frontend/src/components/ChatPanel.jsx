import React, { useState, useEffect, useRef, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { usePreferences } from '../context/PreferencesContext';
import { api } from '../api/client';
import { TricolourBadge } from './TricolourBadge';
import { AlertBanner } from './AlertBanner';
import { ProfileModal } from './ProfileModal';
import { GroupDetailsModal } from './GroupDetailsModal';
import { EmojiPicker } from './EmojiPicker';
import { parseUtcDate, getPresenceStatus, formatMessageTime } from '../utils/date';
import { playNotificationChime } from '../utils/sound';

function formatFileSize(bytes) {
  if (!bytes || bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

const QUICK_REACTIONS = ['👍', '❤️', '😂', '😮', '😢', '🙏'];

export function ChatPanel({ conversationId, onConversationActivity, onBack }) {
  const { user } = useAuth();
  const { preferences } = usePreferences();
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

  // In-Chat Search State
  const [searchOpen, setSearchOpen] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState([]);
  const [searching, setSearching] = useState(false);

  // Profile & Group Modals State
  const [profileModalOpen, setProfileModalOpen] = useState(false);
  const [groupModalOpen, setGroupModalOpen] = useState(false);

  // Inline Message Editing & Deletion State
  const [editingMsgId, setEditingMsgId] = useState(null);
  const [editBody, setEditBody] = useState('');
  const [actionError, setActionError] = useState(null);

  // Phase 4: Replies, Emoji Picker, Attachments & Reactions
  const [stagedReply, setStagedReply] = useState(null);
  const [stagedFile, setStagedFile] = useState(null);
  const [filePreviewUrl, setFilePreviewUrl] = useState(null);
  const [emojiPickerOpen, setEmojiPickerOpen] = useState(false);
  const [reactionPickerMsgId, setReactionPickerMsgId] = useState(null);
  const [highlightedMsgId, setHighlightedMsgId] = useState(null);
  const [lightboxUrl, setLightboxUrl] = useState(null);

  const messagesEndRef = useRef(null);
  const messagesContainerRef = useRef(null);
  const textInputRef = useRef(null);
  const fileInputRef = useRef(null);
  const pollingTimerRef = useRef(null);
  const activeConvIdRef = useRef(conversationId);
  const latestMessageIdRef = useRef(0);
  const oldestMessageIdRef = useRef(0);
  const isPollingRef = useRef(false);
  const messagesRef = useRef([]);

  messagesRef.current = messages;
  activeConvIdRef.current = conversationId;

  const isGroup = conversation?.type === 'group';

  const scrollToBottom = (smooth = true) => {
    if (messagesEndRef.current) {
      messagesEndRef.current.scrollIntoView({ behavior: smooth ? 'smooth' : 'auto' });
    }
  };

  const formatTime = (isoString) => formatMessageTime(isoString);

  const formatDateHeader = (isoString) => {
    const d = parseUtcDate(isoString);
    if (!d) return '';
    const today = new Date();
    if (d.toDateString() === today.toDateString()) {
      return 'Today';
    }
    return d.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' });
  };

  const getPresence = (lastActiveAt) => getPresenceStatus(lastActiveAt);

  // Load conversation details
  const refreshConversationMetadata = useCallback(async () => {
    if (!conversationId) return;
    try {
      const data = await api.getConversation(conversationId);
      if (data?.conversation && activeConvIdRef.current === conversationId) {
        setConversation(data.conversation);
      }
    } catch {}
  }, [conversationId]);

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
        setStagedReply(null);
        setStagedFile(null);
        setFilePreviewUrl(null);
        setEmojiPickerOpen(false);
        setReactionPickerMsgId(null);
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

  // Authenticated Incremental Polling
  useEffect(() => {
    if (!conversationId) return;

    let pollCounter = 0;

    const poll = async () => {
      if (isPollingRef.current) return;
      const targetId = activeConvIdRef.current;
      if (!targetId || targetId !== conversationId) return;

      try {
        isPollingRef.current = true;
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

          api.markConversationAsRead(targetId).catch(() => {});

          const incomingCount = newOnes.filter((m) => m.senderId !== user?.id).length;

          if (incomingCount > 0 && preferences.sound_enabled) {
            playNotificationChime();
          }

          if (
            incomingCount > 0 &&
            preferences.desktop_notifications_enabled &&
            typeof document !== 'undefined' &&
            document.hidden &&
            'Notification' in window &&
            Notification.permission === 'granted'
          ) {
            const lastIncoming = newOnes.filter((m) => m.senderId !== user?.id).pop();
            const title = lastIncoming?.senderName || 'Sampark';
            const body = lastIncoming?.body || (lastIncoming?.attachment ? 'Sent an attachment' : 'New message');
            try {
              new Notification(title, { body, icon: '/sampark-logo.png' });
            } catch {}
          }

          onConversationActivity?.({
            conversationId: targetId,
            lastMessage: latest,
            isSentByMe: latest.senderId === user?.id,
            incomingCount,
          });

          if (!searchOpen) {
            scrollToBottom(true);
          }
        }

        // Every 3rd poll cycle (~7.5s), refresh metadata & reaction updates
        if (pollCounter % 3 === 0) {
          refreshConversationMetadata();

          const fullData = await api.getMessages(targetId, { limit: 50 });
          if (fullData?.messages && activeConvIdRef.current === targetId) {
            const freshMap = new Map(fullData.messages.map((m) => [m.id, m]));
            setMessages((current) =>
              current.map((msg) => {
                const updated = freshMap.get(msg.id);
                if (updated) {
                  return {
                    ...msg,
                    readAt: updated.readAt,
                    isDeleted: updated.isDeleted,
                    body: updated.body,
                    editedAt: updated.editedAt,
                    reactions: updated.reactions || [],
                    replyTo: updated.replyTo,
                    attachment: updated.attachment,
                  };
                }
                return msg;
              })
            );
          }
        }
      } catch {
      } finally {
        isPollingRef.current = false;
      }
    };

    pollingTimerRef.current = setInterval(poll, 2500);

    return () => {
      if (pollingTimerRef.current) {
        clearInterval(pollingTimerRef.current);
      }
    };
  }, [conversationId, user?.id, onConversationActivity, searchOpen, preferences, refreshConversationMetadata]);

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
        const existingIds = new Set(messagesRef.current.map((m) => m.id));
        const newOlder = olderMessages.filter((m) => !existingIds.has(m.id));
        if (newOlder.length > 0) {
          const minOlderId = newOlder.reduce((min, m) => Math.min(min, m.id), newOlder[0].id);
          oldestMessageIdRef.current = minOlderId;
          setMessages((prev) => [...newOlder, ...prev]);
        }

        if (olderMessages.length < 30) {
          setHasMoreOlder(false);
        }

        requestAnimationFrame(() => {
          if (messagesContainerRef.current) {
            const newScrollHeight = messagesContainerRef.current.scrollHeight;
            messagesContainerRef.current.scrollTop = newScrollHeight - prevScrollHeight;
          }
        });
      }
    } catch {
    } finally {
      setLoadingOlder(false);
    }
  };

  // Jump to message by ID
  const handleJumpToMessage = (targetMsgId) => {
    const el = document.getElementById(`msg-${targetMsgId}`);
    if (el) {
      el.scrollIntoView({ behavior: 'smooth', block: 'center' });
      setHighlightedMsgId(targetMsgId);
      setTimeout(() => {
        setHighlightedMsgId(null);
      }, 2000);
    }
  };

  // File Attachment Handling
  const handleFileSelect = (e) => {
    const file = e.target.files?.[0];
    if (!file) return;

    if (file.size > 10 * 1024 * 1024) {
      setSendError('File exceeds the 10MB limit.');
      return;
    }

    setStagedFile(file);
    setSendError(null);

    if (file.type.startsWith('image/')) {
      const url = URL.createObjectURL(file);
      setFilePreviewUrl(url);
    } else {
      setFilePreviewUrl(null);
    }
  };

  const handleRemoveStagedFile = () => {
    if (filePreviewUrl) {
      URL.revokeObjectURL(filePreviewUrl);
    }
    setStagedFile(null);
    setFilePreviewUrl(null);
    if (fileInputRef.current) {
      fileInputRef.current.value = '';
    }
  };

  // Emoji insertion into composer
  const handleInsertEmoji = (emoji) => {
    const input = textInputRef.current;
    if (!input) {
      setInputText((prev) => prev + emoji);
      return;
    }
    const start = input.selectionStart || inputText.length;
    const end = input.selectionEnd || inputText.length;
    const nextText = inputText.substring(0, start) + emoji + inputText.substring(end);
    setInputText(nextText);
    requestAnimationFrame(() => {
      input.focus();
      const pos = start + emoji.length;
      input.setSelectionRange(pos, pos);
    });
  };

  // Emoji Reaction Toggle
  const handleToggleReaction = async (messageId, emoji) => {
    try {
      setReactionPickerMsgId(null);
      const res = await api.toggleReaction(messageId, emoji);
      if (res?.reactions) {
        setMessages((prev) =>
          prev.map((m) => (m.id === messageId ? { ...m, reactions: res.reactions } : m))
        );
      }
    } catch (err) {
      console.warn('Failed to toggle reaction:', err);
    }
  };

  // Message Send Handler
  const handleSend = async (e) => {
    e?.preventDefault?.();
    const trimmed = inputText.trim();
    if ((!trimmed && !stagedFile) || sending || !conversationId) return;

    const targetId = conversationId;

    try {
      setSending(true);
      setSendError(null);

      let attachmentId = null;
      if (stagedFile) {
        const uploadRes = await api.uploadAttachment(targetId, stagedFile);
        if (!uploadRes?.attachment?.id) {
          throw new Error('Failed to upload file attachment.');
        }
        attachmentId = uploadRes.attachment.id;
      }

      const payload = {
        body: trimmed || stagedFile?.name || '',
        attachmentId: attachmentId || undefined,
        replyToId: stagedReply?.id || undefined,
      };

      const res = await api.sendMessage(targetId, payload);
      if (activeConvIdRef.current !== targetId) return;

      if (res?.message) {
        setMessages((prev) => [...prev, res.message]);
        if (res.message.id > latestMessageIdRef.current) {
          latestMessageIdRef.current = res.message.id;
        }
        setInputText('');
        setStagedReply(null);
        handleRemoveStagedFile();
        setEmojiPickerOpen(false);

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
      setSendError(err.message || 'Failed to send message. Please try again.');
    } finally {
      setSending(false);
    }
  };

  // Message Editing Handler
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

  // Message Deletion Handler
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

  // Search Messages in Conversation
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

  const directInitials = (otherUser?.fullName || otherUser?.username || 'U')
    .split(' ')
    .map((n) => n[0])
    .join('')
    .substring(0, 2)
    .toUpperCase();

  const groupInitials = (conversation?.name || 'G')
    .split(' ')
    .map((n) => n[0])
    .join('')
    .substring(0, 2)
    .toUpperCase();

  const displayMessages = searchOpen && searchQuery.trim() ? searchResults : messages;

  return (
    <div className="flex-1 flex flex-col h-full bg-surface min-w-0 overflow-hidden relative">
      {/* Top Header */}
      <header className="h-16 px-4 sm:px-6 bg-surface-container-lowest border-b border-border-hairline z-10 shrink-0 shadow-xs flex items-center justify-between">
        <div className="flex items-center gap-3 min-w-0">
          <button
            onClick={onBack || (() => navigate('/dashboard'))}
            className="md:hidden w-8 h-8 rounded-lg hover:bg-surface-container flex items-center justify-center text-on-surface-variant transition-colors cursor-pointer shrink-0"
            title="Back to conversation list"
            type="button"
          >
            <span className="material-symbols-outlined text-[22px]">arrow_back</span>
          </button>

          {/* Participant or Group Info */}
          <div
            className="flex items-center gap-3 cursor-pointer group min-w-0"
            onClick={() => {
              if (isGroup) {
                setGroupModalOpen(true);
              } else {
                setProfileModalOpen(true);
              }
            }}
            title={isGroup ? 'View group info & members' : 'View member profile'}
          >
            <div className="relative shrink-0">
              {isGroup ? (
                <div className="w-10 h-10 rounded-full bg-linear-to-tr from-[#1B365D] to-[#2E5B88] text-white flex items-center justify-center text-xs font-bold shadow-xs select-none group-hover:ring-2 group-hover:ring-secondary/40 transition-all">
                  <span className="material-symbols-outlined text-[20px]">groups</span>
                </div>
              ) : (
                <div className="w-10 h-10 rounded-full bg-primary-container text-white flex items-center justify-center text-xs font-bold shadow-xs select-none group-hover:ring-2 group-hover:ring-secondary/40 transition-all">
                  {directInitials}
                </div>
              )}
              {!isGroup && presence.isOnline && (
                <span
                  className="absolute bottom-0 right-0 w-2.5 h-2.5 bg-india-green rounded-full ring-2 ring-surface-container-lowest"
                  title="Active now"
                ></span>
              )}
            </div>

            <div className="flex flex-col min-w-0">
              <div className="flex items-center gap-2">
                <span className="text-sm font-bold text-primary truncate group-hover:text-secondary transition-colors">
                  {isGroup ? conversation?.name : (otherUser?.fullName || 'Conversation')}
                </span>
                <TricolourBadge size="sm" />
              </div>
              <div className="flex items-center gap-2 text-xs font-mono text-outline leading-tight">
                {isGroup ? (
                  <span>
                    {conversation?.members?.length || conversation?.memberCount || 0} members • Tap for details
                  </span>
                ) : (
                  <>
                    <span>@{otherUser?.username || 'member'}</span>
                    <span className="text-outline/40">•</span>
                    <span
                      className={`font-sans text-[11px] ${
                        presence.isOnline ? 'text-india-green font-semibold' : 'text-on-surface-variant/80'
                      }`}
                    >
                      {presence.label}
                    </span>
                  </>
                )}
              </div>
            </div>
          </div>
        </div>

        {/* Header Utility Actions */}
        <div className="flex items-center gap-1.5 text-on-surface-variant">
          <button
            type="button"
            onClick={() => setSearchOpen((prev) => !prev)}
            className={`w-9 h-9 rounded-xl flex items-center justify-center transition-colors cursor-pointer ${
              searchOpen
                ? 'bg-primary text-white shadow-xs'
                : 'hover:bg-surface-container hover:text-on-surface'
            }`}
            title="Search inside conversation"
          >
            <span className="material-symbols-outlined text-[20px]">search</span>
          </button>

          <button
            type="button"
            onClick={() => {
              if (isGroup) {
                setGroupModalOpen(true);
              } else {
                setProfileModalOpen(true);
              }
            }}
            className="w-9 h-9 rounded-xl flex items-center justify-center hover:bg-surface-container hover:text-on-surface transition-colors cursor-pointer"
            title={isGroup ? 'Group Information & Members' : 'Member Information'}
          >
            <span className="material-symbols-outlined text-[20px]">
              {isGroup ? 'group' : 'info'}
            </span>
          </button>
        </div>
      </header>

      {/* In-Chat Search Bar Overlay */}
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
        {error && (
          <div className="mb-4">
            <AlertBanner type="error" message={error} onClose={() => setError(null)} />
          </div>
        )}

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

        {/* Empty State */}
        {!loading && !error && messages.length === 0 && (
          <div className="h-full flex flex-col items-center justify-center text-center p-8 my-auto select-none">
            <div className="w-14 h-14 rounded-2xl bg-surface-container-lowest border border-border-hairline flex items-center justify-center text-primary mb-3 shadow-xs">
              <span className="material-symbols-outlined text-[30px] text-secondary">
                {isGroup ? 'groups' : 'forum'}
              </span>
            </div>
            <h3 className="text-base font-bold text-primary mb-1">
              {isGroup
                ? `Welcome to ${conversation?.name}`
                : `Start your conversation with ${otherUser?.fullName || 'this member'}`}
            </h3>
            <p className="text-xs text-on-surface-variant max-w-sm leading-relaxed mb-4">
              {isGroup
                ? 'Send a message, share photos, react with emojis, or reply to teammates.'
                : 'Send a message below to connect. Messages are persisted securely in your private chat history.'}
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
              const isHighlighted = highlightedMsgId === msg.id;
              const senderInitials = (msg.senderName || msg.senderUsername || 'U')
                .split(' ')
                .map((n) => n[0])
                .join('')
                .substring(0, 2)
                .toUpperCase();

              return (
                <div
                  key={msg.id}
                  id={`msg-${msg.id}`}
                  className={`group relative flex items-end gap-2.5 max-w-[85%] sm:max-w-xl transition-all duration-300 rounded-2xl ${
                    isSelf ? 'ml-auto justify-end' : 'justify-start'
                  } ${isHighlighted ? 'ring-2 ring-saffron bg-saffron/10 p-1' : ''}`}
                >
                  {/* Avatar on left for incoming messages */}
                  {!isSelf && (
                    <div
                      className="w-7 h-7 rounded-full bg-primary-container text-white flex items-center justify-center text-[10px] font-semibold shrink-0 shadow-xs select-none cursor-pointer"
                      title={msg.senderName || `@${msg.senderUsername}`}
                    >
                      {senderInitials}
                    </div>
                  )}

                  {/* Hover Actions Bar (both incoming and outgoing!) */}
                  {!msg.isDeleted && !isEditing && (
                    <div
                      className={`opacity-0 group-hover:opacity-100 transition-opacity flex items-center gap-0.5 bg-surface-container-lowest border border-border-hairline rounded-lg shadow-xs px-1 py-0.5 self-center ${
                        isSelf ? 'mr-1 order-first' : 'ml-1 order-last'
                      }`}
                    >
                      {/* Quick Reactions */}
                      <div className="hidden sm:flex items-center">
                        {QUICK_REACTIONS.slice(0, 3).map((emoji) => (
                          <button
                            key={emoji}
                            type="button"
                            onClick={() => handleToggleReaction(msg.id, emoji)}
                            className="w-6 h-6 rounded hover:bg-surface-container flex items-center justify-center text-xs cursor-pointer transition-transform hover:scale-125"
                            title={`React with ${emoji}`}
                          >
                            {emoji}
                          </button>
                        ))}
                      </div>

                      {/* Reaction Picker Button */}
                      <div className="relative">
                        <button
                          type="button"
                          onClick={() =>
                            setReactionPickerMsgId((curr) => (curr === msg.id ? null : msg.id))
                          }
                          className="w-6 h-6 rounded hover:bg-surface-container flex items-center justify-center text-outline hover:text-on-surface transition-colors cursor-pointer"
                          title="Add reaction"
                        >
                          <span className="material-symbols-outlined text-[15px]">add_reaction</span>
                        </button>

                        {reactionPickerMsgId === msg.id && (
                          <div className="absolute bottom-8 right-0 z-40 shadow-xl">
                            <EmojiPicker
                              onSelect={(emoji) => handleToggleReaction(msg.id, emoji)}
                              onClose={() => setReactionPickerMsgId(null)}
                            />
                          </div>
                        )}
                      </div>

                      {/* Reply Button */}
                      <button
                        type="button"
                        onClick={() => setStagedReply(msg)}
                        className="w-6 h-6 rounded hover:bg-surface-container flex items-center justify-center text-outline hover:text-on-surface transition-colors cursor-pointer"
                        title="Reply to message"
                      >
                        <span className="material-symbols-outlined text-[15px]">reply</span>
                      </button>

                      {/* Edit / Delete buttons for own messages */}
                      {isSelf && (
                        <>
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
                        </>
                      )}
                    </div>
                  )}

                  {/* Message Bubble Content */}
                  <div className="flex flex-col gap-1 max-w-full">
                    {/* Sender Label for Group Incoming Messages */}
                    {isGroup && !isSelf && (
                      <span className="text-[11px] font-bold text-secondary mb-0.5 leading-none px-1">
                        {msg.senderName || `@${msg.senderUsername}`}
                      </span>
                    )}

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
                            : 'bg-surface-container-low text-on-surface rounded-bl-xs border border-border-hairline/60'
                        }`}
                      >
                        {/* Reply-To Preview Banner */}
                        {msg.replyTo && (
                          <div
                            onClick={() => handleJumpToMessage(msg.replyTo.id)}
                            className={`p-2 mb-2 rounded-lg border-l-2 cursor-pointer text-xs transition-opacity hover:opacity-90 ${
                              isSelf
                                ? 'bg-white/15 border-white/80 text-white/95'
                                : 'bg-surface-container border-saffron text-on-surface-variant'
                            }`}
                            title="Click to jump to quoted message"
                          >
                            <div className="font-semibold text-[10px] uppercase tracking-wider opacity-85">
                              {msg.replyTo.senderName || `@${msg.replyTo.senderUsername}`}
                            </div>
                            <div className="truncate text-[11px] font-normal">
                              {msg.replyTo.body || 'Attachment'}
                            </div>
                          </div>
                        )}

                        {/* File Attachment Card */}
                        {msg.attachment && (
                          <div className="mb-2">
                            {msg.attachment.contentType?.startsWith('image/') ? (
                              <div className="rounded-xl overflow-hidden max-w-xs max-h-64 border border-border-hairline/50">
                                <img
                                  src={`/api/attachments/${msg.attachment.id}`}
                                  alt={msg.attachment.fileName}
                                  className="w-full h-full object-cover cursor-pointer hover:opacity-95 transition-opacity"
                                  onClick={() => setLightboxUrl(`/api/attachments/${msg.attachment.id}`)}
                                />
                              </div>
                            ) : (
                              <a
                                href={`/api/attachments/${msg.attachment.id}`}
                                download={msg.attachment.fileName}
                                target="_blank"
                                rel="noopener noreferrer"
                                className={`flex items-center gap-2.5 p-2.5 rounded-xl border transition-colors ${
                                  isSelf
                                    ? 'bg-white/10 border-white/20 hover:bg-white/20 text-white'
                                    : 'bg-surface-container-lowest border-border-hairline hover:bg-surface-container text-on-surface'
                                }`}
                              >
                                <span className="material-symbols-outlined text-[24px]">
                                  description
                                </span>
                                <div className="min-w-0 flex-1">
                                  <div className="text-xs font-semibold truncate">
                                    {msg.attachment.fileName}
                                  </div>
                                  <div className="text-[10px] opacity-75 font-mono">
                                    {formatFileSize(msg.attachment.fileSize)}
                                  </div>
                                </div>
                                <span className="material-symbols-outlined text-[18px]">
                                  download
                                </span>
                              </a>
                            )}
                          </div>
                        )}

                        {/* Message Text Body */}
                        {(!msg.attachment || msg.body !== msg.attachment.fileName) && (
                          <p className="whitespace-pre-wrap break-words">{msg.body}</p>
                        )}
                      </div>
                    )}

                    {/* Emoji Reactions Pills */}
                    {msg.reactions && msg.reactions.length > 0 && (
                      <div className="flex flex-wrap gap-1 px-1 mt-0.5">
                        {msg.reactions.map((r) => {
                          const hasReacted = r.users?.some((u) => u.id === user?.id);
                          const usernames = r.users?.map((u) => u.username).join(', ');
                          return (
                            <button
                              key={r.emoji}
                              type="button"
                              onClick={() => handleToggleReaction(msg.id, r.emoji)}
                              title={`Reacted by: ${usernames}`}
                              className={`inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-xs transition-colors border cursor-pointer ${
                                hasReacted
                                  ? 'bg-saffron/20 border-saffron text-primary font-bold shadow-2xs'
                                  : 'bg-surface-container-lowest border-border-hairline text-on-surface-variant hover:bg-surface-container'
                              }`}
                            >
                              <span>{r.emoji}</span>
                              <span className="text-[10px] font-mono font-semibold">{r.count}</span>
                            </button>
                          );
                        })}
                      </div>
                    )}

                    {/* Message Metadata Timestamp & Read Checkmarks */}
                    <div
                      className={`text-[10px] flex items-center gap-1.5 font-mono px-1 ${
                        isSelf ? 'justify-end text-outline' : 'justify-start text-outline'
                      }`}
                    >
                      <span>{formatTime(msg.createdAt)}</span>
                      {msg.editedAt && !msg.isDeleted && (
                        <span className="italic text-[10px] opacity-75">(edited)</span>
                      )}
                      {isSelf && !msg.isDeleted && preferences.read_receipts_enabled && (
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

        <div ref={messagesEndRef} />
      </section>

      {/* Message Composer Footer */}
      <footer className="p-3 sm:p-4 bg-surface-container-lowest border-t border-border-hairline shrink-0 shadow-xs relative">
        {sendError && (
          <div className="mb-3 max-w-3xl mx-auto">
            <AlertBanner type="error" message={sendError} onClose={() => setSendError(null)} />
          </div>
        )}

        {/* Staged Reply Banner */}
        {stagedReply && (
          <div className="max-w-3xl mx-auto mb-2 flex items-center justify-between p-2.5 bg-surface-container-low border-l-4 border-saffron rounded-lg shadow-2xs">
            <div className="min-w-0 pr-2">
              <div className="text-[11px] font-bold text-saffron flex items-center gap-1">
                <span className="material-symbols-outlined text-[14px]">reply</span>
                <span>Replying to {stagedReply.senderName || `@${stagedReply.senderUsername}`}</span>
              </div>
              <p className="text-xs text-on-surface-variant truncate">
                {stagedReply.body || (stagedReply.attachment ? '📎 Attachment' : '')}
              </p>
            </div>
            <button
              type="button"
              onClick={() => setStagedReply(null)}
              className="w-6 h-6 rounded hover:bg-surface-container flex items-center justify-center text-outline hover:text-on-surface transition-colors cursor-pointer"
              title="Cancel reply"
            >
              <span className="material-symbols-outlined text-[16px]">close</span>
            </button>
          </div>
        )}

        {/* Staged File Attachment Preview Card */}
        {stagedFile && (
          <div className="max-w-3xl mx-auto mb-2 flex items-center justify-between p-2.5 bg-surface-container-low border border-border-hairline rounded-xl shadow-2xs">
            <div className="flex items-center gap-2.5 min-w-0">
              {filePreviewUrl ? (
                <img
                  src={filePreviewUrl}
                  alt="preview"
                  className="w-10 h-10 rounded-lg object-cover border border-border-hairline"
                />
              ) : (
                <div className="w-10 h-10 rounded-lg bg-surface-container-high flex items-center justify-center text-primary">
                  <span className="material-symbols-outlined text-[20px]">attach_file</span>
                </div>
              )}
              <div className="min-w-0">
                <div className="text-xs font-semibold text-primary truncate max-w-xs">
                  {stagedFile.name}
                </div>
                <div className="text-[10px] text-outline font-mono">
                  {formatFileSize(stagedFile.size)}
                </div>
              </div>
            </div>
            <button
              type="button"
              onClick={handleRemoveStagedFile}
              className="w-6 h-6 rounded hover:bg-surface-container flex items-center justify-center text-outline hover:text-on-surface transition-colors cursor-pointer"
              title="Remove attachment"
            >
              <span className="material-symbols-outlined text-[16px]">close</span>
            </button>
          </div>
        )}

        {/* Emoji Picker Popover */}
        {emojiPickerOpen && (
          <div className="absolute bottom-20 right-4 sm:right-10 z-40 shadow-xl">
            <EmojiPicker
              onSelect={handleInsertEmoji}
              onClose={() => setEmojiPickerOpen(false)}
            />
          </div>
        )}

        {/* Typing Bar Form */}
        <form onSubmit={handleSend} className="max-w-3xl mx-auto flex flex-col gap-1.5">
          <div className="flex items-center gap-1.5 bg-surface-container-low rounded-xl px-2 py-1 border border-border-hairline focus-within:border-secondary transition-all shadow-xs">
            {/* Attachment Button */}
            <input
              type="file"
              ref={fileInputRef}
              onChange={handleFileSelect}
              className="hidden"
            />
            <button
              type="button"
              onClick={() => fileInputRef.current?.click()}
              className="w-8 h-8 rounded-lg hover:bg-surface-container flex items-center justify-center text-outline hover:text-primary transition-colors cursor-pointer shrink-0"
              title="Attach image or file (up to 10MB)"
            >
              <span className="material-symbols-outlined text-[20px]">attach_file</span>
            </button>

            {/* Input Field */}
            <input
              ref={textInputRef}
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
                isGroup
                  ? `Message in ${conversation?.name}...`
                  : otherUser
                  ? `Message @${otherUser.username}...`
                  : 'Type a message...'
              }
              className="flex-1 bg-transparent border-0 outline-none py-1.5 text-sm text-on-surface placeholder:text-outline disabled:opacity-50"
            />

            {/* Emoji Picker Toggle Button */}
            <button
              type="button"
              onClick={() => setEmojiPickerOpen((prev) => !prev)}
              className={`w-8 h-8 rounded-lg flex items-center justify-center transition-colors cursor-pointer shrink-0 ${
                emojiPickerOpen
                  ? 'bg-surface-container-high text-primary'
                  : 'hover:bg-surface-container text-outline hover:text-primary'
              }`}
              title="Add emoji"
            >
              <span className="material-symbols-outlined text-[20px]">mood</span>
            </button>

            {/* Send Button */}
            <button
              type="submit"
              disabled={(!inputText.trim() && !stagedFile) || sending || loading}
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

      {/* Image Lightbox Modal */}
      {lightboxUrl && (
        <div
          className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/80 backdrop-blur-xs cursor-pointer animate-fade-in"
          onClick={() => setLightboxUrl(null)}
        >
          <div className="relative max-w-4xl max-h-[90vh] overflow-hidden rounded-2xl bg-black">
            <img
              src={lightboxUrl}
              alt="Full resolution preview"
              className="w-full h-full object-contain max-h-[85vh]"
            />
            <button
              onClick={() => setLightboxUrl(null)}
              className="absolute top-3 right-3 w-8 h-8 rounded-full bg-black/60 text-white flex items-center justify-center hover:bg-black/80 transition-colors"
            >
              <span className="material-symbols-outlined text-[20px]">close</span>
            </button>
          </div>
        </div>
      )}

      {/* Peer Profile Modal */}
      {profileModalOpen && (
        <ProfileModal
          isOpen={profileModalOpen}
          onClose={() => setProfileModalOpen(false)}
          mode="view"
          targetUser={otherUser}
        />
      )}

      {/* Group Details & Management Modal */}
      {groupModalOpen && (
        <GroupDetailsModal
          isOpen={groupModalOpen}
          onClose={() => setGroupModalOpen(false)}
          conversationId={conversationId}
          onGroupUpdated={refreshConversationMetadata}
          onGroupLeft={() => {
            navigate('/dashboard');
          }}
        />
      )}
    </div>
  );
}

export default ChatPanel;
