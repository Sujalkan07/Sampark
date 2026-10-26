#include "Database.hpp"
#include <iostream>

namespace connectx {

Database::Database(const std::string& dbPath) : dbPath_(dbPath) {
    open(dbPath_);
}

Database::~Database() {
    close();
}

bool Database::open(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }

    dbPath_ = dbPath;
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Failed to open database " << dbPath_ << ": "
                  << sqlite3_errmsg(db_) << std::endl;
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // Enable WAL mode and foreign key constraints
    char* errMsg = nullptr;
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);

    sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);

    return true;
}

void Database::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Execution error: " << (errMsg ? errMsg : "Unknown") << std::endl;
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool Database::initSchema() {
    const std::string schemaSql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL COLLATE NOCASE,
            email TEXT UNIQUE NOT NULL COLLATE NOCASE,
            full_name TEXT NOT NULL,
            password_hash TEXT NOT NULL,
            salt TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at TEXT NOT NULL DEFAULT (datetime('now')),
            bio TEXT DEFAULT '',
            last_active_at TEXT DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS sessions (
            token TEXT PRIMARY KEY,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            expires_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS conversations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            type TEXT NOT NULL DEFAULT 'direct',
            name TEXT NOT NULL DEFAULT '',
            description TEXT NOT NULL DEFAULT '',
            avatar_url TEXT NOT NULL DEFAULT '',
            created_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
            user_low_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
            user_high_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS conversation_members (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            conversation_id INTEGER NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            role TEXT NOT NULL DEFAULT 'member',
            joined_at TEXT NOT NULL DEFAULT (datetime('now')),
            last_read_message_id INTEGER DEFAULT 0,
            UNIQUE(conversation_id, user_id)
        );

        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            conversation_id INTEGER NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
            sender_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            body TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            read_at TEXT DEFAULT NULL,
            edited_at TEXT DEFAULT NULL,
            is_deleted INTEGER NOT NULL DEFAULT 0,
            reply_to_id INTEGER DEFAULT NULL REFERENCES messages(id) ON DELETE SET NULL
        );

        CREATE TABLE IF NOT EXISTS message_reactions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            message_id INTEGER NOT NULL REFERENCES messages(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            emoji TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            UNIQUE(message_id, user_id, emoji)
        );

        CREATE TABLE IF NOT EXISTS attachments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            message_id INTEGER REFERENCES messages(id) ON DELETE CASCADE,
            uploader_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            conversation_id INTEGER NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
            file_name TEXT NOT NULL,
            file_path TEXT NOT NULL,
            file_size INTEGER NOT NULL,
            mime_type TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS user_preferences (
            user_id INTEGER PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
            theme TEXT NOT NULL DEFAULT 'system',
            sound_notifications INTEGER NOT NULL DEFAULT 1,
            desktop_notifications INTEGER NOT NULL DEFAULT 1,
            read_receipts_enabled INTEGER NOT NULL DEFAULT 1,
            updated_at TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
        CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
        CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(token);
        CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions(user_id);
        CREATE INDEX IF NOT EXISTS idx_conv_user_low ON conversations(user_low_id);
        CREATE INDEX IF NOT EXISTS idx_conv_user_high ON conversations(user_high_id);
        CREATE INDEX IF NOT EXISTS idx_conv_type ON conversations(type);
        CREATE INDEX IF NOT EXISTS idx_conv_updated_at ON conversations(updated_at DESC);
        CREATE INDEX IF NOT EXISTS idx_cm_conv_user ON conversation_members(conversation_id, user_id);
        CREATE INDEX IF NOT EXISTS idx_cm_user ON conversation_members(user_id);
        CREATE INDEX IF NOT EXISTS idx_msg_conv_id ON messages(conversation_id);
        CREATE INDEX IF NOT EXISTS idx_msg_conv_created ON messages(conversation_id, id ASC);
        CREATE INDEX IF NOT EXISTS idx_msg_sender ON messages(sender_id);
        CREATE INDEX IF NOT EXISTS idx_msg_unread ON messages(conversation_id, sender_id, read_at);
        CREATE INDEX IF NOT EXISTS idx_msg_search ON messages(conversation_id, body);
        CREATE INDEX IF NOT EXISTS idx_msg_reply ON messages(reply_to_id);
        CREATE INDEX IF NOT EXISTS idx_mr_msg ON message_reactions(message_id);
        CREATE INDEX IF NOT EXISTS idx_mr_user ON message_reactions(user_id);
        CREATE INDEX IF NOT EXISTS idx_att_msg ON attachments(message_id);
        CREATE INDEX IF NOT EXISTS idx_att_conv ON attachments(conversation_id);
    )";

    bool ok = execute(schemaSql);
    if (!ok) return false;

    migrateSchema();
    return true;
}

