import React from 'react';
import { NavLink, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { TricolourBadge } from './TricolourBadge';

export function Sidebar() {
  const { user, logout } = useAuth();
  const navigate = useNavigate();

  const handleLogout = async () => {
    await logout();
    navigate('/login');
  };

  const userInitials = (user?.fullName || user?.username || 'U')
    .split(' ')
    .map((n) => n[0])
    .join('')
    .substring(0, 2)
    .toUpperCase();

  return (
    <aside className="fixed left-0 top-0 h-full w-[68px] bg-surface-container-lowest border-r border-border-hairline z-50 flex flex-col items-center justify-between py-5">
      {/* Top brand */}
      <div className="flex flex-col items-center w-full gap-6">
        <NavLink to="/dashboard" className="flex flex-col items-center gap-1.5 group" title="ConnectX Dashboard">
          <div className="w-10 h-10 rounded-xl bg-primary flex items-center justify-center text-white font-bold text-sm tracking-tight shadow-xs transition-transform group-hover:scale-105">
            C<span className="text-saffron">X</span>
          </div>
          <TricolourBadge size="sm" />
        </NavLink>

        {/* Navigation icons */}
        <nav className="flex flex-col items-center gap-3 w-full px-2">
          <NavLink
            to="/dashboard"
            title="Messages"
            className={({ isActive }) =>
              `relative w-11 h-11 flex items-center justify-center rounded-xl transition-all ${
                isActive
                  ? 'bg-surface-container-high text-primary font-semibold before:absolute before:left-0 before:top-2 before:bottom-2 before:w-[3px] before:rounded-r-full before:bg-saffron shadow-xs'
                  : 'text-on-surface-variant hover:bg-surface-container-high hover:text-on-surface'
              }`
            }
          >
            <span className="material-symbols-outlined text-[22px]">forum</span>
          </NavLink>

          <NavLink
            to="/find-people"
            title="Find People"
            className={({ isActive }) =>
              `relative w-11 h-11 flex items-center justify-center rounded-xl transition-all ${
                isActive
                  ? 'bg-surface-container-high text-primary font-semibold before:absolute before:left-0 before:top-2 before:bottom-2 before:w-[3px] before:rounded-r-full before:bg-saffron shadow-xs'
                  : 'text-on-surface-variant hover:bg-surface-container-high hover:text-on-surface'
              }`
            }
          >
            <span className="material-symbols-outlined text-[22px]">group_add</span>
          </NavLink>

          <NavLink
            to="/conversation"
            title="Conversation Shell"
            className={({ isActive }) =>
              `relative w-11 h-11 flex items-center justify-center rounded-xl transition-all ${
                isActive
                  ? 'bg-surface-container-high text-primary font-semibold before:absolute before:left-0 before:top-2 before:bottom-2 before:w-[3px] before:rounded-r-full before:bg-saffron shadow-xs'
                  : 'text-on-surface-variant hover:bg-surface-container-high hover:text-on-surface'
              }`
            }
          >
            <span className="material-symbols-outlined text-[22px]">chat</span>
          </NavLink>
        </nav>
      </div>

      {/* Bottom actions & user profile */}
      <div className="flex flex-col items-center gap-4 w-full px-2">
        <button
          onClick={handleLogout}
          type="button"
          title="Sign Out"
          className="w-11 h-11 flex items-center justify-center rounded-xl text-on-surface-variant hover:bg-red-50 hover:text-error transition-colors cursor-pointer"
        >
          <span className="material-symbols-outlined text-[20px]">logout</span>
        </button>

        <div className="relative group cursor-pointer" title={`${user?.fullName} (@${user?.username})`}>
          <div className="w-10 h-10 rounded-full bg-primary flex items-center justify-center text-white text-xs font-semibold shadow-xs">
            {userInitials}
          </div>
          <span
            className="absolute bottom-0 right-0 w-2.5 h-2.5 bg-india-green rounded-full ring-2 ring-surface-container-lowest"
            title="Connected"
          ></span>
        </div>
      </div>
    </aside>
  );
}
