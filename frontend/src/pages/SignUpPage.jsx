import React, { useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { Navbar } from '../components/Navbar';
import { TricolourBadge } from '../components/TricolourBadge';
import { AlertBanner } from '../components/AlertBanner';

export function SignUpPage() {
  const [fullName, setFullName] = useState('');
  const [username, setUsername] = useState('');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [showConfirmPassword, setShowConfirmPassword] = useState(false);
  const [agreedToTerms, setAgreedToTerms] = useState(true);

  const [loading, setLoading] = useState(false);
  const [errorMessage, setErrorMessage] = useState('');

  const { register } = useAuth();
  const navigate = useNavigate();

  // Password strength calculation
  const getPasswordStrength = () => {
    if (!password) return 0;
    let strength = 0;
    if (password.length >= 8) strength += 1;
    if (/[A-Z]/.test(password) && /[a-z]/.test(password)) strength += 1;
    if (/[0-9]/.test(password)) strength += 1;
    if (/[^A-Za-z0-9]/.test(password)) strength += 1;
    return strength; // 0 to 4
  };

  const strengthScore = getPasswordStrength();
  const passwordsMatch = password.length > 0 && confirmPassword.length > 0 && password === confirmPassword;

  const handleSubmit = async (e) => {
    e.preventDefault();
    setErrorMessage('');

    if (!fullName.trim()) {
      setErrorMessage('Full name is required.');
      return;
    }

    if (username.trim().length < 3) {
      setErrorMessage('Username must be at least 3 characters.');
      return;
    }

    if (password.length < 8) {
      setErrorMessage('Password must be at least 8 characters.');
      return;
    }

    if (password !== confirmPassword) {
      setErrorMessage('Passwords do not match. Please verify.');
      return;
    }

    if (!agreedToTerms) {
      setErrorMessage('Please accept the Community Guidelines to continue.');
      return;
    }

    try {
      setLoading(true);
      await register({
        fullName: fullName.trim(),
        username: username.trim(),
        email: email.trim(),
        password,
      });
      navigate('/dashboard', { replace: true });
    } catch (err) {
      setErrorMessage(err.message || 'Registration failed. Please check your information.');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-surface flex flex-col">
      <Navbar />

      <main className="flex-1 pt-16 flex items-center justify-center p-4 sm:p-6 lg:p-8 relative overflow-hidden">
        {/* Ambient architectural lighting background */}
        <div className="absolute -top-32 left-1/2 -translate-x-1/2 w-[680px] h-[360px] bg-gradient-to-b from-secondary/10 via-surface-container to-transparent rounded-full blur-3xl pointer-events-none"></div>
        <div className="absolute bottom-10 right-1/4 w-72 h-72 bg-india-green/5 rounded-full blur-2xl pointer-events-none"></div>
        <div className="absolute top-1/3 left-1/4 w-80 h-80 bg-saffron/5 rounded-full blur-2xl pointer-events-none"></div>

        {/* Central Registration Card */}
        <div className="relative w-full max-w-[460px] bg-surface-container-lowest rounded-2xl border border-border-hairline shadow-lg p-6 sm:p-8 z-10 my-8">
          {/* Header */}
          <div className="flex flex-col items-center text-center mb-6">
            <div className="inline-flex items-center gap-2.5 mb-2">
              <span className="text-2xl font-bold text-primary tracking-tight">ConnectX</span>
              <TricolourBadge />
            </div>
            <h1 className="text-xl font-bold text-on-surface tracking-tight">Create your account</h1>
            <p className="text-xs text-on-surface-variant mt-1">
              Join ConnectX and start a conversation.
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
            {/* Field 1: Full Name */}
            <div className="flex flex-col gap-1">
              <label className="text-xs font-semibold text-on-surface" htmlFor="fullName">
                Full Name
              </label>
              <input
                id="fullName"
                type="text"
                required
                value={fullName}
                onChange={(e) => setFullName(e.target.value)}
                placeholder="Aarav Sharma"
                disabled={loading}
                className="w-full bg-surface-container-low hover:bg-surface-container focus:bg-surface-container-lowest text-on-surface text-sm px-3.5 py-2.5 rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all placeholder:text-outline/70"
              />
            </div>

            {/* Field 2: Username */}
            <div className="flex flex-col gap-1">
              <div className="flex items-center justify-between">
                <label className="text-xs font-semibold text-on-surface" htmlFor="username">
                  Username
                </label>
                {username.trim().length >= 3 && (
                  <span className="text-[11px] font-mono text-india-green flex items-center gap-0.5">
                    <span className="material-symbols-outlined text-[14px]">check_circle</span> Valid handle
                  </span>
                )}
              </div>
              <div className="relative flex items-center">
                <div className="absolute left-3.5 text-xs font-mono text-outline pointer-events-none">@</div>
                <input
                  id="username"
                  type="text"
                  required
                  value={username}
                  onChange={(e) => setUsername(e.target.value)}
                  placeholder="aarav_sharma"
                  disabled={loading}
                  className="w-full bg-surface-container-low hover:bg-surface-container focus:bg-surface-container-lowest text-on-surface text-sm pl-8 pr-3.5 py-2.5 rounded-xl border border-border-hairline focus:border-secondary focus:outline-none font-mono transition-all placeholder:text-outline/70"
                />
              </div>
            </div>

            {/* Field 3: Email */}
            <div className="flex flex-col gap-1">
              <label className="text-xs font-semibold text-on-surface" htmlFor="email">
                Email Address
              </label>
              <div className="relative flex items-center">
                <span className="material-symbols-outlined absolute left-3 text-outline text-[16px] pointer-events-none">
                  alternate_email
                </span>
                <input
                  id="email"
                  type="email"
                  required
                  value={email}
                  onChange={(e) => setEmail(e.target.value)}
                  placeholder="aarav@example.com"
                  disabled={loading}
                  className="w-full bg-surface-container-low hover:bg-surface-container focus:bg-surface-container-lowest text-on-surface text-sm pl-9 pr-3.5 py-2.5 rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all placeholder:text-outline/70"
                />
              </div>
            </div>

            {/* Field 4: Password */}
            <div className="flex flex-col gap-1">
              <label className="text-xs font-semibold text-on-surface" htmlFor="password">
                Password
              </label>
              <div className="relative flex items-center">
                <input
                  id="password"
                  type={showPassword ? 'text' : 'password'}
                  required
                  value={password}
                  onChange={(e) => setPassword(e.target.value)}
                  placeholder="Minimum 8 characters"
                  disabled={loading}
                  className="w-full bg-surface-container-low hover:bg-surface-container focus:bg-surface-container-lowest text-on-surface text-sm pl-3.5 pr-10 py-2.5 rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all placeholder:text-outline/70"
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

              {/* Password Strength Indicator */}
              {password && (
                <div className="flex items-center gap-1.5 pt-1">
                  <div
                    className={`h-1 flex-1 rounded-full transition-colors ${
                      strengthScore >= 1 ? 'bg-india-green' : 'bg-surface-container-highest'
                    }`}
                  ></div>
                  <div
                    className={`h-1 flex-1 rounded-full transition-colors ${
                      strengthScore >= 2 ? 'bg-india-green' : 'bg-surface-container-highest'
                    }`}
                  ></div>
                  <div
                    className={`h-1 flex-1 rounded-full transition-colors ${
                      strengthScore >= 3 ? 'bg-india-green' : 'bg-surface-container-highest'
                    }`}
                  ></div>
                  <div
                    className={`h-1 flex-1 rounded-full transition-colors ${
                      strengthScore >= 4 ? 'bg-india-green' : 'bg-surface-container-highest'
                    }`}
                  ></div>
                  <span className="text-[10px] font-mono text-on-surface-variant pl-1">
                    {strengthScore <= 1 ? 'Weak' : strengthScore <= 3 ? 'Medium' : 'Strong'}
                  </span>
                </div>
              )}
            </div>

            {/* Field 5: Confirm Password */}
            <div className="flex flex-col gap-1">
              <div className="flex items-center justify-between">
                <label className="text-xs font-semibold text-on-surface" htmlFor="confirmPassword">
                  Confirm Password
                </label>
                {confirmPassword && (
                  <span
                    className={`text-[10px] font-mono ${
                      passwordsMatch ? 'text-india-green' : 'text-error'
                    }`}
                  >
                    {passwordsMatch ? 'Passwords match' : 'Passwords do not match'}
                  </span>
                )}
              </div>
              <div className="relative flex items-center">
                <input
                  id="confirmPassword"
                  type={showConfirmPassword ? 'text' : 'password'}
                  required
                  value={confirmPassword}
                  onChange={(e) => setConfirmPassword(e.target.value)}
                  placeholder="Re-enter password"
                  disabled={loading}
                  className="w-full bg-surface-container-low hover:bg-surface-container focus:bg-surface-container-lowest text-on-surface text-sm pl-3.5 pr-10 py-2.5 rounded-xl border border-border-hairline focus:border-secondary focus:outline-none transition-all placeholder:text-outline/70"
                />
                <button
                  type="button"
                  onClick={() => setShowConfirmPassword(!showConfirmPassword)}
                  className="absolute right-2.5 p-1 rounded-md text-on-surface-variant hover:text-on-surface focus:outline-none"
                  aria-label={showConfirmPassword ? 'Hide confirm password' : 'Show confirm password'}
                >
                  <span className="material-symbols-outlined text-[18px]">
                    {showConfirmPassword ? 'visibility_off' : 'visibility'}
                  </span>
                </button>
              </div>
            </div>

            {/* Terms Agreement */}
            <div className="flex items-start gap-2 pt-1">
              <input
                id="terms"
                type="checkbox"
                checked={agreedToTerms}
                onChange={(e) => setAgreedToTerms(e.target.checked)}
                className="mt-0.5 w-4 h-4 rounded text-primary accent-primary cursor-pointer"
              />
              <label htmlFor="terms" className="text-xs text-on-surface-variant cursor-pointer select-none">
                I agree to the Community Guidelines and responsible platform usage.
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
                    <span>Creating account...</span>
                  </>
                ) : (
                  <>
                    <span>Create Account</span>
                    <span className="material-symbols-outlined text-[18px]">arrow_forward</span>
                  </>
                )}
              </button>
            </div>
          </form>

          {/* Bottom link to Login */}
          <div className="mt-6 pt-5 border-t border-border-hairline text-center text-xs text-on-surface-variant">
            Already have an account?{' '}
            <Link to="/login" className="font-semibold text-primary hover:underline">
              Log In
            </Link>
          </div>
        </div>
      </main>
    </div>
  );
}