bool Database::hasColumn(const std::string& tableName, const std::string& columnName) {
    if (!db_) return false;
    std::string sql = "PRAGMA table_info(" + tableName + ");";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK || !stmt) return false;

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* colName = sqlite3_column_text(stmt, 1);
        if (colName && columnName == reinterpret_cast<const char*>(colName)) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

void Database::migrateSchema() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return;

    // 1. Ensure migrations table exists
    char* errMsg = nullptr;
    sqlite3_exec(db_,
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "version INTEGER PRIMARY KEY, "
        "name TEXT NOT NULL, "
        "applied_at TEXT NOT NULL DEFAULT (datetime('now')));",
        nullptr, nullptr, &errMsg);
    if (errMsg) {
        std::cerr << "[Database] Failed to create schema_migrations: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        errMsg = nullptr;
    }

    // 2. Migration 1 & 2 baseline
    sqlite3_exec(db_,
        "INSERT OR IGNORE INTO schema_migrations (version, name) VALUES (1, 'phase1_foundation'), (2, 'phase2_messaging');",
        nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

    // 3. Migration 3: Phase 3 columns & indexes
    if (!hasColumn("users", "bio")) {
        int rc = sqlite3_exec(db_, "ALTER TABLE users ADD COLUMN bio TEXT DEFAULT '';", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK && errMsg) {
            std::cerr << "[Database] Migration warning (users.bio): " << errMsg << std::endl;
            sqlite3_free(errMsg); errMsg = nullptr;
        }
    }
    if (!hasColumn("users", "last_active_at")) {
        int rc = sqlite3_exec(db_, "ALTER TABLE users ADD COLUMN last_active_at TEXT DEFAULT '';", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK && errMsg) {
            std::cerr << "[Database] Migration warning (users.last_active_at): " << errMsg << std::endl;
            sqlite3_free(errMsg); errMsg = nullptr;
        }
    }

    if (!hasColumn("messages", "edited_at")) {
        int rc = sqlite3_exec(db_, "ALTER TABLE messages ADD COLUMN edited_at TEXT DEFAULT NULL;", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK && errMsg) {
            std::cerr << "[Database] Migration warning (messages.edited_at): " << errMsg << std::endl;
            sqlite3_free(errMsg); errMsg = nullptr;
        }
    }
    if (!hasColumn("messages", "is_deleted")) {
        int rc = sqlite3_exec(db_, "ALTER TABLE messages ADD COLUMN is_deleted INTEGER NOT NULL DEFAULT 0;", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK && errMsg) {
            std::cerr << "[Database] Migration warning (messages.is_deleted): " << errMsg << std::endl;
            sqlite3_free(errMsg); errMsg = nullptr;
        }
    }

    sqlite3_exec(db_,
        "INSERT OR IGNORE INTO schema_migrations (version, name) VALUES (3, 'phase3_profiles_and_search');",
        nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

    // 4. Migration 4: Phase 4 Group Chats, Replies, Reactions, Attachments, and Settings
    if (!hasColumn("conversations", "type")) {
        // Safe migration: disable foreign keys temporarily during table swap
        sqlite3_exec(db_, "PRAGMA foreign_keys = OFF;", nullptr, nullptr, nullptr);
        const char* migrateConvSql = R"(
            CREATE TABLE conversations_phase4 (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                type TEXT NOT NULL DEFAULT 'direct',
                name TEXT NOT NULL DEFAULT '',
                description TEXT NOT NULL DEFAULT '',
                avatar_url TEXT NOT NULL DEFAULT '',
                created_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
                user_low_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
                user_high_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
                created_at TEXT NOT NULL DEFAULT (datetime('now')),
                updated_at TEXT NOT NULL DEFAULT (datetime('now'))
            );
            INSERT INTO conversations_phase4 (id, type, name, description, avatar_url, created_by, user_low_id, user_high_id, created_at, updated_at)
            SELECT id, 'direct', '', '', '', NULL, user_low_id, user_high_id, created_at, updated_at FROM conversations;
            DROP TABLE conversations;
            ALTER TABLE conversations_phase4 RENAME TO conversations;
            CREATE INDEX IF NOT EXISTS idx_conv_user_low ON conversations(user_low_id);
            CREATE INDEX IF NOT EXISTS idx_conv_user_high ON conversations(user_high_id);
            CREATE INDEX IF NOT EXISTS idx_conv_type ON conversations(type);
            CREATE INDEX IF NOT EXISTS idx_conv_updated_at ON conversations(updated_at DESC);
        )";
        int rc = sqlite3_exec(db_, migrateConvSql, nullptr, nullptr, &errMsg);
        sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK && errMsg) {
            std::cerr << "[Database] Migration warning (conversations phase4): " << errMsg << std::endl;
            sqlite3_free(errMsg); errMsg = nullptr;
        }
    }

    // conversation_members
    sqlite3_exec(db_, R"(
        CREATE TABLE IF NOT EXISTS conversation_members (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            conversation_id INTEGER NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            role TEXT NOT NULL DEFAULT 'member',
            joined_at TEXT NOT NULL DEFAULT (datetime('now')),
            last_read_message_id INTEGER DEFAULT 0,
            UNIQUE(conversation_id, user_id)
        );
        CREATE INDEX IF NOT EXISTS idx_cm_conv_user ON conversation_members(conversation_id, user_id);
        CREATE INDEX IF NOT EXISTS idx_cm_user ON conversation_members(user_id);
    )", nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

    // messages.reply_to_id
    if (!hasColumn("messages", "reply_to_id")) {
        int rc = sqlite3_exec(db_, "ALTER TABLE messages ADD COLUMN reply_to_id INTEGER DEFAULT NULL REFERENCES messages(id) ON DELETE SET NULL;", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK && errMsg) {
            std::cerr << "[Database] Migration warning (messages.reply_to_id): " << errMsg << std::endl;
            sqlite3_free(errMsg); errMsg = nullptr;
        }
    }

    // message_reactions
    sqlite3_exec(db_, R"(
        CREATE TABLE IF NOT EXISTS message_reactions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            message_id INTEGER NOT NULL REFERENCES messages(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            emoji TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            UNIQUE(message_id, user_id, emoji)
        );
        CREATE INDEX IF NOT EXISTS idx_mr_msg ON message_reactions(message_id);
        CREATE INDEX IF NOT EXISTS idx_mr_user ON message_reactions(user_id);
    )", nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

    // attachments
    sqlite3_exec(db_, R"(
        CREATE TABLE IF NOT EXISTS attachments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            message_id INTEGER REFERENCES messages(id) ON DELETE CASCADE,
            uploader_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            conversation_id INTEGER NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
            file_name TEXT NOT NULL,
            file_path TEXT NOT NULL,
            file_size INTEGER NOT NULL,
            mime_type TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now'))
        );
        CREATE INDEX IF NOT EXISTS idx_att_msg ON attachments(message_id);
        CREATE INDEX IF NOT EXISTS idx_att_conv ON attachments(conversation_id);
    )", nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

    // user_preferences
    sqlite3_exec(db_, R"(
        CREATE TABLE IF NOT EXISTS user_preferences (
            user_id INTEGER PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
            theme TEXT NOT NULL DEFAULT 'system',
            sound_notifications INTEGER NOT NULL DEFAULT 1,
            desktop_notifications INTEGER NOT NULL DEFAULT 1,
            read_receipts_enabled INTEGER NOT NULL DEFAULT 1,
            updated_at TEXT NOT NULL DEFAULT (datetime('now'))
        );
    )", nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

    sqlite3_exec(db_,
        "INSERT OR IGNORE INTO schema_migrations (version, name) VALUES (4, 'phase4_groups_replies_reactions_attachments_settings');",
        nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }
}

