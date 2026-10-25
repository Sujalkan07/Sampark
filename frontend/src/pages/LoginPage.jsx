import React, { useState } from 'react';
import { Link, useNavigate, useLocation } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { Navbar } from '../components/Navbar';
import { TricolourBadge } from '../components/TricolourBadge';
import { AlertBanner } from '../components/AlertBanner';

export function LoginPage() {
  const [identifier, setIdentifier] = useState('');
  const [password, setPassword] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [rememberMe, setRememberMe] = useState(true);
  const [loading, setLoading] = useState(false);
  const [errorMessage, setErrorMessage] = useState('');

  const { login } = useAuth();
  const navigate = useNavigate();
  const location = useLocation();

  const from = location.state?.from?.pathname || '/dashboard';

  const handleSubmit = async (e) => {
    e.preventDefault();
    setErrorMessage('');

    if (!identifier.trim() || !password) {
      setErrorMessage('Please enter both username/email and password.');
      return;
    }

    try {
      setLoading(true);
      await login(identifier.trim(), password);
      navigate(from, { replace: true });
    } catch (err) {
      setErrorMessage(err.message || 'Login failed. Please check your credentials.');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-surface flex flex-col">
      <Navbar />

      <main className="flex-1 pt-16 flex items-center justify-center p-4 sm:p-6 lg:p-8 relative overflow-hidden">
        {/* Subtle Ambient Circles */}
        <div className="absolute -top-32 -left-28 w-96 h-96 rounded-full bg-secondary/5 blur-3xl pointer-events-none"></div>
        <div className="absolute -bottom-36 -right-20 w-[420px] h-[420px] rounded-full bg-saffron/5 blur-3xl pointer-events-none"></div>

        {/* Authentication Card */}
        <div className="relative w-full max-w-[440px] z-10 my-8">
          <div className="bg-surface-container-lowest rounded-2xl border border-border-hairline shadow-lg p-6 sm:p-8 flex flex-col">
            {/* Top Brand Emblem */}
            <div className="flex items-center justify-between pb-6 border-b border-border-hairline mb-6">
              <div className="flex items-center gap-3">
                <img
                  src="/sampark-logo.png"
                  alt="Sampark"
                  className="h-9 w-auto object-contain"
                />
              </div>

              <div className="inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full bg-surface-container-low border border-border-hairline text-xs font-mono text-on-surface-variant">
                <span className="w-1.5 h-1.5 rounded-full bg-india-green"></span>
                <span>Active</span>
              </div>
            </div>

            {/* Header */}
            <div className="mb-6">
              <h1 className="text-2xl font-bold text-primary tracking-tight">Welcome back</h1>
              <p className="text-sm text-on-surface-variant mt-1">
                Log in to continue your conversations.
              </p>
            </div>

            {/* Error Banner */}
            {errorMessage && (
              <div className="mb-5">
                <AlertBanner
                  type="error"
                  message={errorMessage}
                  onClose={() => setErrorMessage('')}
                />
              </div>
            )}

            {/* Form */}
            <form onSubmit={handleSubmit} className="flex flex-col gap-4">
              {/* Field 1: Identifier */}
              <div className="flex flex-col gap-1.5">
                <label className="text-xs font-semibold text-on-surface" htmlFor="identifier">
                  Username or Email
                </label>
                <div className="relative flex items-center">
                  <span className="material-symbols-outlined absolute left-3 text-on-surface-variant text-[18px] pointer-events-none">
                    alternate_email
                  </span>
                  <input
                    id="identifier"
                    type="text"
                    required
                    value={identifier}
                    onChange={(e) => setIdentifier(e.target.value)}
                    placeholder="Enter your username or email"
                    disabled={loading}
                    className="w-full bg-surface-container-low hover:bg-surface-container focus:bg-surface-container-lowest text-on-surface text-sm pl-10 pr-3 py-2.5 rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all placeholder:text-outline/70"
                  />
                </div>
              </div>

              {/* Field 2: Password */}
              <div className="flex flex-col gap-1.5">
                <label className="text-xs font-semibold text-on-surface" htmlFor="password">
                  Password
                </label>
                <div className="relative flex items-center">
                  <span className="material-symbols-outlined absolute left-3 text-on-surface-variant text-[18px] pointer-events-none">
                    lock
                  </span>
                  <input
                    id="password"
                    type={showPassword ? 'text' : 'password'}
                    required
                    value={password}
                    onChange={(e) => setPassword(e.target.value)}
                    placeholder="Enter your password"
                    disabled={loading}
                    className="w-full bg-surface-container-low hover:bg-surface-container focus:bg-surface-container-lowest text-on-surface text-sm pl-10 pr-10 py-2.5 rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all placeholder:text-outline/70"
                  />
                  <button
                    type="button"
                    onClick={() => setShowPassword(!showPassword)}
                    className="absolute right-2.5 p-1 rounded-md text-on-surface-variant hover:text-on-surface focus:outline-none"
                    aria-label={showPassword ? 'Hide password' : 'Show password'}
                  >
                    <span className="material-symbols-outlined text-[18px]">
                      {showPassword ? 'visibility_off' : 'visibility'}
                    </span>
                  </button>
                </div>
              </div>

              {/* Remember Me */}
              <div className="flex items-center justify-between pt-1">
                <label className="flex items-center gap-2 cursor-pointer select-none">
                  <input
                    type="checkbox"
                    checked={rememberMe}
                    onChange={(e) => setRememberMe(e.target.checked)}
                    className="w-4 h-4 rounded text-primary accent-primary cursor-pointer"
                  />
                  <span className="text-xs text-on-surface-variant hover:text-on-surface transition-colors">
                    Remember me
                  </span>
                </label>
              </div>

              {/* Submit Button */}
              <div className="pt-2">
                <button
                  type="submit"
                  disabled={loading}
                  className="w-full h-11 bg-primary hover:bg-primary-hover active:scale-[0.99] text-white font-semibold text-sm rounded-xl shadow-sm transition-all flex items-center justify-center gap-2 cursor-pointer disabled:opacity-70 disabled:cursor-not-allowed"
                >
                  {loading ? (
                    <>
                      <div className="w-4 h-4 border-2 border-white border-t-transparent rounded-full animate-spin"></div>
                      <span>Authenticating...</span>
                    </>
                  ) : (
                    <>
                      <span>Log In</span>
                      <span className="material-symbols-outlined text-[18px]">arrow_forward</span>
                    </>
                  )}
                </button>
              </div>
            </form>

            {/* Bottom link to Register */}
            <div className="mt-6 pt-5 border-t border-border-hairline text-center text-xs text-on-surface-variant">
              Don't have an account yet?{' '}
              <Link to="/signup" className="font-semibold text-primary hover:underline">
                Create Account
              </Link>
            </div>
          </div>
        </div>
      </main>
    </div>
  );
}
