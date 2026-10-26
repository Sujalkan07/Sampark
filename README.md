# Sampark — Full-Stack C++ OOP Messaging Platform

**Sampark** is a modern, high-performance multi-user web messaging platform built with a high-performance **Object-Oriented C++ backend**, **SQLite database persistence**, **cryptographic application security**, and a responsive **React + Vite + Tailwind CSS** frontend inspired by the Google Stitch Indian tricolour design system.

---

## 1. Project Overview & Capabilities (Phases 1, 2, 3, & 4)

### Phase 1: Foundation, Security & Authentication
- **C++ Backend Engine:** HTTP routing, controllers, services, repositories, and RAII database handle management via `cpp-httplib` and modern C++20.
- **Persistent Data Store:** SQLite3 database with automatic non-destructive schema migrations, WAL mode, foreign keys, and indexed queries.
- **Application Security:** PBKDF2-HMAC-SHA256 password hashing (100,000 iterations with 16-byte cryptographically secure random salt via OpenSSL), constant-time verification (`CRYPTO_memcmp`), and parameterized SQL queries (`sqlite3_bind_*`) preventing SQL injection.
- **Session Architecture:** 32-byte cryptographically secure random session tokens, managed via `HttpOnly`, `SameSite=Lax` cookies with server-side validation and expiration enforcement.
- **Multi-User Isolation:** Completely isolated user accounts and sessions. Authenticated identities are strictly resolved from server session state.
- **Member Discovery:** Registered account discovery with search filtering (`/find-people`).

### Phase 2: One-to-One Persistent Messaging & In-Place Dashboard
- **Normalized One-to-One Conversations:** Deterministic conversation pairs (`user_low_id < user_high_id`) with idempotent creation and retrieval.
- **Persistent Message Storage:** Strict chronological message persistence with millisecond-resolution ISO 8601 timestamps.
- **Strict Server-Side Authorization:** Non-participants receive `403 Forbidden` if attempting to read or send messages in a conversation.
- **Incremental Polling Sync:** High-efficiency message synchronization using cursor queries (`?after={lastMessageId}`) ensuring instant chat delivery without full history re-fetching.
- **WhatsApp Web-Style In-Place Dashboard:** Split-screen layout (`/dashboard` and `/dashboard/:conversationId`) with fixed sidebar, live conversation list, unread message badges, and active chat stage.
- **Read Receipts:** Unread message counts with automatic mark-as-read synchronization (`POST /api/conversations/:id/read`).

### Phase 3: Profiles, Message Management, Search & Online Presence
- **User Profiles & Bio:** Profile editing (`PUT /api/users/profile`) allowing users to update their full name and bio (up to 300 characters) with real-time character counters.
- **Password Updates:** In-app authenticated password change (`PUT /api/auth/password`) with validation of current password and confirmation.
- **Peer Profile View:** Interactive profile modal (`GET /api/users/:id`) to view peer bio, handle, join date, and genuine presence status.
- **Message Editing:** Author-only message editing (`PUT /api/messages/:id`) with populated `edited_at` timestamp and an `(edited)` visual badge. Non-authors are rejected with `403 Forbidden`.
- **Message Soft Deletion:** Author-only message deletion (`DELETE /api/messages/:id`) soft-deletes the message (`is_deleted = 1`), preserving conversation thread symmetry while displaying a tombstone indicator ("*This message was deleted*").
- **In-Conversation Search:** Real-time keyword search (`GET /api/conversations/:id/messages/search?q=...`) with match highlights and hit navigation.
- **Global Message Search:** Cross-conversation message history search (`GET /api/messages/search?q=...`) in the dashboard sidebar, jumping directly to the corresponding chat.
- **Genuine Online Presence:** Activity-based online presence derived from database timestamps (`last_active_at` on `users`), showing "Active now" (within 120s) or "Active Xm ago", with visual presence indicators on avatars.

