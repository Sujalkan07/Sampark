#pragma once

#include "Models.hpp"
#include "Database.hpp"
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

    // Phase 3 extensions
    virtual std::optional<Message> getMessageById(int64_t messageId) = 0;
    virtual bool updateMessageBody(int64_t messageId, const std::string& newBody) = 0;
    virtual bool deleteMessage(int64_t messageId) = 0;
    virtual std::vector<Message> searchMessages(int64_t conversationId, const std::string& query, int limit = 50) = 0;
    virtual std::vector<Message> searchAllUserMessages(int64_t userId, const std::string& query, int limit = 50) = 0;
};

class SqliteMessageRepository : public IMessageRepository {
public:
    explicit SqliteMessageRepository(Database& db);
    ~SqliteMessageRepository() override = default;

    std::optional<Message> createMessage(int64_t conversationId, int64_t senderId, const std::string& body) override;
    std::vector<Message> getMessages(int64_t conversationId, int limit = 50, int64_t beforeId = 0, int64_t afterId = 0) override;
    std::optional<Message> getLastMessage(int64_t conversationId) override;
    int getUnreadCount(int64_t conversationId, int64_t recipientUserId) override;
    int markAsRead(int64_t conversationId, int64_t recipientUserId) override;

    // Phase 3 extensions
    std::optional<Message> getMessageById(int64_t messageId) override;
    bool updateMessageBody(int64_t messageId, const std::string& newBody) override;
    bool deleteMessage(int64_t messageId) override;
    std::vector<Message> searchMessages(int64_t conversationId, const std::string& query, int limit = 50) override;
    std::vector<Message> searchAllUserMessages(int64_t userId, const std::string& query, int limit = 50) override;

private:
    Database& db_;
    Message extractMessage(Statement& stmt);
};

} // namespace connectx
