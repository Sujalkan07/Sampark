import React, { useState, useEffect, useRef, useMemo } from 'react';

const EMOJI_CATEGORIES = [
  {
    id: 'smileys',
    name: 'Smileys',
    icon: 'sentiment_satisfied',
    emojis: [
      { char: '😀', name: 'grinning' },
      { char: '😃', name: 'smiling with big eyes' },
      { char: '😄', name: 'grinning with smiling eyes' },
      { char: '😁', name: 'beaming' },
      { char: '😆', name: 'laughing' },
      { char: '😅', name: 'sweat smile' },
      { char: '🤣', name: 'rofl' },
      { char: '😂', name: 'joy' },
      { char: '🙂', name: 'slightly smiling' },
      { char: '🙃', name: 'upside down' },
      { char: '😉', name: 'wink' },
      { char: '😊', name: 'blush' },
      { char: '😇', name: 'innocent' },
      { char: '🥰', name: 'smiling with hearts' },
      { char: '😍', name: 'heart eyes' },
      { char: '🤩', name: 'star struck' },
      { char: '😘', name: 'kissing heart' },
      { char: '😋', name: 'yum' },
      { char: '😛', name: 'tongue' },
      { char: '😜', name: 'winking tongue' },
      { char: '🤪', name: 'zany' },
      { char: '😝', name: 'squinting tongue' },
      { char: '🤗', name: 'hugging' },
      { char: '🤭', name: 'hand over mouth' },
      { char: '🤫', name: 'shushing' },
      { char: '🤔', name: 'thinking' },
      { char: '🫡', name: 'saluting' },
      { char: '🤐', name: 'zipper' },
      { char: '🤨', name: 'raised eyebrow' },
      { char: '😐', name: 'neutral' },
      { char: '😑', name: 'expressionless' },
      { char: '😏', name: 'smirk' },
      { char: '😒', name: 'unamused' },
      { char: '🙄', name: 'rolling eyes' },
      { char: '😬', name: 'grimacing' },
      { char: '😌', name: 'relieved' },
      { char: '😔', name: 'pensive' },
      { char: '😪', name: 'sleepy' },
      { char: '🤤', name: 'drooling' },
      { char: '😴', name: 'sleeping' },
      { char: '😷', name: 'mask' },
      { char: '🤒', name: 'thermometer' },
      { char: '🤕', name: 'bandage' },
      { char: '🤢', name: 'nauseated' },
      { char: '🤮', name: 'vomiting' },
      { char: '🥵', name: 'hot' },
      { char: '🥶', name: 'cold' },
      { char: '🥴', name: 'woozy' },
      { char: '😵', name: 'dizzy' },
      { char: '🤯', name: 'exploding head' },
      { char: '🥳', name: 'partying' },
      { char: '😎', name: 'sunglasses' },
      { char: '🤓', name: 'nerd' },
      { char: '🧐', name: 'monocle' },
      { char: '😕', name: 'confused' },
      { char: '😟', name: 'worried' },
      { char: '😮', name: 'open mouth' },
      { char: '😲', name: 'astonished' },
      { char: '😳', name: 'flushed' },
      { char: '🥺', name: 'pleading' },
      { char: '🥹', name: 'holding back tears' },
      { char: '😦', name: 'frowning' },
      { char: '😨', name: 'fearful' },
      { char: '😰', name: 'anxious' },
      { char: '😥', name: 'sad relieved' },
      { char: '😢', name: 'crying' },
      { char: '😭', name: 'loudly crying' },
      { char: '😱', name: 'screaming' },
      { char: '😖', name: 'confounded' },
      { char: '😣', name: 'persevering' },
      { char: '😞', name: 'disappointed' },
      { char: '😓', name: 'downcast' },
      { char: '😩', name: 'weary' },
      { char: '😫', name: 'tired' },
      { char: '🥱', name: 'yawning' },
      { char: '😤', name: 'triumph' },
      { char: '😡', name: 'pouting' },
      { char: '😠', name: 'angry' },
      { char: '🤬', name: 'cursing' },
    ],
  },
  {
    id: 'gestures',
    name: 'Gestures',
    icon: 'waving_hand',
    emojis: [
      { char: '👋', name: 'wave' },
      { char: '🤚', name: 'raised back of hand' },
      { char: '🖐️', name: 'hand with fingers splayed' },
      { char: '✋', name: 'raised hand' },
      { char: '🖖', name: 'vulcan salute' },
      { char: '👌', name: 'ok hand' },
      { char: '🤌', name: 'pinched fingers' },
      { char: '🤏', name: 'pinching hand' },
      { char: '✌️', name: 'victory hand peace' },
      { char: '🤞', name: 'crossed fingers' },
      { char: '🫰', name: 'hand with index and thumb crossed' },
      { char: '🤟', name: 'love you gesture' },
      { char: '🤘', name: 'sign of the horns rock' },
      { char: '🤙', name: 'call me hand' },
      { char: '👈', name: 'point left' },
      { char: '👉', name: 'point right' },
      { char: '👆', name: 'point up' },
      { char: '👇', name: 'point down' },
      { char: '☝️', name: 'index pointing up' },
      { char: '👍', name: 'thumbs up like' },
      { char: '👎', name: 'thumbs down dislike' },
      { char: '✊', name: 'fist' },
      { char: '👊', name: 'punch brofist' },
      { char: '🤛', name: 'left facing fist' },
      { char: '🤜', name: 'right facing fist' },
      { char: '👏', name: 'clap applause' },
      { char: '🙌', name: 'raising hands celebration' },
      { char: '🫶', name: 'heart hands' },
      { char: '👐', name: 'open hands' },
      { char: '🤲', name: 'palms up together' },
      { char: '🤝', name: 'handshake deal' },
      { char: '🙏', name: 'folded hands pray thank you namaste' },
      { char: '✍️', name: 'writing' },
      { char: '💪', name: 'flexed biceps muscle strong' },
    ],
  },
  {
    id: 'hearts',
    name: 'Hearts & Vibe',
    icon: 'favorite',
    emojis: [
      { char: '❤️', name: 'red heart love' },
      { char: '🧡', name: 'orange heart' },
      { char: '💛', name: 'yellow heart' },
      { char: '💚', name: 'green heart' },
      { char: '💙', name: 'blue heart' },
      { char: '💜', name: 'purple heart' },
      { char: '🖤', name: 'black heart' },
      { char: '🤍', name: 'white heart' },
      { char: '🤎', name: 'brown heart' },
      { char: '💔', name: 'broken heart' },
      { char: '❤️‍🔥', name: 'heart on fire' },
      { char: '❤️‍🩹', name: 'mending heart' },
      { char: '💕', name: 'two hearts' },
      { char: '💞', name: 'revolving hearts' },
      { char: '💓', name: 'beating heart' },
      { char: '💗', name: 'growing heart' },
      { char: '💖', name: 'sparkling heart' },
      { char: '💘', name: 'cupid heart with arrow' },
      { char: '🔥', name: 'fire lit hot' },
      { char: '✨', name: 'sparkles shiny star' },
      { char: '🌟', name: 'glowing star' },
      { char: '💫', name: 'dizzy star' },
      { char: '💥', name: 'collision boom' },
      { char: '💯', name: 'hundred points perfect' },
      { char: '🎉', name: 'party popper tada' },
      { char: '🎊', name: 'confetti ball' },
      { char: '🚀', name: 'rocket fast to the moon' },
      { char: '👑', name: 'crown king queen' },
      { char: '💎', name: 'gem stone diamond' },
      { char: '⚡', name: 'high voltage lightning' },
    ],
  },
  {
    id: 'objects',
    name: 'Objects & Fun',
    icon: 'category',
    emojis: [
      { char: '☕', name: 'coffee tea cup' },
      { char: '🍕', name: 'pizza' },
      { char: '🍔', name: 'hamburger burger' },
      { char: '🍟', name: 'french fries' },
      { char: '🍩', name: 'doughnut donut' },
      { char: '🎂', name: 'birthday cake' },
      { char: '🎈', name: 'balloon' },
      { char: '🎁', name: 'wrapped gift present' },
      { char: '🏆', name: 'trophy champion win' },
      { char: '🥇', name: 'first place gold medal' },
      { char: '⚽', name: 'soccer football ball' },
      { char: '🏀', name: 'basketball' },
      { char: '🎮', name: 'video game controller' },
      { char: '🎧', name: 'headphones music' },
      { char: '📱', name: 'mobile phone cell' },
      { char: '💻', name: 'laptop computer tech' },
      { char: '🖥️', name: 'desktop computer' },
      { char: '💡', name: 'light bulb idea' },
      { char: '📚', name: 'books study read' },
      { char: '📌', name: 'pushpin pin' },
      { char: '🔑', name: 'key secret lock' },
      { char: '🔒', name: 'locked padlock' },
      { char: '🔔', name: 'bell notification alert' },
      { char: '✉️', name: 'envelope email letter' },
      { char: '📢', name: 'loudspeaker shout announcement' },
    ],
  },
];

