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
            <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-surface-container-high border border-border-hairline text-xs font-mono text-on-surface-variant mb-6 shadow-xs">
              <span className="w-2 h-2 rounded-full bg-india-green"></span>
              <span>ConnectX C++ Backend • Phase 1 Active</span>
            </div>

            {/* Headline & Subtitle */}
            <h1 className="text-4xl sm:text-5xl lg:text-[56px] lg:leading-[1.15] text-primary tracking-tight font-bold max-w-3xl mb-5">
              Conversations, connected.
            </h1>
            <p className="text-base sm:text-lg text-on-surface-variant max-w-xl mb-8 leading-relaxed">
              A modern multi-user chat platform powered by a robust C++ OOP backend, SQLite persistence, and high-precision security.
            </p>

            {/* Actions */}
            <div className="flex flex-row items-center justify-center gap-4 mb-12">
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
            <div id="features" className="grid grid-cols-1 md:grid-cols-3 gap-6 w-full max-w-5xl text-left mt-4 mb-12">
              <div className="p-6 rounded-2xl bg-surface-container-lowest border border-border-hairline shadow-xs hover:shadow-sm transition-shadow">
                <div className="w-10 h-10 rounded-xl bg-primary-container text-white flex items-center justify-center mb-4">
                  <span className="material-symbols-outlined text-[20px]">terminal</span>
                </div>
                <h3 className="text-base font-semibold text-primary mb-1">C++ OOP Architecture</h3>
                <p className="text-xs text-on-surface-variant leading-relaxed">
                  Clean separation of concerns with Repositories, Services, Controllers, and RAII resource management.
                </p>
              </div>

              <div className="p-6 rounded-2xl bg-surface-container-lowest border border-border-hairline shadow-xs hover:shadow-sm transition-shadow">
                <div className="w-10 h-10 rounded-xl bg-surface-container-high text-primary flex items-center justify-center mb-4">
                  <span className="material-symbols-outlined text-[20px]">shield</span>
                </div>
                <h3 className="text-base font-semibold text-primary mb-1">Application Security</h3>
                <p className="text-xs text-on-surface-variant leading-relaxed">
                  PBKDF2-HMAC-SHA256 password hashing with random salt, HttpOnly session cookies, and SQL injection protection.
                </p>
              </div>

              <div className="p-6 rounded-2xl bg-surface-container-lowest border border-border-hairline shadow-xs hover:shadow-sm transition-shadow">
                <div className="w-10 h-10 rounded-xl bg-saffron/15 text-saffron flex items-center justify-center mb-4">
                  <span className="material-symbols-outlined text-[20px]">palette</span>
                </div>
                <h3 className="text-base font-semibold text-primary mb-1">Tricolour Design System</h3>
                <p className="text-xs text-on-surface-variant leading-relaxed">
                  Engineered with Ashoka Chakra Navy, surgical Saffron accents, and clean Indian tricolour telemetry.
                </p>
              </div>
            </div>
          </section>

          {/* Product Preview Section */}
          <section className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8 pb-20">
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
                      ConnectX Studio — Phase 1 Shell
                    </span>
                    <span className="w-1.5 h-1.5 rounded-full bg-india-green"></span>
                  </div>
                </div>
                <div className="text-xs text-on-surface-variant font-mono">
                  SQLite Database • C++ HTTP Server
                </div>
              </div>

              {/* Shell Preview Body */}
              <div className="p-8 sm:p-12 flex flex-col items-center justify-center text-center bg-gradient-to-b from-surface-container-lowest to-surface">
                <div className="w-16 h-16 rounded-2xl bg-primary text-white flex items-center justify-center font-bold text-2xl mb-4 shadow-sm">
                  C<span className="text-saffron">X</span>
                </div>
                <h2 className="text-xl font-bold text-primary mb-2">Welcome to ConnectX</h2>
                <p className="text-sm text-on-surface-variant max-w-md mb-6">
                  Phase 1 features full user registration, PBKDF2 password security, authenticated sessions, and responsive dashboard shells.
                </p>
                <Link
                  to="/signup"
                  className="px-6 py-2.5 rounded-xl bg-primary text-white hover:bg-primary-hover font-semibold text-sm transition-colors shadow-xs"
                >
                  Create an Account to Begin
                </Link>
              </div>
            </div>
          </section>
        </div>
      </main>

      {/* Footer */}
      <footer className="w-full py-6 border-t border-border-hairline bg-surface-container-low text-center text-xs text-on-surface-variant">
        <div className="max-w-6xl mx-auto px-4 flex flex-col sm:flex-row items-center justify-between gap-2">
          <div className="flex items-center gap-2">
            <span className="font-bold text-primary">ConnectX</span>
            <TricolourBadge size="sm" />
            <span>— C++ OOP College Project</span>
          </div>
          <div>Phase 1: Foundation & Authentication</div>
        </div>
      </footer>
    </div>
  );
}
