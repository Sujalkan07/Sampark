#pragma once

#include "Database.hpp"
#include "Models.hpp"
#include <optional>
#include <vector>
#include <string>

namespace connectx {

class IMessageRepository {
public:
    virtual ~IMessageRepository() = default;

    virtual std::optional<Message> createMessage(int64_t conversationId, int64_t senderId, const std::string& body) = 0;
    virtual std::vector<Message> getMessages(int64_t conversationId, int limit = 50, int64_t beforeId = 0, int64_t afterId = 0) = 0;
    virtual std::optional<Message> getLastMessage(int64_t conversationId) = 0;
    virtual int getUnreadCount(int64_t conversationId, int64_t recipientUserId) = 0;
    virtual int markAsRead(int64_t conversationId, int64_t recipientUserId) = 0;
};

class SqliteMessageRepository : public IMessageRepository {
public:
    explicit SqliteMessageRepository(Database& db);

    std::optional<Message> createMessage(int64_t conversationId, int64_t senderId, const std::string& body) override;
    std::vector<Message> getMessages(int64_t conversationId, int limit = 50, int64_t beforeId = 0, int64_t afterId = 0) override;
    std::optional<Message> getLastMessage(int64_t conversationId) override;
    int getUnreadCount(int64_t conversationId, int64_t recipientUserId) override;
    int markAsRead(int64_t conversationId, int64_t recipientUserId) override;

private:
    Database& db_;
    static Message extractMessage(Statement& stmt);
};

} // namespace connectx
