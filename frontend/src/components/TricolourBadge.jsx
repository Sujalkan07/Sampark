import React from 'react';

export function TricolourBadge({ size = 'md' }) {
  const isSmall = size === 'sm';
  return (
    <div
      aria-hidden="true"
      className={`flex flex-col gap-[2px] ${isSmall ? 'w-3.5 py-0.5' : 'w-4 py-1'}`}
      title="Indian Tricolour Accent"
    >
      <span className="h-[2px] w-full rounded-full bg-saffron"></span>
      <span className="h-[2px] w-full rounded-full bg-surface-container-high border-t border-black/5 shadow-xs"></span>
      <span className="h-[2px] w-full rounded-full bg-india-green"></span>
    </div>
  );
}