// Statement implementation
Statement::Statement(sqlite3* db, const std::string& sql) {
    if (db) {
        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[Statement] Prepare failed: " << sqlite3_errmsg(db) << " for SQL: " << sql << std::endl;
            stmt_ = nullptr;
        }
    }
}

Statement::~Statement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}

bool Statement::bindText(int index, const std::string& val) {
    if (!stmt_) return false;
    return sqlite3_bind_text(stmt_, index, val.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

bool Statement::bindInt64(int index, int64_t val) {
    if (!stmt_) return false;
    return sqlite3_bind_int64(stmt_, index, val) == SQLITE_OK;
}

bool Statement::bindNull(int index) {
    if (!stmt_) return false;
    return sqlite3_bind_null(stmt_, index) == SQLITE_OK;
}

int Statement::step() {
    if (!stmt_) return SQLITE_ERROR;
    return sqlite3_step(stmt_);
}

bool Statement::reset() {
    if (!stmt_) return false;
    return sqlite3_reset(stmt_) == SQLITE_OK;
}

std::string Statement::getColumnText(int col) {
    if (!stmt_) return "";
    const unsigned char* text = sqlite3_column_text(stmt_, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

int64_t Statement::getColumnInt64(int col) {
    if (!stmt_) return 0;
    return sqlite3_column_int64(stmt_, col);
}

} // namespace connectx
