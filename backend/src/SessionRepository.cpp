#include "SessionRepository.hpp"
#include <iostream>

namespace connectx {

SqliteSessionRepository::SqliteSessionRepository(Database& db) : db_(db) {}

bool SqliteSessionRepository::createSession(const std::string& token, int64_t userId, int durationHours) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "INSERT INTO sessions (token, user_id, created_at, expires_at) "
                            "VALUES (?, ?, datetime('now'), datetime('now', '+' || ? || ' hours'));";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindText(1, token);
    stmt.bindInt64(2, userId);
    stmt.bindInt64(3, durationHours);

    return stmt.step() == SQLITE_DONE;
}

std::optional<Session> SqliteSessionRepository::findSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT token, user_id, created_at, expires_at "
                            "FROM sessions "
                            "WHERE token = ? AND datetime(expires_at) > datetime('now') LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindText(1, token);
    if (stmt.step() == SQLITE_ROW) {
        Session s;
        s.token = stmt.getColumnText(0);
        s.userId = stmt.getColumnInt64(1);
        s.createdAt = stmt.getColumnText(2);
        s.expiresAt = stmt.getColumnText(3);
        return s;
    }
    return std::nullopt;
}

bool SqliteSessionRepository::deleteSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "DELETE FROM sessions WHERE token = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindText(1, token);
    return stmt.step() == SQLITE_DONE;
}

bool SqliteSessionRepository::deleteUserSessions(int64_t userId) {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "DELETE FROM sessions WHERE user_id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, userId);
    return stmt.step() == SQLITE_DONE;
}

bool SqliteSessionRepository::deleteOtherUserSessions(int64_t userId, const std::string& currentToken) {
    if (userId <= 0) return false;
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "DELETE FROM sessions WHERE user_id = ? AND token != ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, userId);
    stmt.bindText(2, currentToken);
    return stmt.step() == SQLITE_DONE;
}

void SqliteSessionRepository::cleanExpiredSessions() {
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "DELETE FROM sessions WHERE datetime(expires_at) <= datetime('now');";
    Statement stmt(db_.getHandle(), sql);
    if (stmt.isValid()) {
        stmt.step();
    }
}

} // namespace connectx
