#include "MessageRepository.hpp"
#include <algorithm>
#include <iostream>

namespace connectx {

SqliteMessageRepository::SqliteMessageRepository(Database& db) : db_(db) {}

Message SqliteMessageRepository::extractMessage(Statement& stmt) {
    Message m;
    m.id = stmt.getColumnInt64(0);
    m.conversationId = stmt.getColumnInt64(1);
    m.senderId = stmt.getColumnInt64(2);
    m.body = stmt.getColumnText(3);
    m.createdAt = stmt.getColumnText(4);
    std::string readAt = stmt.getColumnText(5);
    if (!readAt.empty()) {
        m.readAt = readAt;
    }
    std::string editedAt = stmt.getColumnText(6);
    if (!editedAt.empty()) {
        m.editedAt = editedAt;
    }
    m.isDeleted = (stmt.getColumnInt64(7) != 0);
    return m;
}

std::optional<Message> SqliteMessageRepository::createMessage(int64_t conversationId, int64_t senderId, const std::string& body) {
    if (conversationId <= 0 || senderId <= 0 || body.empty()) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(db_.getMutex());

    // 1. Insert message
    const std::string insertSql = "INSERT INTO messages (conversation_id, sender_id, body, created_at, is_deleted) "
                                  "VALUES (?, ?, ?, datetime('now'), 0);";
    int64_t newMsgId = 0;
    {
        Statement stmt(db_.getHandle(), insertSql);
        if (!stmt.isValid()) return std::nullopt;

        stmt.bindInt64(1, conversationId);
        stmt.bindInt64(2, senderId);
        stmt.bindText(3, body);
        if (stmt.step() != SQLITE_DONE) {
            return std::nullopt;
        }
        newMsgId = sqlite3_last_insert_rowid(db_.getHandle());
    }

    // 2. Update conversation updated_at
    const std::string updateConvSql = "UPDATE conversations SET updated_at = datetime('now') WHERE id = ?;";
    {
        Statement stmt(db_.getHandle(), updateConvSql);
        if (stmt.isValid()) {
            stmt.bindInt64(1, conversationId);
            stmt.step();
        }
    }

    // 3. Retrieve inserted message
    const std::string selectSql = "SELECT id, conversation_id, sender_id, body, created_at, read_at, edited_at, is_deleted "
                                  "FROM messages WHERE id = ? LIMIT 1;";
    {
        Statement stmt(db_.getHandle(), selectSql);
        if (!stmt.isValid()) return std::nullopt;

        stmt.bindInt64(1, newMsgId);
        if (stmt.step() == SQLITE_ROW) {
            return extractMessage(stmt);
        }
    }

    return std::nullopt;
}

std::vector<Message> SqliteMessageRepository::getMessages(int64_t conversationId, int limit, int64_t beforeId, int64_t afterId) {
    std::vector<Message> msgs;
    if (conversationId <= 0) return msgs;

    if (limit <= 0) limit = 50;
    if (limit > 100) limit = 100;

    std::lock_guard<std::mutex> lock(db_.getMutex());

    if (afterId > 0) {
        // Fetch new messages that occurred after the given cursor (ASC order)
        const std::string sql = "SELECT id, conversation_id, sender_id, body, created_at, read_at, edited_at, is_deleted "
                                "FROM messages WHERE conversation_id = ? AND id > ? "
                                "ORDER BY id ASC LIMIT ?;";
        Statement stmt(db_.getHandle(), sql);
        if (!stmt.isValid()) return msgs;

        stmt.bindInt64(1, conversationId);
        stmt.bindInt64(2, afterId);
        stmt.bindInt64(3, limit);

        while (stmt.step() == SQLITE_ROW) {
            msgs.push_back(extractMessage(stmt));
        }
        return msgs;
    }

    if (beforeId > 0) {
        // Fetch older messages before the given cursor
        const std::string sql = "SELECT id, conversation_id, sender_id, body, created_at, read_at, edited_at, is_deleted "
                                "FROM messages WHERE conversation_id = ? AND id < ? "
                                "ORDER BY id DESC LIMIT ?;";
        Statement stmt(db_.getHandle(), sql);
        if (!stmt.isValid()) return msgs;

        stmt.bindInt64(1, conversationId);
        stmt.bindInt64(2, beforeId);
        stmt.bindInt64(3, limit);

        while (stmt.step() == SQLITE_ROW) {
            msgs.push_back(extractMessage(stmt));
        }
        // Reverse to return in chronological ascending order
        std::reverse(msgs.begin(), msgs.end());
        return msgs;
    }

    // Default: fetch the latest 'limit' messages in ascending order
    const std::string sql = "SELECT id, conversation_id, sender_id, body, created_at, read_at, edited_at, is_deleted "
                            "FROM messages WHERE conversation_id = ? "
                            "ORDER BY id DESC LIMIT ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return msgs;

    stmt.bindInt64(1, conversationId);
    stmt.bindInt64(2, limit);

    while (stmt.step() == SQLITE_ROW) {
        msgs.push_back(extractMessage(stmt));
    }
    std::reverse(msgs.begin(), msgs.end());
    return msgs;
}

std::optional<Message> SqliteMessageRepository::getLastMessage(int64_t conversationId) {
    if (conversationId <= 0) return std::nullopt;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, conversation_id, sender_id, body, created_at, read_at, edited_at, is_deleted "
                            "FROM messages WHERE conversation_id = ? ORDER BY id DESC LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, conversationId);
    if (stmt.step() == SQLITE_ROW) {
        return extractMessage(stmt);
    }
    return std::nullopt;
}

int SqliteMessageRepository::getUnreadCount(int64_t conversationId, int64_t recipientUserId) {
    if (conversationId <= 0 || recipientUserId <= 0) return 0;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT COUNT(*) FROM messages "
                            "WHERE conversation_id = ? AND sender_id != ? AND read_at IS NULL;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return 0;

    stmt.bindInt64(1, conversationId);
    stmt.bindInt64(2, recipientUserId);

    if (stmt.step() == SQLITE_ROW) {
        return static_cast<int>(stmt.getColumnInt64(0));
    }
    return 0;
}

int SqliteMessageRepository::markAsRead(int64_t conversationId, int64_t recipientUserId) {
    if (conversationId <= 0 || recipientUserId <= 0) return 0;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE messages SET read_at = datetime('now') "
                            "WHERE conversation_id = ? AND sender_id != ? AND read_at IS NULL;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return 0;

    stmt.bindInt64(1, conversationId);
    stmt.bindInt64(2, recipientUserId);

    if (stmt.step() == SQLITE_DONE) {
        return sqlite3_changes(db_.getHandle());
    }
    return 0;
}

std::optional<Message> SqliteMessageRepository::getMessageById(int64_t messageId) {
    if (messageId <= 0) return std::nullopt;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, conversation_id, sender_id, body, created_at, read_at, edited_at, is_deleted "
                            "FROM messages WHERE id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, messageId);
    if (stmt.step() == SQLITE_ROW) {
        return extractMessage(stmt);
    }
    return std::nullopt;
}

bool SqliteMessageRepository::updateMessageBody(int64_t messageId, const std::string& newBody) {
    if (messageId <= 0 || newBody.empty()) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE messages SET body = ?, edited_at = datetime('now') WHERE id = ? AND is_deleted = 0;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindText(1, newBody);
    stmt.bindInt64(2, messageId);

    return stmt.step() == SQLITE_DONE;
}

bool SqliteMessageRepository::deleteMessage(int64_t messageId) {
    if (messageId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE messages SET body = 'This message was deleted', is_deleted = 1 WHERE id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, messageId);

    return stmt.step() == SQLITE_DONE;
}

std::vector<Message> SqliteMessageRepository::searchMessages(int64_t conversationId, const std::string& query, int limit) {
    std::vector<Message> msgs;
    if (conversationId <= 0 || query.empty()) return msgs;
    if (limit <= 0) limit = 50;
    if (limit > 100) limit = 100;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, conversation_id, sender_id, body, created_at, read_at, edited_at, is_deleted "
                            "FROM messages WHERE conversation_id = ? AND body LIKE ? AND is_deleted = 0 "
                            "ORDER BY id DESC LIMIT ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return msgs;

    stmt.bindInt64(1, conversationId);
    stmt.bindText(2, "%" + query + "%");
    stmt.bindInt64(3, limit);

    while (stmt.step() == SQLITE_ROW) {
        msgs.push_back(extractMessage(stmt));
    }
    return msgs;
}

std::vector<Message> SqliteMessageRepository::searchAllUserMessages(int64_t userId, const std::string& query, int limit) {
    std::vector<Message> msgs;
    if (userId <= 0 || query.empty()) return msgs;
    if (limit <= 0) limit = 50;
    if (limit > 100) limit = 100;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT m.id, m.conversation_id, m.sender_id, m.body, m.created_at, m.read_at, m.edited_at, m.is_deleted "
                            "FROM messages m "
                            "JOIN conversations c ON m.conversation_id = c.id "
                            "WHERE (c.user_low_id = ? OR c.user_high_id = ?) AND m.body LIKE ? AND m.is_deleted = 0 "
                            "ORDER BY m.id DESC LIMIT ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return msgs;

    stmt.bindInt64(1, userId);
    stmt.bindInt64(2, userId);
    stmt.bindText(3, "%" + query + "%");
    stmt.bindInt64(4, limit);

    while (stmt.step() == SQLITE_ROW) {
        msgs.push_back(extractMessage(stmt));
    }
    return msgs;
}

} // namespace connectx
