#include "ConversationRepository.hpp"
#include <algorithm>
#include <iostream>

namespace connectx {

SqliteConversationRepository::SqliteConversationRepository(Database& db) : db_(db) {}

Conversation SqliteConversationRepository::extractConversation(Statement& stmt) {
    Conversation c;
    c.id = stmt.getColumnInt64(0);
    c.type = stmt.getColumnText(1);
    if (c.type.empty()) c.type = "direct";
    c.name = stmt.getColumnText(2);
    c.description = stmt.getColumnText(3);
    c.avatarUrl = stmt.getColumnText(4);
    int64_t creatorId = stmt.getColumnInt64(5);
    if (creatorId > 0) {
        c.createdBy = creatorId;
    }
    c.userLowId = stmt.getColumnInt64(6);
    c.userHighId = stmt.getColumnInt64(7);
    c.createdAt = stmt.getColumnText(8);
    c.updatedAt = stmt.getColumnText(9);
    return c;
}

std::optional<Conversation> SqliteConversationRepository::getOrCreate(int64_t user1Id, int64_t user2Id) {
    if (user1Id <= 0 || user2Id <= 0 || user1Id == user2Id) {
        return std::nullopt;
    }

    int64_t low = std::min(user1Id, user2Id);
    int64_t high = std::max(user1Id, user2Id);

    std::lock_guard<std::mutex> lock(db_.getMutex());

    // 1. Try to find existing direct conversation
    const std::string findSql = "SELECT id, type, name, description, avatar_url, created_by, user_low_id, user_high_id, created_at, updated_at "
                                "FROM conversations WHERE type = 'direct' AND user_low_id = ? AND user_high_id = ? LIMIT 1;";
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

    // 2. Insert new direct conversation row
    const std::string insertSql = "INSERT INTO conversations (type, name, description, avatar_url, created_by, user_low_id, user_high_id, created_at, updated_at) "
                                  "VALUES ('direct', '', '', '', NULL, ?, ?, datetime('now'), datetime('now'));";
    {
        Statement stmt(db_.getHandle(), insertSql);
        if (!stmt.isValid()) return std::nullopt;

        stmt.bindInt64(1, low);
        stmt.bindInt64(2, high);
        if (stmt.step() != SQLITE_DONE) {
            return std::nullopt;
        }
    }

    // 3. Query back newly created row
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
    const std::string sql = "SELECT id, type, name, description, avatar_url, created_by, user_low_id, user_high_id, created_at, updated_at "
                            "FROM conversations WHERE id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, conversationId);
    if (stmt.step() == SQLITE_ROW) {
        Conversation conv = extractConversation(stmt);
        if (conv.type == "group") {
            // Fetch group members
            const std::string memSql = "SELECT cm.id, cm.conversation_id, cm.user_id, cm.role, cm.joined_at, cm.last_read_message_id, "
                                       "u.username, u.full_name, u.bio, u.last_active_at, u.created_at "
                                       "FROM conversation_members cm "
                                       "JOIN users u ON cm.user_id = u.id "
                                       "WHERE cm.conversation_id = ? "
                                       "ORDER BY CASE WHEN cm.role = 'admin' THEN 0 ELSE 1 END, u.full_name ASC;";
            Statement memStmt(db_.getHandle(), memSql);
            if (memStmt.isValid()) {
                memStmt.bindInt64(1, conversationId);
                while (memStmt.step() == SQLITE_ROW) {
                    ConversationMember m;
                    m.id = memStmt.getColumnInt64(0);
                    m.conversationId = memStmt.getColumnInt64(1);
                    m.userId = memStmt.getColumnInt64(2);
                    m.role = memStmt.getColumnText(3);
                    m.joinedAt = memStmt.getColumnText(4);
                    m.lastReadMessageId = memStmt.getColumnInt64(5);

                    User u;
                    u.id = m.userId;
                    u.username = memStmt.getColumnText(6);
                    u.fullName = memStmt.getColumnText(7);
                    u.bio = memStmt.getColumnText(8);
                    u.lastActiveAt = memStmt.getColumnText(9);
                    u.createdAt = memStmt.getColumnText(10);
                    m.user = u;

                    conv.members.push_back(m);
                }
            }
        }
        return conv;
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
    const std::string sql = "SELECT id, type, name, description, avatar_url, created_by, user_low_id, user_high_id, created_at, updated_at "
                            "FROM conversations WHERE type = 'direct' AND user_low_id = ? AND user_high_id = ? LIMIT 1;";
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
    // Direct conversations where user is low/high OR group conversations where user is in conversation_members
    const std::string sql = "SELECT id, type, name, description, avatar_url, created_by, user_low_id, user_high_id, created_at, updated_at "
                            "FROM conversations "
                            "WHERE (type = 'direct' AND (user_low_id = ? OR user_high_id = ?)) "
                            "   OR (type = 'group' AND id IN (SELECT conversation_id FROM conversation_members WHERE user_id = ?)) "
                            "ORDER BY updated_at DESC, id DESC;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return list;

    stmt.bindInt64(1, userId);
    stmt.bindInt64(2, userId);
    stmt.bindInt64(3, userId);

    while (stmt.step() == SQLITE_ROW) {
        Conversation conv = extractConversation(stmt);
        if (conv.type == "group") {
            const std::string memSql = "SELECT cm.id, cm.conversation_id, cm.user_id, cm.role, cm.joined_at, cm.last_read_message_id, "
                                       "u.username, u.full_name, u.bio, u.last_active_at, u.created_at "
                                       "FROM conversation_members cm "
                                       "JOIN users u ON cm.user_id = u.id "
                                       "WHERE cm.conversation_id = ? "
                                       "ORDER BY CASE WHEN cm.role = 'admin' THEN 0 ELSE 1 END, u.full_name ASC;";
            Statement memStmt(db_.getHandle(), memSql);
            if (memStmt.isValid()) {
                memStmt.bindInt64(1, conv.id);
                while (memStmt.step() == SQLITE_ROW) {
                    ConversationMember m;
                    m.id = memStmt.getColumnInt64(0);
                    m.conversationId = memStmt.getColumnInt64(1);
                    m.userId = memStmt.getColumnInt64(2);
                    m.role = memStmt.getColumnText(3);
                    m.joinedAt = memStmt.getColumnText(4);
                    m.lastReadMessageId = memStmt.getColumnInt64(5);

                    User u;
                    u.id = m.userId;
                    u.username = memStmt.getColumnText(6);
                    u.fullName = memStmt.getColumnText(7);
                    u.bio = memStmt.getColumnText(8);
                    u.lastActiveAt = memStmt.getColumnText(9);
                    u.createdAt = memStmt.getColumnText(10);
                    m.user = u;

                    conv.members.push_back(m);
                }
            }
        }
        list.push_back(conv);
    }
    return list;
}

bool SqliteConversationRepository::isParticipant(int64_t conversationId, int64_t userId) {
    if (conversationId <= 0 || userId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string checkTypeSql = "SELECT type, user_low_id, user_high_id FROM conversations WHERE id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), checkTypeSql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, conversationId);
    if (stmt.step() != SQLITE_ROW) return false;

    std::string type = stmt.getColumnText(0);
    if (type.empty()) type = "direct";

    if (type == "direct") {
        int64_t low = stmt.getColumnInt64(1);
        int64_t high = stmt.getColumnInt64(2);
        return (low == userId || high == userId);
    } else {
        // Check conversation_members
        const std::string memSql = "SELECT 1 FROM conversation_members WHERE conversation_id = ? AND user_id = ? LIMIT 1;";
        Statement memStmt(db_.getHandle(), memSql);
        if (!memStmt.isValid()) return false;
        memStmt.bindInt64(1, conversationId);
        memStmt.bindInt64(2, userId);
        return memStmt.step() == SQLITE_ROW;
    }
}

int64_t SqliteConversationRepository::getOtherParticipantId(int64_t conversationId, int64_t userId) {
    auto conv = findById(conversationId);
    if (!conv) return 0;
    if (conv->type == "direct") {
        if (conv->userLowId == userId) return conv->userHighId;
        if (conv->userHighId == userId) return conv->userLowId;
    }
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

// Phase 4: Group Chat Implementation
std::optional<Conversation> SqliteConversationRepository::createGroup(const std::string& name,
                                                                      const std::string& description,
                                                                      const std::string& avatarUrl,
                                                                      int64_t createdBy,
                                                                      const std::vector<int64_t>& memberIds) {
    if (name.empty() || createdBy <= 0) return std::nullopt;

    std::lock_guard<std::mutex> lock(db_.getMutex());

    // Begin immediate transaction
    char* errMsg = nullptr;
    sqlite3_exec(db_.getHandle(), "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errMsg);
    if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

    const std::string insertGroupSql = "INSERT INTO conversations (type, name, description, avatar_url, created_by, user_low_id, user_high_id, created_at, updated_at) "
                                       "VALUES ('group', ?, ?, ?, ?, NULL, NULL, datetime('now'), datetime('now'));";
    int64_t convId = 0;
    {
        Statement stmt(db_.getHandle(), insertGroupSql);
        if (!stmt.isValid()) {
            sqlite3_exec(db_.getHandle(), "ROLLBACK;", nullptr, nullptr, nullptr);
            return std::nullopt;
        }
        stmt.bindText(1, name);
        stmt.bindText(2, description);
        stmt.bindText(3, avatarUrl);
        stmt.bindInt64(4, createdBy);

        if (stmt.step() != SQLITE_DONE) {
            sqlite3_exec(db_.getHandle(), "ROLLBACK;", nullptr, nullptr, nullptr);
            return std::nullopt;
        }
        convId = sqlite3_last_insert_rowid(db_.getHandle());
    }

    if (convId <= 0) {
        sqlite3_exec(db_.getHandle(), "ROLLBACK;", nullptr, nullptr, nullptr);
        return std::nullopt;
    }

    // Add creator as admin
    const std::string insertMemberSql = "INSERT OR IGNORE INTO conversation_members (conversation_id, user_id, role, joined_at) "
                                        "VALUES (?, ?, ?, datetime('now'));";
    {
        Statement stmt(db_.getHandle(), insertMemberSql);
        if (stmt.isValid()) {
            stmt.bindInt64(1, convId);
            stmt.bindInt64(2, createdBy);
            stmt.bindText(3, "admin");
            stmt.step();
        }
    }

    // Add selected members as regular members
    for (int64_t memberId : memberIds) {
        if (memberId > 0 && memberId != createdBy) {
            Statement stmt(db_.getHandle(), insertMemberSql);
            if (stmt.isValid()) {
                stmt.bindInt64(1, convId);
                stmt.bindInt64(2, memberId);
                stmt.bindText(3, "member");
                stmt.step();
            }
        }
    }

    sqlite3_exec(db_.getHandle(), "COMMIT;", nullptr, nullptr, nullptr);

    // Retrieve created conversation
    const std::string findSql = "SELECT id, type, name, description, avatar_url, created_by, user_low_id, user_high_id, created_at, updated_at "
                                "FROM conversations WHERE id = ? LIMIT 1;";
    Statement findStmt(db_.getHandle(), findSql);
    if (!findStmt.isValid()) return std::nullopt;

    findStmt.bindInt64(1, convId);
    if (findStmt.step() == SQLITE_ROW) {
        Conversation conv = extractConversation(findStmt);
        // Load members
        const std::string memSql = "SELECT cm.id, cm.conversation_id, cm.user_id, cm.role, cm.joined_at, cm.last_read_message_id, "
                                   "u.username, u.full_name, u.bio, u.last_active_at, u.created_at "
                                   "FROM conversation_members cm "
                                   "JOIN users u ON cm.user_id = u.id "
                                   "WHERE cm.conversation_id = ? "
                                   "ORDER BY CASE WHEN cm.role = 'admin' THEN 0 ELSE 1 END, u.full_name ASC;";
        Statement memStmt(db_.getHandle(), memSql);
        if (memStmt.isValid()) {
            memStmt.bindInt64(1, convId);
            while (memStmt.step() == SQLITE_ROW) {
                ConversationMember m;
                m.id = memStmt.getColumnInt64(0);
                m.conversationId = memStmt.getColumnInt64(1);
                m.userId = memStmt.getColumnInt64(2);
                m.role = memStmt.getColumnText(3);
                m.joinedAt = memStmt.getColumnText(4);
                m.lastReadMessageId = memStmt.getColumnInt64(5);

                User u;
                u.id = m.userId;
                u.username = memStmt.getColumnText(6);
                u.fullName = memStmt.getColumnText(7);
                u.bio = memStmt.getColumnText(8);
                u.lastActiveAt = memStmt.getColumnText(9);
                u.createdAt = memStmt.getColumnText(10);
                m.user = u;

                conv.members.push_back(m);
            }
        }
        return conv;
    }

    return std::nullopt;
}

std::vector<ConversationMember> SqliteConversationRepository::getGroupMembers(int64_t conversationId) {
    std::vector<ConversationMember> members;
    if (conversationId <= 0) return members;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string memSql = "SELECT cm.id, cm.conversation_id, cm.user_id, cm.role, cm.joined_at, cm.last_read_message_id, "
                               "u.username, u.full_name, u.bio, u.last_active_at, u.created_at "
                               "FROM conversation_members cm "
                               "JOIN users u ON cm.user_id = u.id "
                               "WHERE cm.conversation_id = ? "
                               "ORDER BY CASE WHEN cm.role = 'admin' THEN 0 ELSE 1 END, cm.joined_at ASC, cm.id ASC;";
    Statement memStmt(db_.getHandle(), memSql);
    if (!memStmt.isValid()) return members;

    memStmt.bindInt64(1, conversationId);
    while (memStmt.step() == SQLITE_ROW) {
        ConversationMember m;
        m.id = memStmt.getColumnInt64(0);
        m.conversationId = memStmt.getColumnInt64(1);
        m.userId = memStmt.getColumnInt64(2);
        m.role = memStmt.getColumnText(3);
        m.joinedAt = memStmt.getColumnText(4);
        m.lastReadMessageId = memStmt.getColumnInt64(5);

        User u;
        u.id = m.userId;
        u.username = memStmt.getColumnText(6);
        u.fullName = memStmt.getColumnText(7);
        u.bio = memStmt.getColumnText(8);
        u.lastActiveAt = memStmt.getColumnText(9);
        u.createdAt = memStmt.getColumnText(10);
        m.user = u;

        members.push_back(m);
    }
    return members;
}

bool SqliteConversationRepository::addMember(int64_t conversationId, int64_t userId, const std::string& role) {
    if (conversationId <= 0 || userId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "INSERT OR IGNORE INTO conversation_members (conversation_id, user_id, role, joined_at) "
                            "VALUES (?, ?, ?, datetime('now'));";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, conversationId);
    stmt.bindInt64(2, userId);
    stmt.bindText(3, role.empty() ? "member" : role);
    return stmt.step() == SQLITE_DONE;
}

bool SqliteConversationRepository::removeMember(int64_t conversationId, int64_t userId) {
    if (conversationId <= 0 || userId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "DELETE FROM conversation_members WHERE conversation_id = ? AND user_id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, conversationId);
    stmt.bindInt64(2, userId);
    return stmt.step() == SQLITE_DONE;
}

bool SqliteConversationRepository::updateGroup(int64_t conversationId, const std::string& name, const std::string& description, const std::string& avatarUrl) {
    if (conversationId <= 0 || name.empty()) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE conversations SET name = ?, description = ?, avatar_url = ?, updated_at = datetime('now') "
                            "WHERE id = ? AND type = 'group';";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindText(1, name);
    stmt.bindText(2, description);
    stmt.bindText(3, avatarUrl);
    stmt.bindInt64(4, conversationId);
    return stmt.step() == SQLITE_DONE;
}

bool SqliteConversationRepository::deleteGroup(int64_t conversationId) {
    if (conversationId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "DELETE FROM conversations WHERE id = ? AND type = 'group';";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;

    stmt.bindInt64(1, conversationId);
    return stmt.step() == SQLITE_DONE;
}

std::optional<std::string> SqliteConversationRepository::getMemberRole(int64_t conversationId, int64_t userId) {
    if (conversationId <= 0 || userId <= 0) return std::nullopt;

    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "SELECT role FROM conversation_members WHERE conversation_id = ? AND user_id = ? LIMIT 1;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return std::nullopt;

    stmt.bindInt64(1, conversationId);
    stmt.bindInt64(2, userId);
    if (stmt.step() == SQLITE_ROW) {
        return stmt.getColumnText(0);
    }
    return std::nullopt;
}

bool SqliteConversationRepository::transferAdmin(int64_t conversationId, int64_t currentAdminId, int64_t newAdminId) {
    if (conversationId <= 0 || newAdminId <= 0) return false;

    std::lock_guard<std::mutex> lock(db_.getMutex());

    // Verify newAdminId is a member
    {
        const std::string checkSql = "SELECT 1 FROM conversation_members WHERE conversation_id = ? AND user_id = ? LIMIT 1;";
        Statement checkStmt(db_.getHandle(), checkSql);
        if (!checkStmt.isValid()) return false;
        checkStmt.bindInt64(1, conversationId);
        checkStmt.bindInt64(2, newAdminId);
        if (checkStmt.step() != SQLITE_ROW) {
            return false;
        }
    }

    char* errMsg = nullptr;
    sqlite3_exec(db_.getHandle(), "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errMsg);

    // Update current admin to member
    int rc1 = SQLITE_OK;
    if (currentAdminId > 0) {
        const std::string demoteSql = "UPDATE conversation_members SET role = 'member' WHERE conversation_id = ? AND user_id = ?;";
        Statement stmt(db_.getHandle(), demoteSql);
        if (stmt.isValid()) {
            stmt.bindInt64(1, conversationId);
            stmt.bindInt64(2, currentAdminId);
            rc1 = stmt.step();
        } else {
            rc1 = SQLITE_ERROR;
        }
    }

    // Promote new admin
    int rc2 = SQLITE_OK;
    const std::string promoteSql = "UPDATE conversation_members SET role = 'admin' WHERE conversation_id = ? AND user_id = ?;";
    {
        Statement stmt(db_.getHandle(), promoteSql);
        if (stmt.isValid()) {
            stmt.bindInt64(1, conversationId);
            stmt.bindInt64(2, newAdminId);
            rc2 = stmt.step();
        } else {
            rc2 = SQLITE_ERROR;
        }
    }

    // Update created_by in conversations table
    int rc3 = SQLITE_OK;
    const std::string updateConvSql = "UPDATE conversations SET created_by = ?, updated_at = datetime('now') WHERE id = ?;";
    {
        Statement stmt(db_.getHandle(), updateConvSql);
        if (stmt.isValid()) {
            stmt.bindInt64(1, newAdminId);
            stmt.bindInt64(2, conversationId);
            rc3 = stmt.step();
        } else {
            rc3 = SQLITE_ERROR;
        }
    }

    if ((rc1 == SQLITE_DONE || rc1 == SQLITE_OK) && rc2 == SQLITE_DONE && rc3 == SQLITE_DONE) {
        sqlite3_exec(db_.getHandle(), "COMMIT;", nullptr, nullptr, nullptr);
        return true;
    } else {
        sqlite3_exec(db_.getHandle(), "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
}

bool SqliteConversationRepository::updateLastReadMessage(int64_t conversationId, int64_t userId) {
    if (conversationId <= 0 || userId <= 0) return false;
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE conversation_members "
                            "SET last_read_message_id = (SELECT COALESCE(MAX(id), 0) FROM messages WHERE conversation_id = ?) "
                            "WHERE conversation_id = ? AND user_id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;
    stmt.bindInt64(1, conversationId);
    stmt.bindInt64(2, conversationId);
    stmt.bindInt64(3, userId);
    return stmt.step() == SQLITE_DONE;
}

bool SqliteConversationRepository::updateMemberRole(int64_t conversationId, int64_t userId, const std::string& newRole) {
    if (conversationId <= 0 || userId <= 0) return false;
    if (newRole != "admin" && newRole != "member") return false;
    std::lock_guard<std::mutex> lock(db_.getMutex());
    const std::string sql = "UPDATE conversation_members SET role = ? WHERE conversation_id = ? AND user_id = ?;";
    Statement stmt(db_.getHandle(), sql);
    if (!stmt.isValid()) return false;
    stmt.bindText(1, newRole);
    stmt.bindInt64(2, conversationId);
    stmt.bindInt64(3, userId);
    return stmt.step() == SQLITE_DONE;
}

} // namespace connectx
