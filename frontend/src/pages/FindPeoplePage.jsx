import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { Sidebar } from '../components/Sidebar';
import { TricolourBadge } from '../components/TricolourBadge';

export function FindPeoplePage() {
  const { user } = useAuth();
  const navigate = useNavigate();

  const [searchQuery, setSearchQuery] = useState('');
  const [activeFilter, setActiveFilter] = useState('all');

  return (
    <div className="min-h-screen bg-surface flex text-on-surface">
      <Sidebar />

      <div className="pl-[68px] flex-1 flex flex-col h-screen overflow-hidden">
        {/* Top Header */}
        <header className="h-14 bg-surface-container-lowest border-b border-border-hairline z-40 flex items-center justify-between px-6 shrink-0">
          <div className="flex items-center gap-3">
            <span className="font-bold text-base text-primary tracking-tight">ConnectX</span>
            <span className="text-outline/50">•</span>
            <span className="text-sm font-medium text-on-surface-variant">Find People</span>
          </div>

          <div className="flex items-center gap-4">
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
                Peer Network
              </span>
              <TricolourBadge size="sm" />
            </div>
            <h1 className="text-2xl font-bold text-primary tracking-tight">Find People</h1>
            <p className="text-sm text-on-surface-variant mt-1">
              Discover and connect with registered users on the ConnectX platform.
            </p>
          </div>

          {/* Search Bar */}
          <div className="relative w-full mb-4">
            <span className="material-symbols-outlined absolute left-3.5 top-1/2 -translate-y-1/2 text-outline text-[20px] pointer-events-none">
              search
            </span>
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search by name or @username..."
              className="w-full pl-11 pr-4 py-2.5 bg-surface-container-lowest text-sm text-on-surface placeholder:text-outline rounded-xl border border-border-hairline focus:border-secondary focus:outline-none shadow-xs transition-all"
            />
          </div>

          {/* Filter Chips */}
          <div className="flex items-center gap-2 mb-6 overflow-x-auto pb-1">
            <button
              onClick={() => setActiveFilter('all')}
              className={`px-3.5 py-1 rounded-full text-xs font-semibold transition-all ${
                activeFilter === 'all'
                  ? 'bg-primary text-white shadow-xs'
                  : 'bg-surface-container-lowest text-on-surface-variant border border-border-hairline hover:bg-surface-container-high'
              }`}
            >
              All Users
            </button>
            <button
              onClick={() => setActiveFilter('active')}
              className={`px-3.5 py-1 rounded-full text-xs font-semibold transition-all flex items-center gap-1.5 ${
                activeFilter === 'active'
                  ? 'bg-primary text-white shadow-xs'
                  : 'bg-surface-container-lowest text-on-surface-variant border border-border-hairline hover:bg-surface-container-high'
              }`}
            >
              <span className="w-1.5 h-1.5 rounded-full bg-india-green"></span>
              <span>Active</span>
            </button>
          </div>

          {/* Current User Record (Live Data from C++ backend) */}
          <div className="mb-6">
            <h2 className="text-xs font-semibold uppercase font-mono tracking-wider text-outline mb-3">
              Your Registered Profile (Current Session)
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
                    {user?.email} • Account #{user?.id}
                  </div>
                </div>
              </div>

              <button
                onClick={() => navigate('/dashboard')}
                className="px-3.5 py-1.5 text-xs font-semibold rounded-lg bg-surface-container-low hover:bg-surface-container text-primary border border-border-hairline transition-colors"
              >
                Back to Chats
              </button>
            </div>
          </div>

          {/* Honest Empty State / Phase 2 Boundary Card */}
          <div className="p-8 rounded-2xl bg-surface-container-low border border-border-hairline text-center flex flex-col items-center">
            <div className="w-12 h-12 rounded-2xl bg-surface-container-lowest border border-border-hairline flex items-center justify-center text-primary mb-3 shadow-xs">
              <span className="material-symbols-outlined text-[24px]">group_add</span>
            </div>
            <h3 className="text-base font-bold text-primary mb-1">
              Phase 1 Directory Shell
            </h3>
            <p className="text-xs text-on-surface-variant max-w-md leading-relaxed mb-4">
              Real account creation, PBKDF2 password authentication, and multi-user database isolation are live. Global peer search and conversation initialization will be enabled in Phase 2.
            </p>
            <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-surface-container-highest border border-border-hairline text-[11px] font-mono text-on-surface-variant">
              <span>Database table: <code>users</code> • Active records persisted in SQLite</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
