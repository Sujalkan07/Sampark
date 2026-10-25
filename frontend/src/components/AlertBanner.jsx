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
          ? 'bg-red-50 border-red-200 text-error'
          : isSuccess
          ? 'bg-green-50 border-green-200 text-india-green'
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
