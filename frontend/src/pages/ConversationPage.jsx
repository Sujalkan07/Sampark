import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { Sidebar } from '../components/Sidebar';
import { TricolourBadge } from '../components/TricolourBadge';

export function ConversationPage() {
  const { user } = useAuth();
  const navigate = useNavigate();
  const [inputText, setInputText] = useState('');
  const [showNotice, setShowNotice] = useState(false);

  const handleSend = (e) => {
    e.preventDefault();
    if (!inputText.trim()) return;
    setShowNotice(true);
    setInputText('');
  };

  return (
    <div className="min-h-screen bg-surface flex text-on-surface">
      <Sidebar />

      <div className="pl-[68px] flex-1 flex flex-col h-screen overflow-hidden">
        {/* Top Header */}
        <header className="h-14 bg-surface-container-lowest border-b border-border-hairline z-40 flex items-center justify-between px-6 shrink-0">
          <div className="flex items-center gap-3">
            <span className="font-bold text-base text-primary tracking-tight">ConnectX</span>
            <span className="text-outline/50">•</span>
            <span className="text-sm font-medium text-on-surface-variant">Conversation Preview</span>
          </div>

          <div className="flex items-center gap-3">
            <button
              onClick={() => navigate('/dashboard')}
              className="px-3 py-1 rounded-lg bg-surface-container-low hover:bg-surface-container text-xs font-semibold text-primary border border-border-hairline transition-colors"
            >
              Back to Dashboard
            </button>
            <div className="w-8 h-8 rounded-full bg-primary flex items-center justify-center text-white text-xs font-semibold shadow-xs">
              {(user?.fullName || user?.username || 'U')[0].toUpperCase()}
            </div>
          </div>
        </header>

        {/* Informational Banner */}
        <div className="bg-amber-50/70 border-b border-amber-200/80 px-6 py-2.5 text-xs text-amber-900 flex items-center justify-between">
          <div className="flex items-center gap-2">
            <span className="material-symbols-outlined text-[18px] text-saffron">info</span>
            <span>
              Preview mode. Direct one-to-one messaging will be enabled in the upcoming release.
            </span>
          </div>
          <button
            onClick={() => setShowNotice(false)}
            className="text-amber-700 hover:text-amber-900 font-bold"
          >
            ×
          </button>
        </div>

        {/* Conversation View */}
        <div className="flex-1 flex flex-col justify-between overflow-hidden bg-surface-container-low/30 p-4 sm:p-6 max-w-4xl mx-auto w-full">
          {/* Messages Stream */}
          <div className="flex-1 overflow-y-auto space-y-4 pr-2">
            {/* Timestamp separator */}
            <div className="text-center my-4">
              <span className="px-3 py-1 rounded-full bg-surface-container-high text-[11px] font-medium text-outline">
                Conversation Preview
              </span>
            </div>

            {/* Peer Incoming Bubble (Left aligned) */}
            <div className="flex items-end gap-2.5 max-w-[80%]">
              <div className="w-8 h-8 rounded-full bg-primary-container text-white flex items-center justify-center text-xs font-semibold shrink-0">
                CX
              </div>
              <div className="bg-surface-container-lowest border border-border-hairline text-on-surface p-3.5 rounded-2xl rounded-bl-xs shadow-xs text-sm leading-relaxed">
                <p className="font-semibold text-xs text-primary mb-1">ConnectX Welcome</p>
                <p>
                  Welcome to ConnectX, <strong>{user?.fullName || user?.username}</strong>! Your account is active and ready to connect.
                </p>
                <span className="text-[10px] text-outline block text-right mt-1 font-mono">10:00 AM</span>
              </div>
            </div>

            {/* Self Outgoing Bubble (Right aligned, Ashoka Navy) */}
            <div className="flex items-end justify-end gap-2.5 max-w-[80%] ml-auto">
              <div className="bg-primary text-white p-3.5 rounded-2xl rounded-br-xs shadow-xs text-sm leading-relaxed">
                <p>Hello! Looking forward to connecting and sharing updates.</p>
                <span className="text-[10px] text-white/70 block text-right mt-1 font-mono">10:02 AM • Sent</span>
              </div>
            </div>

            {/* Notice if user types and sends */}
            {showNotice && (
              <div className="p-3 rounded-xl bg-blue-50 border border-blue-200 text-xs text-blue-900 text-center animate-fade-in">
                Live chat messaging will be active in the upcoming release.
              </div>
            )}
          </div>

          {/* Composer Box */}
          <div className="pt-4 shrink-0">
            <form onSubmit={handleSend} className="relative flex items-center gap-2">
              <input
                type="text"
                value={inputText}
                onChange={(e) => setInputText(e.target.value)}
                placeholder="Type a message..."
                className="flex-1 bg-surface-container-lowest text-sm text-on-surface placeholder:text-outline pl-4 pr-12 py-3 rounded-xl border border-border-hairline focus:border-secondary focus:outline-none shadow-xs transition-all"
              />
              <button
                type="submit"
                className="w-10 h-10 rounded-xl bg-primary hover:bg-primary-hover text-white flex items-center justify-center transition-colors shadow-xs cursor-pointer shrink-0"
                title="Send"
              >
                <span className="material-symbols-outlined text-[18px]">send</span>
              </button>
            </form>
          </div>
        </div>
      </div>
    </div>
  );
}
