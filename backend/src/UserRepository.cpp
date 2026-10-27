#include "UserRepository.hpp"
#include <iostream>

namespace connectx {

SqliteUserRepository::SqliteUserRepository(Database& db) : db_(db) {}

User SqliteUserRepository::extractUser(Statement& stmt) {
    User u;
    u.id = stmt.getColumnInt64(0);
    u.username = stmt.getColumnText(1);
    u.email = stmt.getColumnText(2);
    u.fullName = stmt.getColumnText(3);
    u.passwordHash = stmt.getColumnText(4);
    u.salt = stmt.getColumnText(5);
    u.createdAt = stmt.getColumnText(6);
    u.updatedAt = stmt.getColumnText(7);
    u.bio = stmt.getColumnText(8);
    u.lastActiveAt = stmt.getColumnText(9);
    return u;
}

std::optional<User> SqliteUserRepository::findById(int64_t id) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, username, email, full_name, password_hash, salt, created_at, updated_at, bio, last_active_at "
                            "FROM users WHERE id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, id);
    if (stmt.step() == SQLITE_ROW) {
        return extractUser(stmt);
    }
    return std::nullopt;
}

std::optional<User> SqliteUserRepository::findByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, username, email, full_name, password_hash, salt, created_at, updated_at, bio, last_active_at "
                            "FROM users WHERE username = ? COLLATE NOCASE LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindText(1, username);
    if (stmt.step() == SQLITE_ROW) {
        return extractUser(stmt);
    }
    return std::nullopt;
}

std::optional<User> SqliteUserRepository::findByEmail(const std::string& email) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, username, email, full_name, password_hash, salt, created_at, updated_at, bio, last_active_at "
                            "FROM users WHERE email = ? COLLATE NOCASE LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindText(1, email);
    if (stmt.step() == SQLITE_ROW) {
        return extractUser(stmt);
    }
    return std::nullopt;
}

std::optional<User> SqliteUserRepository::findByUsernameOrEmail(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, username, email, full_name, password_hash, salt, created_at, updated_at, bio, last_active_at "
                            "FROM users WHERE username = ? COLLATE NOCASE OR email = ? COLLATE NOCASE LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindText(1, identifier);
    stmt.bindText(2, identifier);
    if (stmt.step() == SQLITE_ROW) {
        return extractUser(stmt);
    }
    return std::nullopt;
}

bool SqliteUserRepository::usernameExists(const std::string& username) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT 1 FROM users WHERE username = ? COLLATE NOCASE LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindText(1, username);
    return stmt.step() == SQLITE_ROW;
}

bool SqliteUserRepository::emailExists(const std::string& email) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT 1 FROM users WHERE email = ? COLLATE NOCASE LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindText(1, email);
    return stmt.step() == SQLITE_ROW;
}

std::optional<User> SqliteUserRepository::create(const std::string& username,
                                                  const std::string& email,
                                                  const std::string& fullName,
                                                  const std::string& passwordHash,
                                                  const std::string& salt) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "INSERT INTO users (username, email, full_name, password_hash, salt, created_at, updated_at, bio, last_active_at) "
                            "VALUES (?, ?, ?, ?, ?, datetime('now'), datetime('now'), '', datetime('now'));";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindText(1, username);
    stmt.bindText(2, email);
    stmt.bindText(3, fullName);
    stmt.bindText(4, passwordHash);
    stmt.bindText(5, salt);

    if (stmt.step() != SQLITE_DONE) {
        std::cerr << "[UserRepository] Failed to insert user: " << sqlite3_errmsg(db_.getHandle()) << std::endl;
        return std::nullopt;
    }

    int64_t newId = sqlite3_last_insert_rowid(db_.getHandle());

    // Fetch the newly inserted user
    const std::string selectSql = "SELECT id, username, email, full_name, password_hash, salt, created_at, updated_at, bio, last_active_at "
                                  "FROM users WHERE id = ? LIMIT 1;";
    Statement selectStmt(db_.getHandle(), selectSql);
    if (selectStmt.isValid()) {
        selectStmt.bindInt64(1, newId);
        if (selectStmt.step() == SQLITE_ROW) {
            return extractUser(selectStmt);
        }
    }

    return std::nullopt;
}