export function EmojiPicker({ onSelect, onClose, className = '', anchor = 'bottom' }) {
  const [activeCategory, setActiveCategory] = useState('smileys');
  const [searchQuery, setSearchQuery] = useState('');
  const pickerRef = useRef(null);
  const searchInputRef = useRef(null);

  // Close on Escape or outside click
  useEffect(() => {
    function handleKeyDown(e) {
      if (e.key === 'Escape') {
        onClose?.();
      }
    }
    function handleClickOutside(e) {
      if (pickerRef.current && !pickerRef.current.contains(e.target)) {
        onClose?.();
      }
    }

    document.addEventListener('keydown', handleKeyDown);
    document.addEventListener('mousedown', handleClickOutside);
    return () => {
      document.removeEventListener('keydown', handleKeyDown);
      document.removeEventListener('mousedown', handleClickOutside);
    };
  }, [onClose]);

  // Filter emojis based on search query
  const filteredEmojis = useMemo(() => {
    const q = searchQuery.toLowerCase().trim();
    if (!q) return null;

    const results = [];
    for (const cat of EMOJI_CATEGORIES) {
      for (const item of cat.emojis) {
        if (item.name.toLowerCase().includes(q) || item.char === q) {
          results.push(item);
        }
      }
    }
    return results;
  }, [searchQuery]);

  return (
    <div
      ref={pickerRef}
      className={`absolute z-50 w-72 sm:w-80 bg-surface-container-lowest border border-border-hairline rounded-2xl shadow-xl flex flex-col overflow-hidden text-on-surface animate-fade-in ${className}`}
      style={{ maxHeight: '380px' }}
    >
      {/* Search Header */}
      <div className="p-2.5 pb-1.5 border-b border-border-hairline flex items-center gap-2">
        <span className="material-symbols-outlined text-[18px] text-outline">search</span>
        <input
          ref={searchInputRef}
          type="text"
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
          placeholder="Search emojis..."
          className="flex-1 text-xs bg-transparent border-0 outline-none placeholder:text-outline text-on-surface"
          autoFocus
        />
        {searchQuery && (
          <button
            type="button"
            onClick={() => setSearchQuery('')}
            className="text-outline hover:text-on-surface text-xs cursor-pointer p-0.5"
          >
            ✕
          </button>
        )}
      </div>

      {/* Category Tabs (only when not searching) */}
      {!searchQuery && (
        <div className="flex items-center justify-around px-2 py-1 bg-surface-container-low/50 border-b border-border-hairline shrink-0">
          {EMOJI_CATEGORIES.map((cat) => (
            <button
              key={cat.id}
              type="button"
              onClick={() => setActiveCategory(cat.id)}
              className={`p-1.5 rounded-lg flex items-center justify-center transition-all cursor-pointer ${
                activeCategory === cat.id
                  ? 'bg-surface-container-lowest text-primary shadow-xs font-semibold'
                  : 'text-outline hover:text-on-surface'
              }`}
              title={cat.name}
            >
              <span className="material-symbols-outlined text-[18px]">{cat.icon}</span>
            </button>
          ))}
        </div>
      )}

      {/* Emoji Grid Area */}
      <div className="flex-1 overflow-y-auto p-2.5 grid grid-cols-7 sm:grid-cols-8 gap-1.5 custom-scrollbar min-h-[200px]">
        {filteredEmojis !== null ? (
          filteredEmojis.length > 0 ? (
            filteredEmojis.map((e, idx) => (
              <button
                key={idx}
                type="button"
                onClick={() => onSelect(e.char)}
                title={e.name}
                className="w-9 h-9 flex items-center justify-center text-xl rounded-lg hover:bg-surface-container hover:scale-110 active:scale-95 transition-transform cursor-pointer select-none"
              >
                {e.char}
              </button>
            ))
          ) : (
            <div className="col-span-full py-8 text-center text-xs text-outline">
              No emojis found for "{searchQuery}"
            </div>
          )
        ) : (
          EMOJI_CATEGORIES.find((c) => c.id === activeCategory)?.emojis.map((e, idx) => (
            <button
              key={idx}
              type="button"
              onClick={() => onSelect(e.char)}
              title={e.name}
              className="w-9 h-9 flex items-center justify-center text-xl rounded-lg hover:bg-surface-container hover:scale-110 active:scale-95 transition-transform cursor-pointer select-none"
            >
              {e.char}
            </button>
          ))
        )}
      </div>

      {/* Quick reaction footer hint */}
      <div className="px-3 py-1.5 bg-surface-container-low border-t border-border-hairline text-[10px] text-outline flex items-center justify-between font-mono">
        <span>Click to insert</span>
        <span>Esc to close</span>
      </div>
    </div>
  );
}

export default EmojiPicker;
