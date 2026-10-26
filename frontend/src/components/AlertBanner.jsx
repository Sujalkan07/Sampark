import React from 'react';

export function AlertBanner({ type = 'error', message, onClose }) {
  if (!message) return null;

  const isError = type === 'error';
  const isSuccess = type === 'success';

  return (
    <div
      role="alert"
      className={`p-3 rounded-lg border text-sm flex items-start gap-2.5 transition-all ${
        isError
          ? 'bg-red-500/10 border-red-500/25 text-red-600 dark:text-red-400'
          : isSuccess
          ? 'bg-emerald-500/10 border-emerald-500/25 text-india-green dark:text-emerald-400'
          : 'bg-surface-container-high border-border-hairline text-on-surface'
      }`}
    >
      <span className="material-symbols-outlined text-[18px] shrink-0 mt-0.5">
        {isError ? 'error' : isSuccess ? 'check_circle' : 'info'}
      </span>
      <div className="flex-1 font-medium leading-snug">{message}</div>
      {onClose && (
        <button
          onClick={onClose}
          type="button"
          aria-label="Dismiss alert"
          className="text-on-surface-variant hover:text-on-surface transition-colors"
        >
          <span className="material-symbols-outlined text-[16px]">close</span>
        </button>
      )}
    </div>
  );
}