std::vector<User> SqliteUserRepository::searchUsers(const std::string& query, int64_t excludeUserId, int limit) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    std::vector<User> users;

    if (limit <= 0) limit = 50;
    if (limit > 100) limit = 100;

    std::string sql;
    bool hasQuery = !query.empty();

    if (hasQuery) {
        sql = "SELECT id, username, '' as email, full_name, '' as password_hash, '' as salt, created_at, updated_at, bio, last_active_at "
              "FROM users "
              "WHERE id != ? AND (username LIKE ? ESCAPE '\\' OR full_name LIKE ? ESCAPE '\\') "
              "ORDER BY username COLLATE NOCASE ASC LIMIT ?;";
    } else {
        sql = "SELECT id, username, '' as email, full_name, '' as password_hash, '' as salt, created_at, updated_at, bio, last_active_at "
              "FROM users "
              "WHERE id != ? "
              "ORDER BY created_at DESC LIMIT ?;";
    }

    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return users;

    stmt.bindInt64(1, excludeUserId);

    if (hasQuery) {
        std::string escaped;
        for (char c : query) {
            if (c == '%' || c == '_' || c == '\\') {
                escaped += '\\';
            }
            escaped += c;
        }
        std::string pattern = "%" + escaped + "%";
        stmt.bindText(2, pattern);
        stmt.bindText(3, pattern);
        stmt.bindInt64(4, limit);
    } else {
        stmt.bindInt64(2, limit);
    }

    while (stmt.step() == SQLITE_ROW) {
        users.push_back(extractUser(stmt));
    }

    return users;
}

bool SqliteUserRepository::updateProfile(int64_t userId, const std::string& fullName, const std::string& bio) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE users SET full_name = ?, bio = ?, updated_at = datetime('now') WHERE id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindText(1, fullName);
    stmt.bindText(2, bio);
    stmt.bindInt64(3, userId);

    return stmt.step() == SQLITE_DONE;
}

bool SqliteUserRepository::updatePassword(int64_t userId, const std::string& newHash, const std::string& newSalt) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE users SET password_hash = ?, salt = ?, updated_at = datetime('now') WHERE id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindText(1, newHash);
    stmt.bindText(2, newSalt);
    stmt.bindInt64(3, userId);

    return stmt.step() == SQLITE_DONE;
}

bool SqliteUserRepository::updateLastActive(int64_t userId) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE users SET last_active_at = datetime('now') WHERE id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, userId);
    return stmt.step() == SQLITE_DONE;
}

// Phase 5: Contacts
bool SqliteUserRepository::addContact(int64_t userId, int64_t contactUserId) {
    if (userId <= 0 || contactUserId <= 0 || userId == contactUserId) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "INSERT OR IGNORE INTO user_contacts (user_id, contact_user_id, created_at) "
                            "VALUES (?, ?, datetime('now'));";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, userId);
    stmt.bindInt64(2, contactUserId);

    return stmt.step() == SQLITE_DONE;
}

bool SqliteUserRepository::removeContact(int64_t userId, int64_t contactUserId) {
    if (userId <= 0 || contactUserId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "DELETE FROM user_contacts WHERE user_id = ? AND contact_user_id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, userId);
    stmt.bindInt64(2, contactUserId);

    return stmt.step() == SQLITE_DONE;
}

bool SqliteUserRepository::isContact(int64_t userId, int64_t contactUserId) {
    if (userId <= 0 || contactUserId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id FROM user_contacts WHERE user_id = ? AND contact_user_id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, userId);
    stmt.bindInt64(2, contactUserId);

    return stmt.step() == SQLITE_ROW;
}

std::vector<User> SqliteUserRepository::getContacts(int64_t userId) {
    std::vector<User> contacts;
    if (userId <= 0) return contacts;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT u.id, u.username, u.email, u.full_name, u.password_hash, u.salt, u.created_at, u.updated_at, u.bio, u.last_active_at "
                            "FROM users u "
                            "JOIN user_contacts uc ON u.id = uc.contact_user_id "
                            "WHERE uc.user_id = ? "
                            "ORDER BY u.full_name COLLATE NOCASE ASC;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return contacts;

    stmt.bindInt64(1, userId);

    while (stmt.step() == SQLITE_ROW) {
        contacts.push_back(extractUser(stmt));
    }
    return contacts;
}

} // namespace connectx
