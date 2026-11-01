# Phase 1 Remediation Report: Database Integrity and Transactional Safety

**Document Version:** 1.0.0  
**Project:** Sampark Messaging Application  
**Execution Date:** November 1, 2026  
**Status:** Complete & Verified  

---

## 1. Executive Summary

Phase 1 addresses critical database integrity, transactional safety, and concurrency vulnerabilities identified in the Sampark C++ backend. The remediation eliminated data race conditions during attachment linking, guaranteed atomicity across message creation and metadata updates, rectified read receipt state representation, hardened group creation transactions, enforced unique partial indexing on direct conversations, and implemented a robust physical and metadata lifecycle for attachments.

All 88 existing backend unit, regression, and migration tests continue to pass with zero regressions.

---

## 2. Identified Vulnerabilities & Implemented Solutions

### 2.1 Conditional Attachment Linking & Double-Link Prevention (Phase 1.1)
- **Vulnerability:** Attachment linking previously executed an unconditional `UPDATE attachments SET message_id = ? WHERE id = ?;`. Concurrent message submissions could overwrite existing message associations or cross-link attachments across users/conversations.
- **Remediation:**
  - Implemented `linkAttachmentConditionally(int64_t attachmentId, int64_t messageId, int64_t conversationId, int64_t uploaderId)` in `MessageRepository.cpp`.
  - The SQL update enforces:
    ```sql
    UPDATE attachments
    SET message_id = ?
    WHERE id = ?
      AND message_id IS NULL
      AND conversation_id = ?
      AND uploader_id = ?;
    ```
  - Explicitly verified with `sqlite3_changes(db_.getHandle()) == 1`. Any attempt to claim an already-linked attachment, link to an unauthorized conversation, or link across users immediately fails and is rejected.

### 2.2 Atomic Message Creation & Attachment Linking (Phase 1.2 & 1.6)
- **Vulnerability:** Previously, message creation and attachment linking occurred across separate, uncoordinated database calls. If attachment linking failed after message insertion, the message remained persisted without its attachment, or attachment references were left orphaned. Additionally, conversation `updated_at` timestamps were updated out of band.
- **Remediation:**
  - Introduced the `Transaction` RAII helper in `Database.hpp` / `Database.cpp` supporting `BEGIN IMMEDIATE TRANSACTION` to prevent lock escalation deadlocks in SQLite WAL mode.
  - Implemented `createMessageWithAttachment` in `MessageRepository.cpp` executing within an immediate transaction:
    1. Reply-to validation: verifies parent message belongs to the same conversation.
    2. Attachment pre-validation: verifies ownership, conversation match, and `message_id IS NULL`.
    3. Message insertion into `messages`.
    4. Attachment linking via conditional SQL matching `message_id IS NULL`.
    5. Conversation `updated_at` refresh matching message persistence.
    6. RAII Commit: if any operation fails or throws, rollback occurs automatically.
  - Refactored `MessagingService::sendMessage` to invoke `createMessageWithAttachment` directly.

### 2.3 Attachment Lifecycle and Orphan Cleanup (Phase 1.3)
- **Vulnerability:** Soft-deleted messages retained references to attachments on disk. Group deletion relied on SQLite `CASCADE` which dropped database records but left raw files stranded in `uploads/`. Error handlers in orphan cleanup silently swallowed filesystem exceptions.
- **Remediation:**
  - Added `deleteAttachmentRecord(int64_t attachmentId)` and `getAttachmentPathsForConversation(int64_t conversationId)` in `MessageRepository`.
  - Updated `MessagingService::deleteMessage`: when an author soft-deletes a message, its associated attachment file is deleted from disk and the metadata record is purged.
  - Updated `MessagingService::deleteGroup` and `MessagingService::leaveGroup` (group disbandment): all attachment file paths associated with the conversation are retrieved and unlinked from the filesystem before deleting the conversation record.
  - Hardened `cleanupOrphanedAttachments` with explicit exception logging rather than silent empty catches. Missing files and permission errors are safely handled without corrupting execution.

