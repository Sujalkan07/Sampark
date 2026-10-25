import React from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { TricolourBadge } from './TricolourBadge';

export function Navbar() {
  const { user, isAuthenticated } = useAuth();
  const navigate = useNavigate();

  return (
    <header className="fixed top-0 w-full z-50 bg-surface/90 backdrop-blur-xl border-b border-border-hairline shadow-xs">
      <div className="h-16 max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 flex items-center justify-between">
        {/* Brand */}
        <Link to="/" className="flex items-center gap-2.5 group">
          <div className="w-8 h-8 rounded-lg bg-primary-container flex items-center justify-center text-white font-bold text-sm tracking-tight shadow-xs">
            C
          </div>
          <span className="font-bold text-lg text-primary tracking-tight">ConnectX</span>
          <TricolourBadge />
        </Link>

        {/* Public Nav items */}
        <nav className="hidden md:flex items-center gap-1">
          <Link
            to="/"
            className="px-3 py-1.5 rounded-lg text-sm text-on-surface-variant hover:text-on-surface hover:bg-surface-container-high transition-colors font-medium"
          >
            Overview
          </Link>
          <a
            href="#features"
            className="px-3 py-1.5 rounded-lg text-sm text-on-surface-variant hover:text-on-surface hover:bg-surface-container-high transition-colors font-medium"
          >
            Features
          </a>
          <a
            href="#tech"
            className="px-3 py-1.5 rounded-lg text-sm text-on-surface-variant hover:text-on-surface hover:bg-surface-container-high transition-colors font-medium"
          >
            C++ Architecture
          </a>
        </nav>

        {/* Right CTA */}
        <div className="flex items-center gap-3">
          {isAuthenticated ? (
            <div className="flex items-center gap-3">
              <span className="text-sm font-medium text-on-surface hidden sm:inline">
                {user?.fullName || user?.username}
              </span>
              <button
                onClick={() => navigate('/dashboard')}
                className="px-4 py-2 text-xs font-semibold rounded-lg bg-primary text-white hover:bg-primary-hover transition-colors shadow-xs"
              >
                Go to Dashboard
              </button>
            </div>
          ) : (
            <>
              <Link
                to="/login"
                className="px-3 py-1.5 text-xs sm:text-sm font-medium text-on-surface-variant hover:text-on-surface hover:bg-surface-container-high rounded-lg transition-colors"
              >
                Log In
              </Link>
              <Link
                to="/signup"
                className="px-3.5 py-1.5 text-xs sm:text-sm font-semibold text-white bg-saffron hover:bg-saffron-hover rounded-lg transition-colors shadow-xs"
              >
                Create Account
              </Link>
            </>
          )}
        </div>
      </div>
    </header>
  );
}
