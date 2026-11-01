# SAMPARK — PHASE 0 BASELINE INSPECTION & AUDIT REPORT

**Date:** 2026-11-01  
**Author:** Remediation Engineering Team  
**Status:** Baseline Completed — Ready for Phase 1 Execution  

---

## 1. Executive Summary

A comprehensive baseline inspection and audit of the **Sampark** repository was conducted across all backend services, database repositories, frontend client and components, build toolchains, deployment configurations, and test suites.

The application is an end-to-end full-stack messaging platform composed of:
- **Backend:** C++20 using `cpp-httplib` (embedded header) and `nlohmann::json` (embedded header).
- **Database:** SQLite3 running in WAL mode with Foreign Key support.
- **Frontend:** React 18, Vite 6, Tailwind CSS, React Router DOM 6.
- **Deployment & Containers:** Docker (Debian Bookworm multi-stage), Render web service (`render.yaml`), Vercel SPA (`vercel.json`).
- **Testing:** Custom native C++ test suite (`backend/tests/backend_test.cpp`), PowerShell E2E test harness (`e2e_integration_test.ps1`), and Vite production build.

Baseline testing established:
- **C++ Backend Unit Suite:** 88 tests executed, **88 PASS, 0 FAIL**.
- **Frontend Production Build:** Vite v6.4.5 built 52 modules cleanly in 2.71 seconds, **0 ERRORS**.
- **End-to-End HTTP Integration Test:** 26 automated steps executed, **26 PASS, 0 FAIL**.

Despite passing functional baseline suites under single-threaded and nominal paths, the codebase contains critical concurrency, transaction boundary, credential exposure, CSRF vulnerability, error handling, and deployment persistence risks identified during static and transactional audits.

---

## 2. Repository Structure

```
Sampark/
├── .github/
│   └── workflows/
│       └── ci.yml               # GitHub Actions CI workflow
├── backend/
│   ├── include/
│   │   ├── AuthController.hpp
│   │   ├── AuthService.hpp
│   │   ├── ConversationRepository.hpp
│   │   ├── Database.hpp
│   │   ├── HealthController.hpp
│   │   ├── httplib.h            # cpp-httplib single header
│   │   ├── MessageRepository.hpp
│   │   ├── MessagingController.hpp
│   │   ├── MessagingService.hpp
│   │   ├── Models.hpp           # Data structures and JSON serialization
│   │   ├── Server.hpp
│   │   ├── SessionRepository.hpp
│   │   ├── UserRepository.hpp
│   │   ├── Validator.hpp
│   │   └── nlohmann/
│   │       └── json.hpp         # nlohmann::json single header
│   ├── src/
│   │   ├── AuthController.cpp
│   │   ├── AuthService.cpp
│   │   ├── ConversationRepository.cpp
│   │   ├── Database.cpp
│   │   ├── HealthController.cpp
│   │   ├── main.cpp
│   │   ├── MessageRepository.cpp
│   │   ├── MessagingController.cpp
│   │   ├── MessagingService.cpp
│   │   ├── Server.cpp
│   │   ├── SessionRepository.cpp
│   │   ├── UserRepository.cpp
│   │   └── Validator.cpp
│   ├── tests/
│   │   └── backend_test.cpp     # 88 baseline unit/service tests
│   ├── uploads/                 # Attachment upload destination (.gitkeep)
│   └── Makefile                 # Dual Linux/Windows Make configuration
├── frontend/
│   ├── public/
│   │   └── sampark-logo.png
│   ├── src/
│   │   ├── api/
│   │   │   └── client.js        # Fetch API client and error models
│   │   ├── components/
│   │   │   ├── AttachmentsModal.jsx
│   │   │   ├── ChatPanel.jsx    # Primary messaging and polling panel
│   │   │   ├── CreateGroupModal.jsx
│   │   │   ├── GroupDetailsModal.jsx
│   │   │   ├── NotificationPreferencesModal.jsx
│   │   │   ├── ProfileModal.jsx
│   │   │   └── SearchModal.jsx
│   │   ├── context/
│   │   │   └── AuthContext.jsx  # Authentication state & provider
│   │   ├── pages/
│   │   │   ├── DashboardPage.jsx
│   │   │   ├── LoginPage.jsx
│   │   │   └── RegisterPage.jsx
│   │   ├── App.jsx
│   │   ├── index.css
│   │   └── main.jsx
│   ├── package.json
│   ├── vite.config.js
│   └── tailwind.config.js
├── Dockerfile                   # Multi-stage Docker build
├── .dockerignore
├── render.yaml                  # Render service blueprint
├── vercel.json                  # Vercel SPA routing rules
├── e2e_integration_test.ps1     # 26-step PowerShell API test
└── README.md
```

