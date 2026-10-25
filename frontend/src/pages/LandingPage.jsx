import React from 'react';
import { Link } from 'react-router-dom';
import { Navbar } from '../components/Navbar';
import { TricolourBadge } from '../components/TricolourBadge';
import { useAuth } from '../context/AuthContext';

export function LandingPage() {
  const { isAuthenticated } = useAuth();

  return (
    <div className="min-h-screen bg-surface flex flex-col">
      <Navbar />

      <main className="flex-1 pt-16">
        {/* Ambient Backdrops */}
        <div className="relative w-full overflow-hidden">
          <div className="absolute -top-32 left-1/2 -translate-x-1/2 w-[780px] h-[480px] bg-gradient-to-b from-secondary/10 via-surface-container/60 to-transparent blur-3xl pointer-events-none -z-10 rounded-full"></div>
          <div className="absolute top-[420px] -right-32 w-80 h-80 bg-saffron/10 blur-3xl pointer-events-none -z-10 rounded-full"></div>

          {/* Hero Section */}
          <section className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8 pt-16 pb-12 flex flex-col items-center text-center">
            {/* Status Pill */}
            <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-surface-container-high border border-border-hairline text-xs font-medium text-on-surface-variant mb-6 shadow-xs">
              <span className="w-2 h-2 rounded-full bg-india-green"></span>
              <span>A private, focused space for conversations</span>
            </div>

            {/* Headline & Subtitle */}
            <h1 className="text-4xl sm:text-5xl lg:text-[56px] lg:leading-[1.15] text-primary tracking-tight font-bold max-w-3xl mb-5">
              Conversations, connected.
            </h1>
            <p className="text-base sm:text-lg text-on-surface-variant max-w-xl mb-8 leading-relaxed">
              A simple, personal space to connect and chat. Designed with clarity, quiet focus, and modern elegance.
            </p>

            {/* Actions */}
            <div className="flex flex-row items-center justify-center gap-4 mb-14">
              {isAuthenticated ? (
                <Link
                  to="/dashboard"
                  className="inline-flex items-center justify-center px-8 py-3.5 rounded-xl bg-primary hover:bg-primary-hover text-white font-semibold shadow-sm transition-all duration-150"
                >
                  Open Dashboard
                </Link>
              ) : (
                <>
                  <Link
                    to="/signup"
                    className="inline-flex items-center justify-center px-8 py-3.5 rounded-xl bg-saffron hover:bg-saffron-hover text-white font-semibold shadow-sm transition-all duration-150"
                  >
                    Get Started
                  </Link>
                  <Link
                    to="/login"
                    className="inline-flex items-center justify-center px-8 py-3.5 rounded-xl bg-surface-container-lowest text-primary hover:bg-surface-container-high border border-border-hairline font-semibold shadow-sm transition-all duration-150"
                  >
                    Log In
                  </Link>
                </>
              )}
            </div>

            {/* Value Pillars */}
            <div id="features" className="grid grid-cols-1 md:grid-cols-3 gap-6 w-full max-w-5xl text-left mt-2 mb-14">
              <div className="p-6 rounded-2xl bg-surface-container-lowest border border-border-hairline shadow-xs hover:shadow-sm transition-shadow">
                <div className="w-10 h-10 rounded-xl bg-primary-container text-white flex items-center justify-center mb-4">
                  <span className="material-symbols-outlined text-[20px]">chat</span>
                </div>
                <h3 className="text-base font-semibold text-primary mb-1">Private Conversations</h3>
                <p className="text-xs text-on-surface-variant leading-relaxed">
                  Direct one-to-one messaging focused on privacy, simplicity, and personal connection.
                </p>
              </div>

              <div className="p-6 rounded-2xl bg-surface-container-lowest border border-border-hairline shadow-xs hover:shadow-sm transition-shadow">
                <div className="w-10 h-10 rounded-xl bg-surface-container-high text-primary flex items-center justify-center mb-4">
                  <span className="material-symbols-outlined text-[20px]">palette</span>
                </div>
                <h3 className="text-base font-semibold text-primary mb-1">Thoughtful Interface</h3>
                <p className="text-xs text-on-surface-variant leading-relaxed">
                  A minimalist, distraction-free environment engineered with calm typography and restrained accents.
                </p>
              </div>

              <div className="p-6 rounded-2xl bg-surface-container-lowest border border-border-hairline shadow-xs hover:shadow-sm transition-shadow">
                <div className="w-10 h-10 rounded-xl bg-saffron/15 text-saffron flex items-center justify-center mb-4">
                  <span className="material-symbols-outlined text-[20px]">devices</span>
                </div>
                <h3 className="text-base font-semibold text-primary mb-1">Everywhere You Are</h3>
                <p className="text-xs text-on-surface-variant leading-relaxed">
                  Seamlessly responsive across desktop, tablet, and mobile browsers with consistent state.
                </p>
              </div>
            </div>
          </section>

          {/* Product Preview Section (Approved Stitch Desktop Preview) */}
          <section id="preview" className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8 pb-20">
            <div className="relative w-full rounded-2xl bg-surface-container-lowest border border-border-hairline shadow-xl overflow-hidden">
              {/* Tricolour Accent Strip */}
              <div className="w-full flex h-[3px]">
                <span className="flex-1 bg-saffron"></span>
                <span className="flex-1 bg-surface-container-lowest"></span>
                <span className="flex-1 bg-india-green"></span>
              </div>

              {/* Window Controls Header */}
              <div className="px-6 py-3 bg-surface-container-low border-b border-border-hairline flex items-center justify-between">
                <div className="flex items-center gap-3">
                  <div className="flex items-center gap-1.5">
                    <span className="w-3 h-3 rounded-full bg-surface-container-highest"></span>
                    <span className="w-3 h-3 rounded-full bg-surface-container-highest"></span>
                    <span className="w-3 h-3 rounded-full bg-surface-container-highest"></span>
                  </div>
                  <div className="h-4 w-px bg-border-hairline ml-1"></div>
                  <div className="flex items-center gap-2">
                    <span className="text-xs font-semibold uppercase tracking-wider text-on-surface-variant font-mono">
                      ConnectX Desktop
                    </span>
                    <span className="w-1.5 h-1.5 rounded-full bg-india-green"></span>
                  </div>
                </div>
                <div className="text-xs text-on-surface-variant font-medium">
                  Direct Messaging Preview
                </div>
              </div>

              {/* Shell Preview Body: 2-Column Chat Simulation from Stitch */}
              <div className="grid grid-cols-12 min-h-[460px]">
                {/* Left Column: Sample Conversations List (4 Cols) */}
                <div className="col-span-12 md:col-span-4 bg-surface-container-low/60 border-r border-border-hairline p-4 flex flex-col justify-between">
                  <div className="flex flex-col gap-2">
                    {/* Search bar inside pane */}
                    <div className="px-3 py-2 bg-surface-container-lowest rounded-xl mb-1 flex items-center gap-2 text-on-surface-variant border border-border-hairline shadow-xs">
                      <span className="material-symbols-outlined text-[16px]">search</span>
                      <span className="text-xs text-on-surface-variant/70">Jump to conversation...</span>
                    </div>

                    {/* Contact Item: Active (Arun Kumar) */}
                    <div className="p-3 rounded-xl bg-surface-container-lowest border border-border-hairline shadow-xs flex items-center justify-between">
                      <div className="flex items-center gap-3">
                        <div className="relative w-9 h-9 rounded-full bg-primary-container flex items-center justify-center text-white font-semibold text-xs">
                          AK
                          <span className="absolute bottom-0 right-0 w-2.5 h-2.5 rounded-full bg-india-green ring-2 ring-white"></span>
                        </div>
                        <div className="flex flex-col">
                          <span className="text-xs font-bold text-on-surface leading-tight">Arun Kumar</span>
                          <span className="text-[11px] text-on-surface-variant truncate w-32">Ready for the review sync?</span>
                        </div>
                      </div>
                      <span className="text-[10px] text-on-surface-variant/80 font-mono">14:28</span>
                    </div>

                    {/* Contact Item 2 (Priya Sharma) */}
                    <div className="p-3 rounded-xl bg-surface-container-low/80 flex items-center justify-between">
                      <div className="flex items-center gap-3">
                        <div className="relative w-9 h-9 rounded-full bg-surface-container-high flex items-center justify-center text-on-surface font-semibold text-xs">
                          PS
                          <span className="absolute bottom-0 right-0 w-2.5 h-2.5 rounded-full bg-outline-variant ring-2 ring-white"></span>
                        </div>
                        <div className="flex flex-col">
                          <span className="text-xs font-semibold text-on-surface leading-tight">Priya Sharma</span>
                          <span className="text-[11px] text-on-surface-variant truncate w-32">Updated the shared draft.</span>
                        </div>
                      </div>
                      <span className="text-[10px] text-on-surface-variant/80 font-mono">11:15</span>
                    </div>

                    {/* Contact Item 3 (Rohan Nair) */}
                    <div className="p-3 rounded-xl bg-surface-container-low/80 flex items-center justify-between">
                      <div className="flex items-center gap-3">
                        <div className="relative w-9 h-9 rounded-full bg-surface-container-high flex items-center justify-center text-on-surface font-semibold text-xs">
                          RN
                          <span className="absolute bottom-0 right-0 w-2.5 h-2.5 rounded-full bg-saffron ring-2 ring-white"></span>
                        </div>
                        <div className="flex flex-col">
                          <span className="text-xs font-semibold text-on-surface leading-tight">Rohan Nair</span>
                          <span className="text-[11px] text-on-surface-variant truncate w-32">See you at the meeting!</span>
                        </div>
                      </div>
                      <span className="text-[10px] text-on-surface-variant/80 font-mono">Yesterday</span>
                    </div>
                  </div>

                  {/* Profile Summary strip at bottom */}
                  <div className="p-2.5 rounded-xl bg-surface-container-lowest border border-border-hairline flex items-center justify-between mt-3">
                    <div className="flex items-center gap-2">
                      <div className="w-7 h-7 rounded-full bg-primary flex items-center justify-center text-white text-[10px] font-bold">
                        ME
                      </div>
                      <span className="text-xs font-semibold text-on-surface">Online</span>
                    </div>
                    <span className="material-symbols-outlined text-[16px] text-on-surface-variant">tune</span>
                  </div>
                </div>

                {/* Right Column: Active Conversation Stream (8 Cols) */}
                <div className="col-span-12 md:col-span-8 bg-surface-container-lowest flex flex-col justify-between">
                  {/* Pinned Contact Header */}
                  <div className="px-6 py-3.5 bg-surface-container-lowest border-b border-border-hairline flex items-center justify-between shadow-xs">
                    <div className="flex items-center gap-3">
                      <div className="relative w-9 h-9 rounded-full bg-primary-container flex items-center justify-center text-white font-semibold text-xs">
                        AK
                        <span className="absolute bottom-0 right-0 w-2.5 h-2.5 rounded-full bg-india-green ring-2 ring-white"></span>
                      </div>
                      <div className="flex flex-col">
                        <span className="text-sm font-bold text-on-surface">Arun Kumar</span>
                        <div className="flex items-center gap-1.5">
                          <span className="w-1.5 h-1.5 rounded-full bg-india-green"></span>
                          <span className="text-[11px] text-on-surface-variant">Active now</span>
                        </div>
                      </div>
                    </div>
                    <div className="flex items-center gap-2 text-on-surface-variant">
                      <button type="button" className="w-8 h-8 rounded-lg hover:bg-surface-container-low flex items-center justify-center transition-colors">
                        <span className="material-symbols-outlined text-[18px]">search</span>
                      </button>
                      <button type="button" className="w-8 h-8 rounded-lg hover:bg-surface-container-low flex items-center justify-center transition-colors">
                        <span className="material-symbols-outlined text-[18px]">more_vert</span>
                      </button>
                    </div>
                  </div>

                  {/* Message Stream */}
                  <div className="p-6 flex flex-col gap-4 flex-1 justify-center">
                    {/* Peer Incoming */}
                    <div className="flex items-end gap-2.5 max-w-md">
                      <div className="w-7 h-7 rounded-full bg-primary-container flex items-center justify-center text-white text-[10px] font-bold mb-1">
                        AK
                      </div>
                      <div className="flex flex-col gap-1 items-start">
                        <div className="p-3.5 rounded-2xl rounded-bl-xs bg-surface-container text-on-surface text-xs leading-relaxed">
                          Hey! I’ve reviewed the outline. The layout looks sharp, focused, and intuitive.
                        </div>
                        <span className="text-[10px] text-on-surface-variant/80 ml-1 font-mono">14:26</span>
                      </div>
                    </div>

                    {/* Self Outgoing (Ashoka Navy) */}
                    <div className="flex items-end gap-2.5 max-w-md self-end justify-end">
                      <div className="flex flex-col gap-1 items-end">
                        <div className="p-3.5 rounded-2xl rounded-br-xs bg-primary text-white text-xs leading-relaxed shadow-xs">
                          Thanks! Just launched the new interface. Clean, reliable, and completely personal.
                        </div>
                        <div className="flex items-center gap-1 mr-1">
                          <span className="text-[10px] text-on-surface-variant/80 font-mono">14:28</span>
                          <span className="material-symbols-outlined text-[14px] text-secondary">done_all</span>
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Composer Simulation */}
                  <div className="p-4 bg-surface-container-lowest border-t border-border-hairline">
                    <div className="flex items-center gap-2 p-1.5 rounded-xl bg-surface-container-low border border-border-hairline">
                      <input
                        disabled
                        readOnly
                        className="flex-1 bg-transparent border-0 outline-none text-xs text-on-surface placeholder:text-on-surface-variant/60 px-2"
                        placeholder="Type a message..."
                        value="Sounds good! Let's get started."
                      />
                      <button
                        type="button"
                        className="h-8 px-3 rounded-lg bg-saffron text-white flex items-center justify-center gap-1"
                      >
                        <span className="material-symbols-outlined text-[16px]">send</span>
                      </button>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </section>
        </div>
      </main>

      {/* Clean Product Footer */}
      <footer className="w-full py-6 border-t border-border-hairline bg-surface-container-low text-xs text-on-surface-variant">
        <div className="max-w-6xl mx-auto px-4 flex flex-col sm:flex-row items-center justify-between gap-2">
          <div className="flex items-center gap-2">
            <span className="font-bold text-primary">ConnectX</span>
            <TricolourBadge size="sm" />
            <span className="text-outline">·</span>
            <span>Minimalist network chat</span>
          </div>
          <div>© 2026 ConnectX Platform. All rights reserved.</div>
        </div>
      </footer>
    </div>
  );
}