### 2.4 Read Receipt Representation & Privacy Decoupling (Phase 1.4)
- **Vulnerability:** The legacy system stored the literal string `'suppressed'` inside the `read_at` timestamp column. This conflated timestamp data with a privacy preference, broke SQL date/time ordering, caused unread message count inaccuracies (`read_at IS NULL`), and caused frontend truthiness bugs where `Boolean('suppressed') === true`.
- **Remediation:**
  - Added dedicated column `read_receipt_suppressed INTEGER NOT NULL DEFAULT 0` to the `messages` table.
  - Created Migration 6 in `Database.cpp` to upgrade existing tables and migrate legacy records:
    ```sql
    UPDATE messages SET read_receipt_suppressed = 1 WHERE read_at = 'suppressed';
    ```
  - `read_at` now remains strictly a valid ISO 8601 timestamp or `NULL`.
  - In `MessageRepository::markAsRead`:
    - If read receipts are enabled by both users: `read_at` is set to `datetime('now')` and `read_receipt_suppressed = 0`.
    - If read receipts are disabled by either participant: `read_at` is set to `datetime('now')` (so unread counts accurately clear to 0) and `read_receipt_suppressed = 1`.
  - In `Message::toJson()` and repository extraction: when `readReceiptSuppressed == true`, `readAt` is serialized as `null`, ensuring complete privacy without corrupting unread calculations.

### 2.5 Group Creation Transactional Handling (Phase 1.5)
- **Vulnerability:** Group creation previously used `INSERT OR IGNORE`, did not check individual member insertion success, and did not roll back if member insertions failed.
- **Remediation:**
  - Wrapped `ConversationRepository::createGroup` in an immediate transaction.
  - Added validation for creator user existence via `SELECT 1 FROM users WHERE id = ?`.
  - Pre-validated and deduplicated initial member IDs.
  - Replaced `INSERT OR IGNORE` with strict `INSERT INTO conversation_members` checking `step() == SQLITE_DONE` and `sqlite3_changes() == 1` for each member.
  - If any member cannot be inserted or committed, the entire transaction rolls back and returns `std::nullopt`.
  - Updated `MessagingService::createGroup` to validate that every requested member ID exists via `userRepo_->findById()`, rejecting invalid member IDs with HTTP 400.

### 2.6 Direct Conversation Uniqueness Protection (Phase 1.7)
- **Vulnerability:** Concurrent direct conversation creation requests between two users could result in duplicate conversation records.
- **Remediation:**
  - Added a unique partial index to the database schema and Migration 6:
    ```sql
    CREATE UNIQUE INDEX IF NOT EXISTS idx_conv_direct_unique 
    ON conversations(user_low_id, user_high_id) 
    WHERE type = 'direct';
    ```
  - In Migration 6, added duplicate cleanup prior to index creation, preserving the oldest conversation record and reassigning message records.
  - In `ConversationRepository::getOrCreate`, caught `SQLITE_CONSTRAINT` on the unique index and automatically retrieved the existing conversation record created concurrently.

---

## 3. Files Modified

| Component | Path | Changes |
|-----------|------|---------|
| Database Header | `backend/include/Database.hpp` | Added RAII `Transaction` class, busy timeout, and statement definitions. |
| Database Source | `backend/src/Database.cpp` | Configured `sqlite3_busy_timeout(db_, 5000);`, implemented `Transaction`, added schema column `read_receipt_suppressed`, added Migration 6 with duplicate cleanup and unique partial index. |
| Models Header | `backend/include/Models.hpp` | Added `readReceiptSuppressed` to `Message`, updated `toJson()` to nullify `readAt` when suppressed, aligned `UserPreferences` default. |
| Message Repo Header | `backend/include/MessageRepository.hpp` | Declared `createMessageWithAttachment`, `linkAttachmentConditionally`, `deleteAttachmentRecord`, and `getAttachmentPathsForConversation`. |
| Message Repo Source | `backend/src/MessageRepository.cpp` | Implemented atomic message+attachment insertion, conditional linking, attachment deletion, safe orphan cleanup logging, decoupled read receipts. |
| Conversation Repo Source | `backend/src/ConversationRepository.cpp` | Hardened `createGroup` with immediate transactions and member validation; handled `SQLITE_CONSTRAINT` in `getOrCreate`; maintained member joined_at ordering. |
| Messaging Service Source | `backend/src/MessagingService.cpp` | Switched to atomic message creation; added attachment cleanup on message and group deletion; validated member existence in group creation. |

---

## 4. Verification and Test Results

- **Unit & Regression Suite:** Executed `make test` under WSL Ubuntu 24.04 (`g++ 13.3.0`).
  - Total Tests: **88**
  - Passed: **88**
  - Failed: **0**
- **Test 72 (Legacy Migration):** Successfully verified legacy database upgrade, schema migration idempotence, and preservation of data integrity.
- **Zero Breaking API Changes:** All REST API contracts and frontend responses remain backward compatible.