---

## 3. Current Architecture

1. **Client Tier:**
   - Single Page Application (SPA) built with React 18, React Router 6, and Vite.
   - Communicates with the backend via JSON REST APIs over HTTP/HTTPS.
   - Relies on polling intervals for live updates (2.5s for messages, 4.0s for conversation list).

2. **API & Routing Tier:**
   - Multi-threaded HTTP server implemented using `cpp-httplib`.
   - Thread pool handles incoming TCP connections.
   - Controllers (`AuthController`, `MessagingController`, `HealthController`) parse HTTP requests, map routing regexes, and invoke service abstractions.

3. **Service Tier:**
   - `AuthService` handles registration, PBKDF2 password hashing with per-user salt (100k iterations), session issuance, profile updates, and discovery.
   - `MessagingService` enforces conversation participation, group member roles, message validation, reply threading, attachment uploads, reactions, and preferences.

4. **Persistence Tier:**
   - Repository pattern (`UserRepository`, `SessionRepository`, `ConversationRepository`, `MessageRepository`).
   - SQLite3 database accessed via a RAII `Database` class and prepared `Statement` wrapper.
   - Transactions are used ad-hoc in some repositories, but lacking across cross-repository or multi-step operations.

---

## 4. Baseline Test & Build Results

### 4.1 C++ Backend Test Suite
- **Command:** `make clean && make test` (Ubuntu 24.04 / GCC 13.3.0 C++20)
- **Result:** **88 of 88 PASS**
- **Output Excerpt:**
  ```
  >>> STARTING SAMPARK BACKEND TEST SUITE <<<
  [PASS] Test 1: Database initialization and schema creation
  ...
  [PASS] Test 88: MessagingController handleRemoveContact: Removal over HTTP verified
  =============================================
   ALL 88 BACKEND TESTS PASSED SUCCESSFULLY!   
  =============================================
  ```

### 4.2 Frontend Build
- **Command:** `npm run build` (Node 20.x, Vite 6.4.5)
- **Result:** **SUCCESS**
  ```
  dist/index.html                   1.93 kB │ gzip:  0.88 kB
  dist/assets/index-Bn1TIj4p.css   45.56 kB │ gzip:  8.17 kB
  dist/assets/index-DtwJadne.js   340.85 kB │ gzip: 89.64 kB
  built in 2.71s
  ```

### 4.3 End-to-End HTTP Integration Test
- **Command:** `powershell -ExecutionPolicy Bypass -File .\e2e_integration_test.ps1`
- **Result:** **26 of 26 PASS (100%)**
- **Tested:** Health, CORS preflight and untrusted rejection, registration, session cookies, duplicate rejection, session isolation, user discovery, direct conversation creation, message send & reply, message history retrieval, read receipts, profile updates, group chat lifecycle, replies, reactions, attachment upload/download, user preferences, and group deletion.

---

## 5. Audit Findings & Known Issues