### Phase 4: Group Chats, Replies, Reactions, Attachments & Dark Mode
- **Group Chats:** Multi-user group creation with member picker (`POST /api/groups`), group member listings with roles (`admin` vs `member`), admin member additions, removals, and auto-admin transfer on creator leave (`POST /api/groups/:id/leave`), plus complete disbanding (`DELETE /api/conversations/:id`).
- **Message Replies:** Quoted message referencing (`reply_to_id`) with cross-conversation authorization enforcement, reply banner in composer, clickable quotes inside message bubbles, and smooth jump-to-message with flash highlight.
- **Emoji Reactions:** Interactive emoji reactions (`POST /api/messages/:id/reactions`) with toggling on/off, counts, user listing popovers, and quick reaction toolbar on message hover.
- **File & Image Sharing:** File attachments (`POST /api/conversations/:id/attachments`) with 10MB size enforcement, extension allowlist, path traversal protection, image lightbox preview, document card downloads with authorization (`GET /api/attachments/:id`).
- **Personalization & Settings:** Appearance (Light, Dark, System theme), custom audio notification chimes (`playNotificationChime` via Web Audio API), desktop browser notification permissions, read receipts toggle (`read_receipts_enabled`), and in-modal profile editing.
- **Typing Bar Emoji Picker:** Custom categorized emoji picker popover with search, keyboard shortcuts (`Esc` to close), and instant insertion into composer.
- **Complete Ergonomic Dark Mode:** CSS custom property system (`:root` / `.dark`) with RGB channels for dynamic opacity, high-contrast typography, dark scrollbars, anti-FOUC pre-hydration script, and 1-click Sun/Moon toggle buttons across Sidebar, Dashboard Header, and Public Navbar.

---

## 2. Visual Identity & Google Stitch Heritage

- **Ashoka Chakra Navy (`#0A1128`):** Bedrock primary color for headings, primary CTAs, outgoing chat bubbles, and structural anchors.
- **Saffron (`#F58220`):** Attention anchor for badges, active thread highlights, and primary actions.
- **India Green (`#138808`):** System telemetry, online indicators, and verified security statuses.
- **Surfaces (`#FAFAF7`, `#F4F4F1`, `#FFFFFF`):** Warm, expansive alabaster surfaces.
- **Hairline Borders (`#E7E9E4`):** 1px structural framing.
- **Typography:** `Inter` for interface hierarchy and `JetBrains Mono` for handles and system telemetry.

---

## 3. C++ OOP Architecture

The backend demonstrates strict Object-Oriented Programming (OOP) principles, clean architecture, and separation of concerns:

```
[ HTTP Request ]
       │
       ▼
[ Server (httplib::Server) ] ────► Handles CORS, pre/post routing, route dispatching
       │
       ▼
[ Controllers ]
  ├── HealthController ──────────► GET /api/health
  ├── AuthController   ──────────► POST /api/auth/register, /login, /logout, GET /me,
  │                                PUT /api/auth/password, PUT /api/users/profile, GET /api/users/:id
  └── MessagingController ───────► POST/GET /api/conversations, GET/POST messages,
                                  PUT/DELETE /api/messages/:id, search endpoints
       │
       ▼
[ Services ]
  ├── Validator ─────────────────► Input validation, sanitization, length limits
  ├── AuthService (IAuthService) ─► Password hashing (PBKDF2), session generation, profile updates
  └── MessagingService (IMessagingService) ──► Conversation authorization, message lifecycle & search
       │
       ▼
[ Repositories ]
  ├── UserRepository (IUserRepository) ──────► User CRUD, profile & presence persistence
  ├── SessionRepository (ISessionRepository) ─► Session lifecycle, expiration cleanup
  ├── ConversationRepository ────────────────► Conversation normalization & idempotency
  └── MessageRepository ─────────────────────► Message pagination, cursor sync, edits, search
       │
       ▼
[ Database Layer ]
  ├── Database (RAII Connection) ────────────► sqlite3* lifecycle, mutex synchronization, migrations
  └── Statement (RAII Statement Wrapper) ────► sqlite3_stmt* binding and stepping
       │
       ▼
[ SQLite Engine (connectx.db) ]
```

