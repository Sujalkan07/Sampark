# ConnectX — Phase 1: Foundation & Authentication

ConnectX is a modern, high-performance multi-user chat platform designed as a C++ OOP engineering project. It combines an object-oriented C++ backend, SQLite database persistence, application-level cryptographic security, and a responsive frontend interface integrated from Google Stitch.

---

## 1. Project Overview & Scope

### Phase 1 Included Capabilities
- **C++ Backend Engine:** HTTP routing, controllers, services, repositories, and RAII database handle management.
- **Persistent Data Store:** SQLite3 database with schema migrations, WAL mode, foreign keys, and indexes.
- **Application Security:** PBKDF2-HMAC-SHA256 password hashing (100,000 iterations with 16-byte cryptographically secure random salt via OpenSSL), constant-time verification, and parameterized SQL statements preventing SQL injection.
- **Session Architecture:** 32-byte cryptographically secure random session tokens, managed via `HttpOnly`, `SameSite=Lax` cookies with server-side validation and expiration enforcement.
- **Multi-User Isolation:** Completely isolated user accounts and sessions. Authenticated identities are strictly resolved from server session state.
- **Frontend Integration:** Full React + Vite + Tailwind CSS SPA reproducing the approved Google Stitch design tokens and responsive layouts.
- **Screen Flow:** Public Landing Page (`/`), Sign Up (`/signup`), Log In (`/login`), Protected Chat Dashboard (`/dashboard`), Protected Find People Directory Shell (`/find-people`), and Active Conversation Shell (`/conversation`).

### Explicit Phase 1 Boundaries
- Real-time WebSocket packet delivery, message persistence, group creation, online presence broadcasts, typing indicators, and message editing/deletion are deferred to **Phase 2 & Phase 3**.
- Empty states honestly inform the user that messaging functionality will be connected in Phase 2.

---

## 2. Google Stitch MCP Integration & Design Tokens

### Design Source & Verification
- **Stitch MCP Server:** Antigravity's lazy-loaded `StitchMCP` tool was inspected and utilized to query project `6993837939835101471` (ConnectX Tricolour).
- **Retrieved Assets:** Screens were audited via `list_screens`, `get_screen`, and `download_file`. Specific empty state templates (`Desktop Chat Dashboard — Empty State`) and desktop auth flows were retrieved directly.
- **Local Stitch Export:** The export in `stitch_export/stitch_connectx_tricolour_chat_ui` was audited against the live Stitch project to confirm complete design token parity.

### Visual Identity Tokens
- **Ashoka Chakra Navy (`#0A1128`):** Bedrock primary color for headings, primary CTAs, outgoing chat bubbles, and structural anchors.
- **Saffron (`#F58220`):** Non-destructive attention anchor for badges, active thread highlights, and primary actions.
- **India Green (`#138808`):** System telemetry, online indicators, and verified security statuses.
- **Surfaces (`#FAFAF7`, `#F4F4F1`, `#FFFFFF`):** Expansive, warm alabaster surfaces.
- **Hairline Borders (`#E7E9E4`):** 1px structural framing.
- **Typography:** `Inter` for interface hierarchy and `JetBrains Mono` for handles and system telemetry.

---

## 3. C++ OOP Architecture

The backend demonstrates strict Object-Oriented Programming (OOP) principles and separation of concerns:

```
[ HTTP Request ]
       │
       ▼
[ Server (httplib::Server) ] ────► Handles CORS, pre/post routing, route dispatching
       │
       ▼
[ Controllers ]
  ├── HealthController ──────────► GET /api/health
  └── AuthController   ──────────► POST /api/auth/register, /login, /logout, GET /me
       │
       ▼
[ Services ]
  ├── Validator ─────────────────► Format validation, trimming, sanitization
  └── AuthService (IAuthService) ─► Password hashing (PBKDF2), session generation, auth logic
       │
       ▼
[ Repositories ]
  ├── UserRepository (IUserRepository) ──────► User CRUD, parameterized SQL
  └── SessionRepository (ISessionRepository) ─► Session lifecycle, expiration cleanup
       │
       ▼
[ Database Layer ]
  ├── Database (RAII Connection) ────────────► sqlite3* lifecycle, mutex synchronization
  └── Statement (RAII Statement Wrapper) ────► sqlite3_stmt* binding and stepping
       │
       ▼
[ SQLite Engine (connectx.db) ]
```

### Key OOP Principles
1. **Encapsulation & RAII:** `Database` and `Statement` manage database handles and prepared statements via RAII, preventing memory and resource leaks.
2. **Interface Segregation & Dependency Inversion:** `IUserRepository`, `ISessionRepository`, and `IAuthService` allow decoupled testing and mock injection.
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
    password_hash TEXT NOT NULL,
    salt TEXT NOT NULL,
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

---

## 5. API Reference

| Method | Endpoint | Description | Protected | Request Body | Success Code |
|---|---|---|---|---|---|
| `GET` | `/api/health` | Service health status | No | None | `200 OK` |
| `POST` | `/api/auth/register` | Create account & start session | No | `{ username, email, fullName, password }` | `201 Created` |
| `POST` | `/api/auth/login` | Authenticate & start session | No | `{ identifier, password }` | `200 OK` |
| `POST` | `/api/auth/logout` | Terminate session & clear cookie | No | None | `200 OK` |
| `GET` | `/api/auth/me` | Fetch authenticated user profile | **Yes** | None (reads cookie / Bearer token) | `200 OK` |

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
   The server will start on `http://localhost:8080` and automatically initialize `connectx.db`.

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

4. (Optional) Build for production:
   ```bash
   npm run build
   ```

---

## 7. Automated Testing

### Backend Unit & Integration Tests (15 Tests)
The C++ test suite verifies schema creation, input validation, salted PBKDF2 hashing, duplicate rejections, authentication, multi-user isolation, SQL injection prevention, session invalidation, and persistence across server restarts.

Run the C++ test suite:
```bash
cd backend
mingw32-make test
```

### End-to-End HTTP API Integration Tests (10 Tests)
Spawns the compiled C++ server on a live TCP port, executes real HTTP network calls (health, registration, duplicate checks, session validation, multi-user isolation, wrong password rejection, logout, and protected route denials), and verifies response integrity:
```bash
powershell -ExecutionPolicy Bypass -File .\e2e_integration_test.ps1
```

---

## 8. Security Specifications

1. **Password Protection:** Plaintext passwords are never persisted. Passwords are salted with 16 bytes from OpenSSL's cryptographically secure pseudo-random number generator (`RAND_bytes`) and hashed using PBKDF2-HMAC-SHA256 (100,000 iterations). Verification is executed using OpenSSL's constant-time comparison (`CRYPTO_memcmp`).
2. **Session Security:** Sessions use 32-byte cryptographic random hex strings (64 characters). Cookies are dispatched with `HttpOnly`, `SameSite=Lax`, and `Path=/`.
3. **Server-Side Authorization:** Every protected endpoint verifies the session token against the database. No client-side identity claim is trusted without verified session mapping.
4. **Injection Immunity:** All SQL queries are strictly compiled and executed through prepared statements (`sqlite3_bind_*`).