### 5.1 Phase 1 — Database Integrity & Transactional Safety
1. **Attachment Linking Race Condition (`MessageRepository.cpp:601`):**
   - Current query: `UPDATE attachments SET message_id = ? WHERE id = ?;`
   - Does not check `message_id IS NULL`, does not verify row count via `sqlite3_changes()`. A concurrent call can overwrite an existing attachment association or link an attachment twice.
2. **Non-Atomic Message Creation & Attachment Linking (`MessagingService.cpp:277-300`):**
   - Message insertion occurs first; attachment linking occurs in a separate database call. If the process crashes or fails between operations, a message is persisted without attachment, or an attachment remains orphaned.
3. **Incomplete Attachment Deletion and Orphan Cleanup (`MessageRepository.cpp`, `MessagingService.cpp`):**
   - Deleting a message does not delete its physical file.
   - Deleting a conversation removes rows via foreign-key cascade without purging disk files.
   - Filesystem failures during cleanup are ignored silently (`catch (...) {}`).
4. **Read Receipt Representation Anti-Pattern (`MessageRepository.cpp:354`):**
   - Stores literal string `'suppressed'` in the timestamp column `read_at`.
   - Conflates a timestamp with a privacy state.
5. **Group Creation Transaction Vulnerability (`ConversationRepository.cpp:279-331`):**
   - Uses `INSERT OR IGNORE` for initial group members without verifying return codes or foreign key violations.
   - `sqlite3_exec(COMMIT)` return code is not checked. If member insertion fails due to an invalid member ID, the transaction commits an incomplete group.
6. **Message Timestamp Consistency (`MessageRepository.cpp:185`):**
   - Message insert and conversation `updated_at` are executed separately without guaranteeing atomic commit.
7. **Direct-Conversation Duplicate Vulnerability (`ConversationRepository.cpp:38-65`, `Database.cpp`):**
   - Schema lacks a `UNIQUE` index or constraint on `(user_low_id, user_high_id)` for `type = 'direct'`.
   - Concurrent requests between the same pair of users can insert duplicate conversations.

### 5.2 Phase 2 — Authentication & Security Hardening
1. **Session Token Exposure in LocalStorage (`AuthController.cpp:104, 139`, `client.js:74, 85`):**
   - JSON responses return `token: result.session->token`.
   - Frontend explicitly saves it into `localStorage.setItem('sampark_token', ...)`.
   - All subsequent requests inject `Authorization: Bearer <token>`.
   - Exposes session credentials to Cross-Site Scripting (XSS).
2. **Missing CSRF Protection (`Server.cpp:94-139`):**
   - State-changing endpoints (POST, PUT, DELETE) do not reject untrusted `Origin` or `Referer` headers when receiving cookie-authenticated browser requests.
   - Missing CSRF validation allows cross-site request forgery when cookies are sent.
3. **Payload Type Validation Failures (`AuthController.cpp:83-88, 120-123`):**
   - Non-object payloads (e.g., JSON array `[]` or primitive numbers) trigger `json::type_error` caught by generic catch, returning HTTP 500 instead of HTTP 400.
4. **Missing Authentication Rate Limiting:**
   - No rate limiting or backoff on `/api/auth/login`, `/api/auth/register`, or `/api/auth/password`.
   - Vulnerable to brute force credential attacks and automated abuse.

### 5.3 Phase 3 — Database Reliability & Concurrency
1. **Inconsistent Notification Preference Initialization (`Models.hpp:162`, `Database.cpp:147`, `MessageRepository.cpp:731`):**
   - `Models.hpp` initializes `desktopNotifications = false`.
   - SQLite schema defaults `desktop_notifications = 0`.
   - `MessageRepository.cpp` inserts `1` for `desktop_notifications` on first query.
   - Initial retrieval returns `false`, while subsequent queries return `true`.
2. **SQLite Concurrency & Busy Handling:**
   - Need explicit busy timeout configuration (`sqlite3_busy_timeout`) and structured retries for transient locks.