### Key OOP Principles
1. **Encapsulation & RAII:** `Database` and `Statement` manage database handles and prepared statements via RAII, preventing memory and resource leaks.
2. **Interface Segregation & Dependency Inversion:** `IUserRepository`, `ISessionRepository`, `IConversationRepository`, `IMessageRepository`, `IAuthService`, and `IMessagingService` decouple layers and enable mock injection for isolated unit testing.
3. **Parameterization:** All queries use `sqlite3_bind_*` with zero untrusted SQL concatenation.
4. **Thread Safety:** Database operations are synchronized using `std::mutex` with SQLite WAL mode.

---

## 4. Database Schema

### `users` Table
```sql
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL COLLATE NOCASE,
    email TEXT UNIQUE NOT NULL COLLATE NOCASE,
    full_name TEXT NOT NULL,
    bio TEXT NOT NULL DEFAULT '',
    password_hash TEXT NOT NULL,
    salt TEXT NOT NULL,
    last_active_at TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
```

### `sessions` Table
```sql
CREATE TABLE IF NOT EXISTS sessions (
    token TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    expires_at TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(token);
CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions(user_id);
```

### `conversations` Table
```sql
CREATE TABLE IF NOT EXISTS conversations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_low_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    user_high_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(user_low_id, user_high_id),
    CHECK(user_low_id < user_high_id)
);
CREATE INDEX IF NOT EXISTS idx_conv_low ON conversations(user_low_id);
CREATE INDEX IF NOT EXISTS idx_conv_high ON conversations(user_high_id);
```

### `messages` Table
```sql
CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    conversation_id INTEGER NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
    sender_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    body TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    read_at TEXT DEFAULT NULL,
    edited_at TEXT DEFAULT NULL,
    is_deleted INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_messages_conv_created ON messages(conversation_id, created_at ASC);
CREATE INDEX IF NOT EXISTS idx_messages_unread ON messages(conversation_id, read_at, sender_id);
```

---

## 5. API Reference

### Authentication & Profiles
| Method | Endpoint | Description | Protected | Request Body | Success Code |
|---|---|---|---|---|---|
| `GET` | `/api/health` | Service health status | No | None | `200 OK` |
| `POST` | `/api/auth/register` | Create account & start session | No | `{ username, email, fullName, password }` | `201 Created` |
| `POST` | `/api/auth/login` | Authenticate & start session | No | `{ identifier, password }` | `200 OK` |
| `POST` | `/api/auth/logout` | Terminate session & clear cookie | No | None | `200 OK` |
| `GET` | `/api/auth/me` | Fetch authenticated user profile | **Yes** | None | `200 OK` |
| `PUT` | `/api/auth/password` | Change user password | **Yes** | `{ currentPassword, newPassword }` | `200 OK` |
| `GET` | `/api/users` | List registered members for discovery | **Yes** | Optional query `?q=` | `200 OK` |
| `GET` | `/api/users/:id` | Fetch peer public profile (bio, presence) | **Yes** | None | `200 OK` |
| `PUT` | `/api/users/profile` | Update profile (full name, bio) | **Yes** | `{ fullName, bio }` | `200 OK` |

