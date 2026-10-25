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
            user_low_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            user_high_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at TEXT NOT NULL DEFAULT (datetime('now')),
            CONSTRAINT chk_diff_users CHECK (user_low_id < user_high_id),
            CONSTRAINT uq_participants UNIQUE (user_low_id, user_high_id)
        );

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

        CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
        CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
        CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(token);
        CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions(user_id);
        CREATE INDEX IF NOT EXISTS idx_conv_user_low ON conversations(user_low_id);
        CREATE INDEX IF NOT EXISTS idx_conv_user_high ON conversations(user_high_id);
        CREATE INDEX IF NOT EXISTS idx_conv_updated_at ON conversations(updated_at DESC);
        CREATE INDEX IF NOT EXISTS idx_msg_conv_id ON messages(conversation_id);
        CREATE INDEX IF NOT EXISTS idx_msg_conv_created ON messages(conversation_id, id ASC);
        CREATE INDEX IF NOT EXISTS idx_msg_sender ON messages(sender_id);
        CREATE INDEX IF NOT EXISTS idx_msg_unread ON messages(conversation_id, sender_id, read_at);
        CREATE INDEX IF NOT EXISTS idx_msg_search ON messages(conversation_id, body);
    )";

    bool ok = execute(schemaSql);
    if (!ok) return false;

    migrateSchema();
    return true;
}

void Database::migrateSchema() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return;

    char* errMsg = nullptr;
    sqlite3_exec(db_, "ALTER TABLE users ADD COLUMN bio TEXT DEFAULT '';", nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

    sqlite3_exec(db_, "ALTER TABLE users ADD COLUMN last_active_at TEXT DEFAULT '';", nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

    sqlite3_exec(db_, "ALTER TABLE messages ADD COLUMN edited_at TEXT DEFAULT NULL;", nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

    sqlite3_exec(db_, "ALTER TABLE messages ADD COLUMN is_deleted INTEGER NOT NULL DEFAULT 0;", nullptr, nullptr, &errMsg);
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