3. **Versioned Migration Architecture:**
   - Database schema has rudimentary migration steps; need robust versioning and migration verification to safely handle schema changes (such as separating read receipt suppression from timestamps and adding unique partial indexes).

### 5.4 Phase 4 — Frontend Reliability
1. **Auth State Network Failure Sensitivity (`AuthContext.jsx:24-27`):**
   - Any network exception or temporary 5xx during `checkSession()` immediately executes `setUser(null)`, logging out active users during transient disconnects.
2. **Unthrottled Background Polling (`ChatPanel.jsx:364`, `DashboardPage.jsx:152`):**
   - Polling continues at fixed 2.5s / 4s even when browser tab is inactive/hidden.
   - No exponential backoff on repeated failures.
   - Potential overlapping fetches under slow network conditions.
3. **Attachment Upload and Send Error Handling:**
   - Incomplete retry flow when upload succeeds but message send fails.

### 5.5 Phase 5 — Deployment & Infrastructure
1. **Ephemeral Container Storage (`Dockerfile:39-44`, `render.yaml`):**
   - SQLite database is located at default relative path `connectx.db`.
   - Uploads are saved to `./uploads`.
   - `render.yaml` has no persistent disk configuration; container restarts wipe all messages and uploaded files.
2. **Container Security (`Dockerfile:39`):**
   - Container runs as root; directory uses `chmod 777 uploads`.
3. **CI Dependency Deficiency (`ci.yml:20`):**
   - GitHub Actions workflow installs `build-essential libsqlite3-dev` but omits `libssl-dev`.

---

## 6. Files Requiring Modifications

| Domain | File Path | Scope of Remediation |
|---|---|---|
| Backend | `backend/src/MessageRepository.cpp` | Conditional linking, atomic message+attachment transaction, orphan cleanup, read receipt representation, preference default fix. |
| Backend | `backend/include/MessageRepository.hpp` | Interface updates for transactional linking and lifecycle methods. |
| Backend | `backend/src/MessagingService.cpp` | Atomic create-and-link orchestration, member validation, cleanup triggers. |
| Backend | `backend/include/MessagingService.hpp` | Method signatures and member verification helper declarations. |
| Backend | `backend/src/ConversationRepository.cpp` | Safe group creation transaction, member verification, direct conversation uniqueness protection. |
| Backend | `backend/include/ConversationRepository.hpp` | Signatures for member checks and idempotent direct conversation retrieval. |
| Backend | `backend/src/Database.cpp` | Schema migration (read receipts, unique partial index, busy timeout configuration). |
| Backend | `backend/include/Database.hpp` | Helper functions, busy timeout configuration, transaction helpers. |
| Backend | `backend/src/AuthController.cpp` | Cookie-only browser response (strip token from JSON), strict JSON type validation, rate limiting integration. |
| Backend | `backend/src/Server.cpp` | CSRF origin/referer enforcement for mutations, rate limit filters, persistent directory initialization. |
| Backend | `backend/include/Server.hpp` | Route definitions, CSRF and rate limiting state. |
| Backend | `backend/include/Models.hpp` | Fix `UserPreferences` default values and read receipt model fields. |
| Backend | `backend/tests/backend_test.cpp` | Unit tests for all Phase 1-6 remediation items. |
| Frontend | `frontend/src/api/client.js` | Eliminate localStorage token storage and Authorization Bearer header; use HttpOnly cookies and CSRF headers. |
| Frontend | `frontend/src/context/AuthContext.jsx` | Disentangle network failure from 401 unauthenticated; resilient session retry. |
| Frontend | `frontend/src/components/ChatPanel.jsx` | Tab visibility polling throttle, backoff, retryable attachment send, read receipt UI alignment. |
| Frontend | `frontend/src/pages/DashboardPage.jsx` | Visibility-aware conversation polling with backoff. |
| Infra | `Dockerfile` | Dedicated non-root runner user, secure permission boundaries, persistent path envs. |
| Infra | `render.yaml` | Persistent disk attachment (`/var/data`), updated `DATABASE_PATH` and `UPLOAD_DIRECTORY`. |
| Infra | `.github/workflows/ci.yml` | Add `libssl-dev` to apt packages, run new test targets. |

