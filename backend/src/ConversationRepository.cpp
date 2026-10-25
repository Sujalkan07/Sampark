#include "ConversationRepository.hpp"
#include <algorithm>
#include <iostream>

namespace connectx {

SqliteConversationRepository::SqliteConversationRepository(Database& db) : db_(db) {}

Conversation SqliteConversationRepository::extractConversation(Statement& stmt) {
    Conversation c;
    c.id = stmt.getColumnInt64(0);
    c.userLowId = stmt.getColumnInt64(1);
    c.userHighId = stmt.getColumnInt64(2);
    c.createdAt = stmt.getColumnText(3);
    c.updatedAt = stmt.getColumnText(4);
    return c;
}

std::optional<Conversation> SqliteConversationRepository::getOrCreate(int64_t user1Id, int64_t user2Id) {
    if (user1Id <= 0 || user2Id <= 0 || user1Id == user2Id) {
        return std::nullopt;
    }

    int64_t low = std::min(user1Id, user2Id);
    int64_t high = std::max(user1Id, user2Id);

    std::lock_guard<std::mutex> lock(db_.getMutex());

    // 1. Try to find existing
    const std::string findSql = "SELECT id, user_low_id, user_high_id, created_at, updated_at "
                                "FROM conversations WHERE user_low_id = ? AND user_high_id = ? LIMIT 1;";
    {
        Statement stmt(db_.getHandle(), findSql);
        if (stmt.isValid()) {
            stmt.bindInt64(1, low);
            stmt.bindInt64(2, high);
            if (stmt.step() == SQLITE_ROW) {
                return extractConversation(stmt);
            }
        }
    }

    // 2. Insert new row
    const std::string insertSql = "INSERT OR IGNORE INTO conversations (user_low_id, user_high_id, created_at, updated_at) "
                                  "VALUES (?, ?, datetime('now'), datetime('now'));";
    {
        Statement stmt(db_.getHandle(), insertSql);
        if (!stmt.isValid()) return std::nullopt;

        stmt.bindInt64(1, low);
        stmt.bindInt64(2, high);
        if (stmt.step() != SQLITE_DONE) {
            return std::nullopt;
        }
    }

    // 3. Query back newly created or concurrently inserted row
    {
        Statement stmt(db_.getHandle(), findSql);
        if (!stmt.isValid()) return std::nullopt;

        stmt.bindInt64(1, low);
        stmt.bindInt64(2, high);
        if (stmt.step() == SQLITE_ROW) {
            return extractConversation(stmt);
        }
    }

    return std::nullopt;
}

std::optional<Conversation> SqliteConversationRepository::findById(int64_t conversationId) {
    if (conversationId <= 0) return std::nullopt;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, user_low_id, user_high_id, created_at, updated_at "
                            "FROM conversations WHERE id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, conversationId);
    if (stmt.step() == SQLITE_ROW) {
        return extractConversation(stmt);
    }
    return std::nullopt;
}

std::optional<Conversation> SqliteConversationRepository::findByParticipants(int64_t user1Id, int64_t user2Id) {
    if (user1Id <= 0 || user2Id <= 0 || user1Id == user2Id) {
        return std::nullopt;
    }

    int64_t low = std::min(user1Id, user2Id);
    int64_t high = std::max(user1Id, user2Id);

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, user_low_id, user_high_id, created_at, updated_at "
                            "FROM conversations WHERE user_low_id = ? AND user_high_id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, low);
    stmt.bindInt64(2, high);
    if (stmt.step() == SQLITE_ROW) {
        return extractConversation(stmt);
    }
    return std::nullopt;
}

std::vector<Conversation> SqliteConversationRepository::getUserConversations(int64_t userId) {
    std::vector<Conversation> list;
    if (userId <= 0) return list;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, user_low_id, user_high_id, created_at, updated_at "
                            "FROM conversations WHERE user_low_id = ? OR user_high_id = ? "
                            "ORDER BY updated_at DESC, id DESC;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return list;

    stmt.bindInt64(1, userId);
    stmt.bindInt64(2, userId);

    while (stmt.step() == SQLITE_ROW) {
        list.push_back(extractConversation(stmt));
    }
    return list;
}

bool SqliteConversationRepository::isParticipant(int64_t conversationId, int64_t userId) {
    if (conversationId <= 0 || userId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT 1 FROM conversations WHERE id = ? AND (user_low_id = ? OR user_high_id = ?) LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, conversationId);
    stmt.bindInt64(2, userId);
    stmt.bindInt64(3, userId);

    return stmt.step() == SQLITE_ROW;
}

int64_t SqliteConversationRepository::getOtherParticipantId(int64_t conversationId, int64_t userId) {
    auto conv = findById(conversationId);
    if (!conv) return 0;
    if (conv->userLowId == userId) return conv->userHighId;
    if (conv->userHighId == userId) return conv->userLowId;
    return 0;
}

bool SqliteConversationRepository::updateTimestamp(int64_t conversationId) {
    if (conversationId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE conversations SET updated_at = datetime('now') WHERE id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, conversationId);
    return stmt.step() == SQLITE_DONE;
}

} // namespace connectx