### Messaging & Search
| Method | Endpoint | Description | Protected | Request Body | Success Code |
|---|---|---|---|---|---|
| `POST` | `/api/conversations` | Create or get 1-on-1 conversation | **Yes** | `{ participantId }` | `200/201` |
| `GET` | `/api/conversations` | List user's conversations with preview | **Yes** | None | `200 OK` |
| `GET` | `/api/conversations/:id` | Get single conversation details | **Yes** | None | `200 OK` |
| `GET` | `/api/conversations/:id/messages`| Get message stream (supports `?after=`, `?before=`) | **Yes** | None | `200 OK` |
| `POST` | `/api/conversations/:id/messages`| Send message to conversation | **Yes** | `{ body }` | `201 Created` |
| `POST` | `/api/conversations/:id/read` | Mark all unread messages as read | **Yes** | None | `200 OK` |
| `PUT` | `/api/messages/:id` | Edit message (author only) | **Yes** | `{ body }` | `200 OK` |
| `DELETE` | `/api/messages/:id` | Soft delete message (author only) | **Yes** | None | `200 OK` |
| `GET` | `/api/conversations/:id/messages/search`| Search messages in conversation | **Yes** | Query `?q=keyword` | `200 OK` |
| `GET` | `/api/messages/search` | Global search across all user's chats | **Yes** | Query `?q=keyword` | `200 OK` |

---

## 6. Local Setup and Execution

### Prerequisites
- **C++ Compiler:** GCC supporting C++20 (e.g. MSYS2 UCRT64 `g++` 16.2.0 or modern Linux GCC / Clang).
- **Libraries:** `sqlite3`, `libssl`, `libcrypto`, `ws2_32` (Windows) or `pthread` (Linux).
- **Node.js:** v18+ (tested on Node.js v22).
- **Make:** `mingw32-make` or standard `make`.

### Build & Run C++ Backend

1. Navigate to the `backend/` directory:
   ```bash
   cd backend
   ```

2. Compile the server:
   ```bash
   mingw32-make server.exe
   # or on Linux/macOS:
   # make server.exe
   ```

3. Run the server:
   ```bash
   .\server.exe 0.0.0.0 8080 connectx.db
   ```
   The server will start on `http://localhost:8080` and automatically initialize or migrate `connectx.db`.

### Build & Run Frontend

1. In a separate terminal, navigate to the `frontend/` directory:
   ```bash
   cd frontend
   ```

2. Install dependencies (if not already installed):
   ```bash
   npm install
   ```

3. Start the development server:
   ```bash
   npm run dev
   ```
   Open `http://localhost:5173` in your browser. Requests to `/api` are automatically proxied to `http://localhost:8080`.

4. Build for production:
   ```bash
   npm run build
   ```

---

## 7. Automated Testing

### 1. Backend Unit & Integration Tests (61 Tests)
The C++ test suite verifies schema creation, input validation, salted PBKDF2 hashing, duplicate rejections, authentication, multi-user isolation, SQL injection prevention, session invalidation, conversation normalization, messaging permissions, chronological pagination, delta cursor polling, unread count tracking, profile editing, bio length validation, password updates, message editing, soft deletion, in-conversation search, global message search, CORS allowlist verification, concurrent session revocation, redundant soft-delete rejection, search wildcard escaping, schema migration idempotency, and complete persistence across server restarts, as well as Phase 4 group management, message replies, emoji reactions, attachment limits & validation, and user preference synchronization.

Run the C++ unit test suite:
```bash
cd backend
make test
# or manually in WSL / Linux:
# ./test_suite
```
**Result:** 61/61 tests passing (100%).

### 2. End-to-End HTTP API Integration Tests (26 Multi-Step Tests)
Spawns the compiled C++ server on a live TCP port, executes real HTTP network calls across the entire feature surface (health check, CORS preflight allowlist and untrusted origin rejection, user registration, session token security, multi-user session isolation, 1-on-1 messaging, read receipts, privacy boundaries, profile updates, group creation, member management, group message delivery, message replies, emoji reactions, file attachment uploads and downloads, user preferences & Dark Mode updates, group admin actions, group disbanding, and database persistence), and verifies response integrity:
```bash
powershell -ExecutionPolicy Bypass -File .\e2e_integration_test.ps1
```
**Result:** 26/26 tests passing (100%).
