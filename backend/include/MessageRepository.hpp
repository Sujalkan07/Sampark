#pragma once

#include "Models.hpp"
#include "Database.hpp"
#include <optional>
#include <vector>
#include <string>
#include <utility>

namespace connectx {

class IMessageRepository {
public:
    virtual ~IMessageRepository() = default;

    virtual std::optional<Message> createMessage(int64_t conversationId, int64_t senderId, const std::string& body, std::optional<int64_t> replyToId = std::nullopt) = 0;
    virtual std::vector<Message> getMessages(int64_t conversationId, int limit = 50, int64_t beforeId = 0, int64_t afterId = 0, int64_t currentUserId = 0) = 0;
    virtual std::optional<Message> getLastMessage(int64_t conversationId) = 0;
    virtual int getUnreadCount(int64_t conversationId, int64_t recipientUserId) = 0;
    virtual int markAsRead(int64_t conversationId, int64_t recipientUserId) = 0;

    // Phase 3 extensions
    virtual std::optional<Message> getMessageById(int64_t messageId, int64_t currentUserId = 0) = 0;
    virtual bool updateMessageBody(int64_t messageId, const std::string& newBody) = 0;
    virtual bool deleteMessage(int64_t messageId) = 0;
    virtual std::vector<Message> searchMessages(int64_t conversationId, const std::string& query, int limit = 50, int64_t currentUserId = 0) = 0;
    virtual std::vector<Message> searchAllUserMessages(int64_t userId, const std::string& query, int limit = 50) = 0;

    // Phase 4 extensions: Reactions, Attachments, Preferences
    virtual std::pair<bool, std::string> toggleReaction(int64_t messageId, int64_t userId, const std::string& emoji) = 0;
    virtual std::vector<ReactionSummary> getMessageReactions(int64_t messageId, int64_t currentUserId = 0) = 0;
    virtual std::optional<Attachment> createAttachment(int64_t uploaderId, int64_t conversationId, const std::string& fileName, const std::string& filePath, int64_t fileSize, const std::string& mimeType) = 0;
    virtual bool linkAttachmentToMessage(int64_t attachmentId, int64_t messageId) = 0;
    virtual std::optional<Attachment> getAttachmentById(int64_t attachmentId) = 0;
    virtual std::optional<Attachment> getMessageAttachment(int64_t messageId) = 0;
    virtual bool deleteAttachment(int64_t attachmentId) = 0;
    virtual int cleanupOrphanedAttachments(int maxAgeHours = 24) = 0;
    virtual UserPreferences getUserPreferences(int64_t userId) = 0;
    virtual bool updateUserPreferences(const UserPreferences& prefs) = 0;
};

class SqliteMessageRepository : public IMessageRepository {
public:
    explicit SqliteMessageRepository(Database& db);
    ~SqliteMessageRepository() override = default;

    std::optional<Message> createMessage(int64_t conversationId, int64_t senderId, const std::string& body, std::optional<int64_t> replyToId = std::nullopt) override;
    std::vector<Message> getMessages(int64_t conversationId, int limit = 50, int64_t beforeId = 0, int64_t afterId = 0, int64_t currentUserId = 0) override;
    std::optional<Message> getLastMessage(int64_t conversationId) override;
    int getUnreadCount(int64_t conversationId, int64_t recipientUserId) override;
    int markAsRead(int64_t conversationId, int64_t recipientUserId) override;

    // Phase 3 extensions
    std::optional<Message> getMessageById(int64_t messageId, int64_t currentUserId = 0) override;
    bool updateMessageBody(int64_t messageId, const std::string& newBody) override;
    bool deleteMessage(int64_t messageId) override;
    std::vector<Message> searchMessages(int64_t conversationId, const std::string& query, int limit = 50, int64_t currentUserId = 0) override;
    std::vector<Message> searchAllUserMessages(int64_t userId, const std::string& query, int limit = 50) override;

    // Phase 4 extensions: Reactions, Attachments, Preferences
    std::pair<bool, std::string> toggleReaction(int64_t messageId, int64_t userId, const std::string& emoji) override;
    std::vector<ReactionSummary> getMessageReactions(int64_t messageId, int64_t currentUserId = 0) override;
    std::optional<Attachment> createAttachment(int64_t uploaderId, int64_t conversationId, const std::string& fileName, const std::string& filePath, int64_t fileSize, const std::string& mimeType) override;
    bool linkAttachmentToMessage(int64_t attachmentId, int64_t messageId) override;
    std::optional<Attachment> getAttachmentById(int64_t attachmentId) override;
    std::optional<Attachment> getMessageAttachment(int64_t messageId) override;
    bool deleteAttachment(int64_t attachmentId) override;
    int cleanupOrphanedAttachments(int maxAgeHours = 24) override;
    UserPreferences getUserPreferences(int64_t userId) override;
    bool updateUserPreferences(const UserPreferences& prefs) override;

private:
    Database& db_;
    Message extractMessage(Statement& stmt, int64_t currentUserId = 0);
    std::optional<ReplyPreview> fetchReplyPreview(int64_t replyToId);
};

} // namespace connectx