---

## 7. Compatibility Risks & Mitigation Strategy

1. **HttpOnly Cookie Transition vs Non-Browser Clients:**
   - *Risk:* Automated scripts or API clients that pass `Authorization: Bearer <token>` could break if Bearer support is removed.
   - *Mitigation:* Preserve server-side extraction of `Authorization: Bearer` in `extractSessionToken()`, but stop emitting tokens to the browser in login/register JSON and stop saving tokens to `localStorage`.
2. **Direct Conversation Unique Partial Index:**
   - *Risk:* If an existing database has duplicate direct conversations, creating the unique index will fail with `SQLITE_CONSTRAINT`.
   - *Mitigation:* Migration script will deduplicate any duplicate pairs beforehand by merging or picking the newest conversation before executing `CREATE UNIQUE INDEX`.
3. **Read Receipt Schema Migration:**
   - *Risk:* Historical messages with `read_at = 'suppressed'` could be corrupted or lost.
   - *Mitigation:* Migration will convert `'suppressed'` records by populating `read_receipt_suppressed = 1` and restoring `read_at` to NULL or message creation timestamp, preserving existing read state.
4. **Persistent Mount Permissions on Container Start:**
   - *Risk:* Render persistent disk mounted as non-root user may fail permissions if directories don't exist.
   - *Mitigation:* C++ backend checks and creates directory paths using standard POSIX/C++17 `std::filesystem::create_directories` at boot with appropriate logging.

---

## 8. Remediation Implementation Plan

- [x] **Phase 0: Baseline Inspection and Audit** (Current)
- [ ] **Phase 1: Database Integrity and Transactional Safety**
  - Conditional attachment linking (`WHERE id = ? AND message_id IS NULL`) with `sqlite3_changes()`.
  - Atomic message creation and attachment linking transaction.
  - Attachment lifecycle: delete on message hard delete, delete on conversation delete, robust orphan cleanup.
  - Decouple read receipt suppression from timestamp representation.
  - Transactionally safe group creation with strict member validation and commit checks.
  - Atomic message insert and conversation `updated_at` update.
  - Direct conversation uniqueness partial index and deduplication.
- [ ] **Phase 2: Authentication and Security Hardening**
  - Remove localStorage token persistence from frontend; remove token from login/register response JSON.
  - CSRF origin/referer validation for state-changing requests.
  - Strict authentication payload type checking (prevent 500 on bad JSON types).
  - In-memory thread-safe rate limiter with progressive backoff on auth endpoints.
  - Security review of PBKDF2 iterations and session entropy.
- [ ] **Phase 3: Database Reliability & Concurrency**
  - Unify `UserPreferences` defaults across `Models.hpp`, `Database.cpp`, and `MessageRepository.cpp`.
  - Configure `sqlite3_busy_timeout` (5000ms) and retry logic on `SQLITE_BUSY`.
  - Versioned migration table update for read receipts and indexes.
- [ ] **Phase 4: Frontend Reliability**
  - AuthContext multi-state (`authenticated`, `unauthenticated`, `checking`, `temporarily_unavailable`).
  - Visibility-aware polling with backoff in `ChatPanel` and `DashboardPage`.
  - Robust attachment failure and retry handling in `ChatPanel`.
- [ ] **Phase 5: Deployment and Infrastructure**
  - Persistent volume configuration in `render.yaml`.
  - Non-root user and permission hardening in `Dockerfile`.
  - Fix `libssl-dev` dependency in `.github/workflows/ci.yml`.
- [ ] **Phase 6: Comprehensive Testing & Verification**
  - Backend unit and concurrency tests for all new behaviors.
  - Integration tests for end-to-end flows.
  - Verification of builds and full audit documentation.
