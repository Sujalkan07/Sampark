#include "MessageRepository.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace connectx {

namespace fs = std::filesystem;

SqliteMessageRepository::SqliteMessageRepository(Database& db) : db_(db) {}

static std::string escapeLikePattern(const std::string& input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (char c : input) {
        if (c == '%' || c == '_' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

std::optional<ReplyPreview> SqliteMessageRepository::fetchReplyPreview(int64_t replyToId) {
    if (replyToId <= 0) return std::nullopt;

    const std::string sql = "SELECT m.id, m.sender_id, u.username, u.full_name, m.body, m.is_deleted "
                            "FROM messages m "
                            "LEFT JOIN users u ON m.sender_id = u.id "
                            "WHERE m.id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, replyToId);
    if (stmt.step() == SQLITE_ROW) {
        ReplyPreview r;
        r.id = stmt.getColumnInt64(0);
        r.senderId = stmt.getColumnInt64(1);
        r.senderUsername = stmt.getColumnText(2);
        r.senderName = stmt.getColumnText(3);
        r.isDeleted = (stmt.getColumnInt64(5) != 0);
        if (r.isDeleted) {
            r.body = "This message was deleted";
        } else {
            r.body = stmt.getColumnText(4);
            // Snip to 120 chars if longer for a clean preview
            if (r.body.size() > 120) {
                r.body = r.body.substr(0, 117) + "...";
            }
        }
        return r;
    }
    return std::nullopt;
}

Message SqliteMessageRepository::extractMessage(Statement& stmt, int64_t currentUserId) {
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

    int64_t replyToId = stmt.getColumnInt64(8);
    if (replyToId > 0) {
        m.replyToId = replyToId;
        m.replyTo = fetchReplyPreview(replyToId);
    }

    m.senderUsername = stmt.getColumnText(9);
    m.senderName = stmt.getColumnText(10);

    // Fetch reactions for this message
    const std::string rxSql = "SELECT mr.emoji, COUNT(mr.id) AS cnt, "
                              "MAX(CASE WHEN mr.user_id = ? THEN 1 ELSE 0 END) AS user_reacted, "
                              "GROUP_CONCAT(u.username, ', ') AS users_list "
                              "FROM message_reactions mr "
                              "JOIN users u ON mr.user_id = u.id "
                              "WHERE mr.message_id = ? "
                              "GROUP BY mr.emoji "
                              "ORDER BY cnt DESC, mr.emoji ASC;";
    Statement rxStmt(db_.getHandle(), rxSql);
    if (rxStmt.isValid()) {
        rxStmt.bindInt64(1, currentUserId);
        rxStmt.bindInt64(2, m.id);
        while (rxStmt.step() == SQLITE_ROW) {
            ReactionSummary s;
            s.emoji = rxStmt.getColumnText(0);
            s.count = static_cast<int>(rxStmt.getColumnInt64(1));
            s.userReacted = (rxStmt.getColumnInt64(2) != 0);
            std::string userStr = rxStmt.getColumnText(3);
            if (!userStr.empty()) {
                std::stringstream ss(userStr);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    size_t start = item.find_first_not_of(" \t");
                    size_t end = item.find_last_not_of(" \t");
                    if (start != std::string::npos && end != std::string::npos) {
                        s.users.push_back(item.substr(start, end - start + 1));
                    }
                }
            }
            m.reactions.push_back(s);
        }
    }

    // Fetch attachment for this message (only if message is not deleted)
    if (!m.isDeleted) {
        const std::string attSql = "SELECT id, message_id, uploader_id, conversation_id, file_name, file_path, file_size, mime_type, created_at "
                                   "FROM attachments WHERE message_id = ? LIMIT 1;";
        Statement attStmt(db_.getHandle(), attSql);
        if (attStmt.isValid()) {
            attStmt.bindInt64(1, m.id);
            if (attStmt.step() == SQLITE_ROW) {
                Attachment att;
                att.id = attStmt.getColumnInt64(0);
                att.messageId = m.id;
                att.uploaderId = attStmt.getColumnInt64(2);
                att.conversationId = attStmt.getColumnInt64(3);
                att.fileName = attStmt.getColumnText(4);
                att.filePath = attStmt.getColumnText(5);
                att.fileSize = attStmt.getColumnInt64(6);
                att.mimeType = attStmt.getColumnText(7);
                att.createdAt = attStmt.getColumnText(8);
                m.attachment = att;
            }
        }
    }

    return m;
}

std::optional<Message> SqliteMessageRepository::createMessage(int64_t conversationId, int64_t senderId, const std::string& body, std::optional<int64_t> replyToId) {
    if (conversationId <= 0 || senderId <= 0 || body.empty()) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(db_.getMutex());

    // If replyToId provided, verify it belongs to the same conversation
    if (replyToId.has_value() && *replyToId > 0) {
        const std::string checkSql = "SELECT conversation_id FROM messages WHERE id = ? LIMIT 1;";
        Statement checkStmt(db_.getHandle(), checkSql);
        if (!checkStmt.isValid()) return std::nullopt;
        checkStmt.bindInt64(1, *replyToId);
        if (checkStmt.step() != SQLITE_ROW || checkStmt.getColumnInt64(0) != conversationId) {
            return std::nullopt; // Cross-conversation reply rejection!
        }
    }

    // 1. Insert message
    const std::string insertSql = "INSERT INTO messages (conversation_id, sender_id, body, created_at, is_deleted, reply_to_id) "
                                  "VALUES (?, ?, ?, datetime('now'), 0, ?);";
    int64_t newMsgId = 0;
    {
        Statement stmt(db_.getHandle(), insertSql);
        if (!stmt.isValid()) return std::nullopt;

        stmt.bindInt64(1, conversationId);
        stmt.bindInt64(2, senderId);
        stmt.bindText(3, body);
        if (replyToId.has_value() && *replyToId > 0) {
            stmt.bindInt64(4, *replyToId);
        } else {
            stmt.bindNull(4);
        }

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
    const std::string selectSql = "SELECT m.id, m.conversation_id, m.sender_id, m.body, m.created_at, m.read_at, m.edited_at, m.is_deleted, m.reply_to_id, "
                                  "u.username, u.full_name "
                                  "FROM messages m "
                                  "LEFT JOIN users u ON m.sender_id = u.id "
                                  "WHERE m.id = ? LIMIT 1;";
    {
        Statement stmt(db_.getHandle(), selectSql);
        if (!stmt.isValid()) return std::nullopt;

        stmt.bindInt64(1, newMsgId);
        if (stmt.step() == SQLITE_ROW) {
            return extractMessage(stmt, senderId);
        }
    }

    return std::nullopt;
}

std::vector<Message> SqliteMessageRepository::getMessages(int64_t conversationId, int limit, int64_t beforeId, int64_t afterId, int64_t currentUserId) {
    std::vector<Message> msgs;
    if (conversationId <= 0) return msgs;

    if (limit <= 0) limit = 50;
    if (limit > 100) limit = 100;

    std::lock_guard<std::mutex> lock(db_.getMutex());

    if (afterId > 0) {
        const std::string sql = "SELECT m.id, m.conversation_id, m.sender_id, m.body, m.created_at, m.read_at, m.edited_at, m.is_deleted, m.reply_to_id, "
                                "u.username, u.full_name "
                                "FROM messages m "
                                "LEFT JOIN users u ON m.sender_id = u.id "
                                "WHERE m.conversation_id = ? AND m.id > ? "
                                "ORDER BY m.id ASC LIMIT ?;";
        Statement stmt(db_.getHandle(), sql);
        if (!stmt.isValid()) return msgs;

        stmt.bindInt64(1, conversationId);
        stmt.bindInt64(2, afterId);
        stmt.bindInt64(3, limit);

        while (stmt.step() == SQLITE_ROW) {
            msgs.push_back(extractMessage(stmt, currentUserId));
        }
        return msgs;
    }

    if (beforeId > 0) {
        const std::string sql = "SELECT m.id, m.conversation_id, m.sender_id, m.body, m.created_at, m.read_at, m.edited_at, m.is_deleted, m.reply_to_id, "
                                "u.username, u.full_name "
                                "FROM messages m "
                                "LEFT JOIN users u ON m.sender_id = u.id "
                                "WHERE m.conversation_id = ? AND m.id < ? "
                                "ORDER BY m.id DESC LIMIT ?;";
        Statement stmt(db_.getHandle(), sql);
        if (!stmt.isValid()) return msgs;

        stmt.bindInt64(1, conversationId);
        stmt.bindInt64(2, beforeId);
        stmt.bindInt64(3, limit);

        while (stmt.step() == SQLITE_ROW) {
            msgs.push_back(extractMessage(stmt, currentUserId));
        }
        std::reverse(msgs.begin(), msgs.end());
        return msgs;
    }

    // Default: latest 'limit' messages in ascending order
    const std::string sql = "SELECT m.id, m.conversation_id, m.sender_id, m.body, m.created_at, m.read_at, m.edited_at, m.is_deleted, m.reply_to_id, "
                            "u.username, u.full_name "
                            "FROM messages m "
                            "LEFT JOIN users u ON m.sender_id = u.id "
                            "WHERE m.conversation_id = ? "
                            "ORDER BY m.id DESC LIMIT ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return msgs;

    stmt.bindInt64(1, conversationId);
    stmt.bindInt64(2, limit);

    while (stmt.step() == SQLITE_ROW) {
        msgs.push_back(extractMessage(stmt, currentUserId));
    }
    std::reverse(msgs.begin(), msgs.end());
    return msgs;
}

std::optional<Message> SqliteMessageRepository::getLastMessage(int64_t conversationId) {
    if (conversationId <= 0) return std::nullopt;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT m.id, m.conversation_id, m.sender_id, m.body, m.created_at, m.read_at, m.edited_at, m.is_deleted, m.reply_to_id, "
                            "u.username, u.full_name "
                            "FROM messages m "
                            "LEFT JOIN users u ON m.sender_id = u.id "
                            "WHERE m.conversation_id = ? ORDER BY m.id DESC LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, conversationId);
    if (stmt.step() == SQLITE_ROW) {
        return extractMessage(stmt, 0);
    }
    return std::nullopt;
}

int SqliteMessageRepository::getUnreadCount(int64_t conversationId, int64_t recipientUserId) {
    if (conversationId <= 0 || recipientUserId <= 0) return 0;

    std::lock_guard<std::mutex> lock(db_.getMutex());

    // Check if conversation is group
    const std::string typeSql = "SELECT type FROM conversations WHERE id = ? LIMIT 1;";
    Statement typeStmt(db_.getHandle(), typeSql);
    std::string type = "direct";
    if (typeStmt.isValid()) {
        typeStmt.bindInt64(1, conversationId);
        if (typeStmt.step() == SQLITE_ROW) {
            type = typeStmt.getColumnText(0);
        }
    }

    if (type == "group") {
        const std::string groupSql = "SELECT COUNT(*) FROM messages "
                                     "WHERE conversation_id = ? AND sender_id != ? "
                                     "  AND id > (SELECT COALESCE(last_read_message_id, 0) FROM conversation_members WHERE conversation_id = ? AND user_id = ?);";
        Statement stmt(db_.getHandle(), groupSql);
        if (!stmt.isValid()) return 0;
        stmt.bindInt64(1, conversationId);
        stmt.bindInt64(2, recipientUserId);
        stmt.bindInt64(3, conversationId);
        stmt.bindInt64(4, recipientUserId);
        if (stmt.step() == SQLITE_ROW) {
            return static_cast<int>(stmt.getColumnInt64(0));
        }
        return 0;
    } else {
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

std::optional<Message> SqliteMessageRepository::getMessageById(int64_t messageId, int64_t currentUserId) {
    if (messageId <= 0) return std::nullopt;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT m.id, m.conversation_id, m.sender_id, m.body, m.created_at, m.read_at, m.edited_at, m.is_deleted, m.reply_to_id, "
                            "u.username, u.full_name "
                            "FROM messages m "
                            "LEFT JOIN users u ON m.sender_id = u.id "
                            "WHERE m.id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, messageId);
    if (stmt.step() == SQLITE_ROW) {
        return extractMessage(stmt, currentUserId);
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

std::vector<Message> SqliteMessageRepository::searchMessages(int64_t conversationId, const std::string& query, int limit, int64_t currentUserId) {
    std::vector<Message> msgs;
    if (conversationId <= 0 || query.empty()) return msgs;
    if (limit <= 0) limit = 50;
    if (limit > 100) limit = 100;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT m.id, m.conversation_id, m.sender_id, m.body, m.created_at, m.read_at, m.edited_at, m.is_deleted, m.reply_to_id, "
                            "u.username, u.full_name "
                            "FROM messages m "
                            "LEFT JOIN users u ON m.sender_id = u.id "
                            "WHERE m.conversation_id = ? AND m.body LIKE ? ESCAPE '\\' AND m.is_deleted = 0 "
                            "ORDER BY m.id DESC LIMIT ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return msgs;

    stmt.bindInt64(1, conversationId);
    stmt.bindText(2, "%" + escapeLikePattern(query) + "%");
    stmt.bindInt64(3, limit);

    while (stmt.step() == SQLITE_ROW) {
        msgs.push_back(extractMessage(stmt, currentUserId));
    }
    return msgs;
}

std::vector<Message> SqliteMessageRepository::searchAllUserMessages(int64_t userId, const std::string& query, int limit) {
    std::vector<Message> msgs;
    if (userId <= 0 || query.empty()) return msgs;
    if (limit <= 0) limit = 50;
    if (limit > 100) limit = 100;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT m.id, m.conversation_id, m.sender_id, m.body, m.created_at, m.read_at, m.edited_at, m.is_deleted, m.reply_to_id, "
                            "u.username, u.full_name "
                            "FROM messages m "
                            "JOIN conversations c ON m.conversation_id = c.id "
                            "LEFT JOIN users u ON m.sender_id = u.id "
                            "WHERE ((c.type = 'direct' AND (c.user_low_id = ? OR c.user_high_id = ?)) "
                            "    OR (c.type = 'group' AND c.id IN (SELECT conversation_id FROM conversation_members WHERE user_id = ?))) "
                            "  AND m.body LIKE ? ESCAPE '\\' AND m.is_deleted = 0 "
                            "ORDER BY m.id DESC LIMIT ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return msgs;

    stmt.bindInt64(1, userId);
    stmt.bindInt64(2, userId);
    stmt.bindInt64(3, userId);
    stmt.bindText(4, "%" + escapeLikePattern(query) + "%");
    stmt.bindInt64(5, limit);

    while (stmt.step() == SQLITE_ROW) {
        msgs.push_back(extractMessage(stmt, userId));
    }
    return msgs;
}

// Phase 4: Reactions
std::pair<bool, std::string> SqliteMessageRepository::toggleReaction(int64_t messageId, int64_t userId, const std::string& emoji) {
    if (messageId <= 0 || userId <= 0 || emoji.empty()) {
        return {false, "invalid_params"};
    }

    std::lock_guard<std::mutex> lock(db_.getMutex());

    // Check if reaction already exists
    const std::string checkSql = "SELECT id FROM message_reactions WHERE message_id = ? AND user_id = ? AND emoji = ? LIMIT 1;";
    int64_t existingId = 0;
    {
        Statement stmt(db_.getHandle(), checkSql);
        if (stmt.isValid()) {
            stmt.bindInt64(1, messageId);
            stmt.bindInt64(2, userId);
            stmt.bindText(3, emoji);
            if (stmt.step() == SQLITE_ROW) {
                existingId = stmt.getColumnInt64(0);
            }
        }
    }

    if (existingId > 0) {
        const std::string delSql = "DELETE FROM message_reactions WHERE id = ?;";
        Statement stmt(db_.getHandle(), delSql);
        if (stmt.isValid()) {
            stmt.bindInt64(1, existingId);
            if (stmt.step() == SQLITE_DONE) {
                return {true, "removed"};
            }
        }
    } else {
        const std::string insSql = "INSERT OR IGNORE INTO message_reactions (message_id, user_id, emoji, created_at) "
                                   "VALUES (?, ?, ?, datetime('now'));";
        Statement stmt(db_.getHandle(), insSql);
        if (stmt.isValid()) {
            stmt.bindInt64(1, messageId);
            stmt.bindInt64(2, userId);
            stmt.bindText(3, emoji);
            if (stmt.step() == SQLITE_DONE) {
                return {true, "added"};
            }
        }
    }
    return {false, "error"};
}

std::vector<ReactionSummary> SqliteMessageRepository::getMessageReactions(int64_t messageId, int64_t currentUserId) {
    std::vector<ReactionSummary> summaries;
    if (messageId <= 0) return summaries;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string rxSql = "SELECT mr.emoji, COUNT(mr.id) AS cnt, "
                              "MAX(CASE WHEN mr.user_id = ? THEN 1 ELSE 0 END) AS user_reacted, "
                              "GROUP_CONCAT(u.username, ', ') AS users_list "
                              "FROM message_reactions mr "
                              "JOIN users u ON mr.user_id = u.id "
                              "WHERE mr.message_id = ? "
                              "GROUP BY mr.emoji "
                              "ORDER BY cnt DESC, mr.emoji ASC;";
    Statement rxStmt(db_.getHandle(), rxSql);
    if (!rxStmt.isValid()) return summaries;

    rxStmt.bindInt64(1, currentUserId);
    rxStmt.bindInt64(2, messageId);
    while (rxStmt.step() == SQLITE_ROW) {
        ReactionSummary s;
        s.emoji = rxStmt.getColumnText(0);
        s.count = static_cast<int>(rxStmt.getColumnInt64(1));
        s.userReacted = (rxStmt.getColumnInt64(2) != 0);
        std::string userStr = rxStmt.getColumnText(3);
        if (!userStr.empty()) {
            std::stringstream ss(userStr);
            std::string item;
            while (std::getline(ss, item, ',')) {
                size_t start = item.find_first_not_of(" \t");
                size_t end = item.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos) {
                    s.users.push_back(item.substr(start, end - start + 1));
                }
            }
        }
        summaries.push_back(s);
    }
    return summaries;
}

// Phase 4: Attachments
std::optional<Attachment> SqliteMessageRepository::createAttachment(int64_t uploaderId, int64_t conversationId, const std::string& fileName, const std::string& filePath, int64_t fileSize, const std::string& mimeType) {
    if (uploaderId <= 0 || conversationId <= 0 || fileName.empty() || filePath.empty()) return std::nullopt;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "INSERT INTO attachments (message_id, uploader_id, conversation_id, file_name, file_path, file_size, mime_type, created_at) "
                            "VALUES (NULL, ?, ?, ?, ?, ?, ?, datetime('now'));";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, uploaderId);
    stmt.bindInt64(2, conversationId);
    stmt.bindText(3, fileName);
    stmt.bindText(4, filePath);
    stmt.bindInt64(5, fileSize);
    stmt.bindText(6, mimeType);

    if (stmt.step() != SQLITE_DONE) return std::nullopt;

    int64_t attId = sqlite3_last_insert_rowid(db_.getHandle());
    Attachment att;
    att.id = attId;
    att.uploaderId = uploaderId;
    att.conversationId = conversationId;
    att.fileName = fileName;
    att.filePath = filePath;
    att.fileSize = fileSize;
    att.mimeType = mimeType;
    return att;
}

bool SqliteMessageRepository::linkAttachmentToMessage(int64_t attachmentId, int64_t messageId) {
    if (attachmentId <= 0 || messageId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE attachments SET message_id = ? WHERE id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, messageId);
    stmt.bindInt64(2, attachmentId);
    return stmt.step() == SQLITE_DONE;
}

std::optional<Attachment> SqliteMessageRepository::getAttachmentById(int64_t attachmentId) {
    if (attachmentId <= 0) return std::nullopt;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, message_id, uploader_id, conversation_id, file_name, file_path, file_size, mime_type, created_at "
                            "FROM attachments WHERE id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, attachmentId);
    if (stmt.step() == SQLITE_ROW) {
        Attachment att;
        att.id = stmt.getColumnInt64(0);
        int64_t msgId = stmt.getColumnInt64(1);
        if (msgId > 0) att.messageId = msgId;
        att.uploaderId = stmt.getColumnInt64(2);
        att.conversationId = stmt.getColumnInt64(3);
        att.fileName = stmt.getColumnText(4);
        att.filePath = stmt.getColumnText(5);
        att.fileSize = stmt.getColumnInt64(6);
        att.mimeType = stmt.getColumnText(7);
        att.createdAt = stmt.getColumnText(8);
        return att;
    }
    return std::nullopt;
}

std::optional<Attachment> SqliteMessageRepository::getMessageAttachment(int64_t messageId) {
    if (messageId <= 0) return std::nullopt;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT id, message_id, uploader_id, conversation_id, file_name, file_path, file_size, mime_type, created_at "
                            "FROM attachments WHERE message_id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, messageId);
    if (stmt.step() == SQLITE_ROW) {
        Attachment att;
        att.id = stmt.getColumnInt64(0);
        att.messageId = messageId;
        att.uploaderId = stmt.getColumnInt64(2);
        att.conversationId = stmt.getColumnInt64(3);
        att.fileName = stmt.getColumnText(4);
        att.filePath = stmt.getColumnText(5);
        att.fileSize = stmt.getColumnInt64(6);
        att.mimeType = stmt.getColumnText(7);
        att.createdAt = stmt.getColumnText(8);
        return att;
    }
    return std::nullopt;
}

bool SqliteMessageRepository::deleteAttachment(int64_t attachmentId) {
    if (attachmentId <= 0) return false;
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "DELETE FROM attachments WHERE id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;
    stmt.bindInt64(1, attachmentId);
    return stmt.step() == SQLITE_DONE;
}

int SqliteMessageRepository::cleanupOrphanedAttachments(int maxAgeHours) {
    std::vector<std::pair<int64_t, std::string>> toDelete;
    {
        std::lock_guard<std::mutex> lock(db_.getMutex());
        const std::string sql = "SELECT id, file_path FROM attachments "
                                "WHERE message_id IS NULL AND created_at < datetime('now', '-' || ? || ' hours');";
        Statement stmt(db_.getHandle(), sql);
        if (stmt.isValid()) {
            stmt.bindInt64(1, maxAgeHours);
            while (stmt.step() == SQLITE_ROW) {
                toDelete.emplace_back(stmt.getColumnInt64(0), stmt.getColumnText(1));
            }
        }
    }

    int count = 0;
    for (const auto& [id, path] : toDelete) {
        try {
            if (fs::exists(path)) {
                fs::remove(path);
            }
        } catch (...) {}
        deleteAttachment(id);
        count++;
    }
    return count;
}

// Phase 4: User Preferences
UserPreferences SqliteMessageRepository::getUserPreferences(int64_t userId) {
    UserPreferences prefs;
    prefs.userId = userId;
    if (userId <= 0) return prefs;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT theme, sound_notifications, desktop_notifications, read_receipts_enabled, updated_at "
                            "FROM user_preferences WHERE user_id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (stmt.isValid()) {
        stmt.bindInt64(1, userId);
        if (stmt.step() == SQLITE_ROW) {
            prefs.theme = stmt.getColumnText(0);
            prefs.soundNotifications = (stmt.getColumnInt64(1) != 0);
            prefs.desktopNotifications = (stmt.getColumnInt64(2) != 0);
            prefs.readReceiptsEnabled = (stmt.getColumnInt64(3) != 0);
            prefs.updatedAt = stmt.getColumnText(4);
            return prefs;
        }
    }

    // Insert default preferences if not yet existing
    const std::string insSql = "INSERT OR IGNORE INTO user_preferences (user_id, theme, sound_notifications, desktop_notifications, read_receipts_enabled, updated_at) "
                               "VALUES (?, 'system', 1, 1, 1, datetime('now'));";
    Statement insStmt(db_.getHandle(), insSql);
    if (insStmt.isValid()) {
        insStmt.bindInt64(1, userId);
        insStmt.step();
    }
    return prefs;
}

bool SqliteMessageRepository::updateUserPreferences(const UserPreferences& prefs) {
    if (prefs.userId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "INSERT INTO user_preferences (user_id, theme, sound_notifications, desktop_notifications, read_receipts_enabled, updated_at) "
                            "VALUES (?, ?, ?, ?, ?, datetime('now')) "
                            "ON CONFLICT(user_id) DO UPDATE SET "
                            "theme = excluded.theme, "
                            "sound_notifications = excluded.sound_notifications, "
                            "desktop_notifications = excluded.desktop_notifications, "
                            "read_receipts_enabled = excluded.read_receipts_enabled, "
                            "updated_at = datetime('now');";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, prefs.userId);
    stmt.bindText(2, prefs.theme.empty() ? "system" : prefs.theme);
    stmt.bindInt64(3, prefs.soundNotifications ? 1 : 0);
    stmt.bindInt64(4, prefs.desktopNotifications ? 1 : 0);
    stmt.bindInt64(5, prefs.readReceiptsEnabled ? 1 : 0);
    return stmt.step() == SQLITE_DONE;
}

} // namespace connectx
