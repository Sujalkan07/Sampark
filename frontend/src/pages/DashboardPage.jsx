import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { Sidebar } from '../components/Sidebar';
import { TricolourBadge } from '../components/TricolourBadge';

export function DashboardPage() {
  const { user } = useAuth();
  const navigate = useNavigate();

  const [searchQuery, setSearchQuery] = useState('');
  const [activeTab, setActiveTab] = useState('all');
  const [showNotice, setShowNotice] = useState(false);

  return (
    <div className="min-h-screen bg-surface flex text-on-surface">
      {/* 68px Fixed Sidebar */}
      <Sidebar />

      {/* Main Content Area offset by 68px */}
      <div className="pl-[68px] flex-1 flex flex-col h-screen overflow-hidden">
        {/* Top Header Bar */}
        <header className="h-14 bg-surface-container-lowest border-b border-border-hairline z-40 flex items-center justify-between px-6 shrink-0">
          <div className="flex items-center gap-3">
            <span className="font-bold text-base text-primary tracking-tight">ConnectX</span>
            <span className="text-outline/50">•</span>
            <span className="text-sm font-medium text-on-surface-variant">Messages</span>
          </div>

          <div className="flex items-center gap-4">
            <div className="hidden sm:flex items-center gap-2 px-2.5 py-1 rounded-lg bg-surface-container-low border border-border-hairline text-xs font-mono text-on-surface-variant">
              <span className="w-2 h-2 rounded-full bg-india-green"></span>
              <span>Logged in as @{user?.username}</span>
            </div>

            <div className="w-8 h-8 rounded-full bg-primary flex items-center justify-center text-white text-xs font-semibold shadow-xs">
              {(user?.fullName || user?.username || 'U')[0].toUpperCase()}
            </div>
          </div>
        </header>

        {/* Studio Body: Split View */}
        <div className="flex-1 flex overflow-hidden">
          {/* Conversation List Sidebar (340px) */}
          <aside className="w-full md:w-[340px] shrink-0 bg-surface-container-lowest border-r border-border-hairline flex flex-col h-full z-10">
            {/* Header */}
            <div className="p-5 pb-3">
              <div className="flex items-center justify-between mb-4">
                <div className="flex items-center gap-2.5">
                  <h1 className="text-lg font-bold text-primary tracking-tight">Messages</h1>
                  <span className="inline-flex items-center justify-center px-2 py-0.5 rounded-full bg-surface-container-high text-on-surface-variant text-xs font-semibold">
                    0
                  </span>
                </div>

                <button
                  onClick={() => setShowNotice(true)}
                  className="w-8 h-8 rounded-lg bg-surface-container-low hover:bg-surface-container text-primary flex items-center justify-center transition-all active:scale-95 group shadow-xs cursor-pointer"
                  title="Start new conversation"
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
                  placeholder="Search conversations..."
                  className="w-full pl-9 pr-3 py-2 bg-surface-container-low hover:bg-surface-container focus:bg-surface-container-lowest text-xs text-on-surface placeholder:text-outline rounded-lg border border-border-hairline focus:border-secondary focus:outline-none transition-all"
                />
              </div>

              {/* Filter Chips */}
              <div className="flex items-center gap-1.5 mt-3 pt-1">
                <button
                  type="button"
                  onClick={() => setActiveTab('all')}
                  className={`px-3 py-1 rounded-full text-xs font-semibold transition-colors ${
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
                  className={`px-3 py-1 rounded-full text-xs font-semibold transition-colors flex items-center gap-1.5 ${
                    activeTab === 'unread'
                      ? 'bg-primary text-white shadow-xs'
                      : 'text-on-surface-variant hover:bg-surface-container'
                  }`}
                >
                  <span>Unread</span>
                </button>
                <button
                  type="button"
                  onClick={() => setActiveTab('direct')}
                  className={`px-3 py-1 rounded-full text-xs font-semibold transition-colors ${
                    activeTab === 'direct'
                      ? 'bg-primary text-white shadow-xs'
                      : 'text-on-surface-variant hover:bg-surface-container'
                  }`}
                >
                  Direct
                </button>
              </div>
            </div>

            {/* Conversation Empty State List */}
            <div className="flex-1 overflow-y-auto px-4 py-6 flex flex-col items-center justify-center text-center">
              <div className="w-12 h-12 rounded-full bg-surface-container-high flex items-center justify-center text-on-surface-variant mb-3">
                <span className="material-symbols-outlined text-[24px]">chat_bubble_outline</span>
              </div>
              <h2 className="text-sm font-semibold text-primary mb-1">No conversations yet</h2>
              <p className="text-xs text-on-surface-variant max-w-[240px] leading-relaxed mb-4">
                Start by finding someone to connect with. New chats will appear here.
              </p>
              <button
                onClick={() => navigate('/find-people')}
                className="px-3.5 py-1.5 text-xs font-semibold rounded-lg bg-surface-container text-primary hover:bg-surface-container-high transition-colors border border-border-hairline"
              >
                Find People
              </button>
            </div>

            {/* Directory Card */}
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
                className="w-7 h-7 rounded-lg hover:bg-surface-container flex items-center justify-center text-on-surface-variant transition-colors"
                title="Open Directory"
              >
                <span className="material-symbols-outlined text-[16px]">arrow_forward</span>
              </button>
            </div>
          </aside>

          {/* Main Empty State Stage (Approved Stitch Empty State) */}
          <section className="hidden md:flex flex-1 bg-surface flex-col justify-between items-center relative overflow-hidden select-none p-6">
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

            {/* Notice Modal/Alert if triggered */}
            {showNotice && (
              <div className="w-full max-w-md my-2 p-3 bg-blue-50 border border-blue-200 rounded-xl text-xs text-blue-900 flex items-center justify-between z-20">
                <span>Direct messaging will be available in the upcoming release. You can search member profiles in Find People.</span>
                <button onClick={() => setShowNotice(false)} className="text-blue-700 hover:text-blue-900 font-bold ml-2">
                  ×
                </button>
              </div>
            )}

            {/* Centered Minimal Hero Panel */}
            <div className="w-full max-w-lg px-6 flex flex-col items-center text-center my-auto z-10">
              {/* Geometric Icon Emblem */}
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

              {/* Heading */}
              <h2 className="text-2xl font-bold text-primary tracking-tight mb-2">
                Your conversations start here.
              </h2>

              {/* Explanatory Subtext */}
              <p className="text-sm text-on-surface-variant max-w-sm mb-6 leading-relaxed">
                Choose someone from your chats or find a person on the ConnectX network to send your first message.
              </p>

              {/* Action Cluster */}
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

                <button
                  type="button"
                  onClick={() => setShowNotice(true)}
                  className="w-full sm:w-auto px-4 py-2.5 rounded-xl bg-surface-container-lowest hover:bg-surface-container text-on-surface border border-border-hairline font-semibold text-sm shadow-xs transition-colors flex items-center justify-center gap-1.5 cursor-pointer"
                >
                  <span className="material-symbols-outlined text-[18px] text-outline">dialpad</span>
                  <span>New Direct Message</span>
                </button>
              </div>

              {/* Keyboard Assist */}
              <div className="mt-8 flex items-center gap-2 text-outline text-xs">
                <span>Welcome to ConnectX,</span>
                <span className="font-semibold text-primary">{user?.fullName || user?.username}</span>
              </div>
            </div>

            {/* Bottom Status Footer */}
            <footer className="w-full flex items-center justify-center z-10">
              <div className="flex items-center gap-2 px-3 py-1.5 rounded-full bg-surface-container-low border border-border-hairline text-xs font-medium text-on-surface-variant">
                <span className="w-2 h-2 rounded-full bg-india-green"></span>
                <span>Connected to ConnectX Server • Active</span>
              </div>
            </footer>
          </section>
        </div>
      </div>
    </div>
  );
}
