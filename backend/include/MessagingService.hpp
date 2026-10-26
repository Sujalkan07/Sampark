#pragma once

#include "ConversationRepository.hpp"
#include "MessageRepository.hpp"
#include "UserRepository.hpp"
#include "SessionRepository.hpp"
#include "Models.hpp"
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace connectx {

struct MessagingResult {
    bool success = false;
    int statusCode = 200;
    std::string message;
    std::optional<Conversation> conversation;
    std::optional<Message> chatMessage;
    std::vector<Conversation> conversations;
    std::vector<Message> messages;
    std::vector<ConversationMember> members;
    std::vector<ReactionSummary> reactions;
    std::string reactionAction;
    std::optional<Attachment> attachment;
    std::optional<UserPreferences> preferences;
    std::string fileContent;
    std::string fileMimeType;
    std::string fileName;
};

class IMessagingService {
public:
    virtual ~IMessagingService() = default;

    virtual MessagingResult createOrGetConversation(const std::string& sessionToken, int64_t targetUserId) = 0;
    virtual MessagingResult listConversations(const std::string& sessionToken) = 0;
    virtual MessagingResult getConversation(const std::string& sessionToken, int64_t conversationId) = 0;
    virtual MessagingResult getMessages(const std::string& sessionToken, int64_t conversationId, int limit, int64_t beforeId, int64_t afterId) = 0;
    virtual MessagingResult sendMessage(const std::string& sessionToken, int64_t conversationId, const std::string& body,
                                        std::optional<int64_t> replyToId = std::nullopt,
                                        std::optional<int64_t> attachmentId = std::nullopt) = 0;
    virtual MessagingResult markAsRead(const std::string& sessionToken, int64_t conversationId) = 0;

    // Phase 3 extensions
    virtual MessagingResult editMessage(const std::string& sessionToken, int64_t messageId, const std::string& newBody) = 0;
    virtual MessagingResult deleteMessage(const std::string& sessionToken, int64_t messageId) = 0;
    virtual MessagingResult searchConversationMessages(const std::string& sessionToken, int64_t conversationId, const std::string& query, int limit = 50) = 0;
    virtual MessagingResult searchGlobalMessages(const std::string& sessionToken, const std::string& query, int limit = 50) = 0;

    // Phase 4: Group Chat Extensions
    virtual MessagingResult createGroup(const std::string& sessionToken,
                                        const std::string& name,
                                        const std::string& description,
                                        const std::string& avatarUrl,
                                        const std::vector<int64_t>& memberIds) = 0;
    virtual MessagingResult getGroupMembers(const std::string& sessionToken, int64_t conversationId) = 0;
    virtual MessagingResult addGroupMember(const std::string& sessionToken, int64_t conversationId, int64_t targetUserId) = 0;
    virtual MessagingResult removeGroupMember(const std::string& sessionToken, int64_t conversationId, int64_t targetUserId) = 0;
    virtual MessagingResult updateGroupMemberRole(const std::string& sessionToken, int64_t conversationId, int64_t targetUserId, const std::string& role) = 0;
    virtual MessagingResult updateGroup(const std::string& sessionToken, int64_t conversationId,
                                        const std::string& name, const std::string& description, const std::string& avatarUrl) = 0;
    virtual MessagingResult deleteGroup(const std::string& sessionToken, int64_t conversationId) = 0;
    virtual MessagingResult leaveGroup(const std::string& sessionToken, int64_t conversationId) = 0;

    // Phase 4: Reactions
    virtual MessagingResult toggleReaction(const std::string& sessionToken, int64_t messageId, const std::string& emoji) = 0;

    // Phase 4: Attachments
    virtual MessagingResult uploadAttachment(const std::string& sessionToken, int64_t conversationId,
                                             const std::string& fileName, const std::string& rawBytes, const std::string& mimeType) = 0;
    virtual MessagingResult getAttachment(const std::string& sessionToken, int64_t attachmentId) = 0;
    virtual MessagingResult deleteAttachment(const std::string& sessionToken, int64_t attachmentId) = 0;
    virtual int cleanupOrphanedAttachments(int maxAgeHours = 24) = 0;

    // Phase 4: Preferences
    virtual MessagingResult getUserPreferences(const std::string& sessionToken) = 0;
    virtual MessagingResult updateUserPreferences(const std::string& sessionToken, const UserPreferences& prefs) = 0;
};

class MessagingService : public IMessagingService {
public:
    MessagingService(std::shared_ptr<IConversationRepository> convRepo,
                     std::shared_ptr<IMessageRepository> msgRepo,
                     std::shared_ptr<IUserRepository> userRepo,
                     std::shared_ptr<ISessionRepository> sessionRepo);

    MessagingResult createOrGetConversation(const std::string& sessionToken, int64_t targetUserId) override;
    MessagingResult listConversations(const std::string& sessionToken) override;
    MessagingResult getConversation(const std::string& sessionToken, int64_t conversationId) override;
    MessagingResult getMessages(const std::string& sessionToken, int64_t conversationId, int limit, int64_t beforeId, int64_t afterId) override;
    MessagingResult sendMessage(const std::string& sessionToken, int64_t conversationId, const std::string& body,
                                std::optional<int64_t> replyToId = std::nullopt,
                                std::optional<int64_t> attachmentId = std::nullopt) override;
    MessagingResult markAsRead(const std::string& sessionToken, int64_t conversationId) override;

    // Phase 3 extensions
    MessagingResult editMessage(const std::string& sessionToken, int64_t messageId, const std::string& newBody) override;
    MessagingResult deleteMessage(const std::string& sessionToken, int64_t messageId) override;
    MessagingResult searchConversationMessages(const std::string& sessionToken, int64_t conversationId, const std::string& query, int limit = 50) override;
    MessagingResult searchGlobalMessages(const std::string& sessionToken, const std::string& query, int limit = 50) override;

    // Phase 4: Group Chat Extensions
    MessagingResult createGroup(const std::string& sessionToken,
                                const std::string& name,
                                const std::string& description,
                                const std::string& avatarUrl,
                                const std::vector<int64_t>& memberIds) override;
    MessagingResult getGroupMembers(const std::string& sessionToken, int64_t conversationId) override;
    MessagingResult addGroupMember(const std::string& sessionToken, int64_t conversationId, int64_t targetUserId) override;
    MessagingResult removeGroupMember(const std::string& sessionToken, int64_t conversationId, int64_t targetUserId) override;
    MessagingResult updateGroupMemberRole(const std::string& sessionToken, int64_t conversationId, int64_t targetUserId, const std::string& role) override;
    MessagingResult updateGroup(const std::string& sessionToken, int64_t conversationId,
                                const std::string& name, const std::string& description, const std::string& avatarUrl) override;
    MessagingResult deleteGroup(const std::string& sessionToken, int64_t conversationId) override;
    MessagingResult leaveGroup(const std::string& sessionToken, int64_t conversationId) override;

    // Phase 4: Reactions
    MessagingResult toggleReaction(const std::string& sessionToken, int64_t messageId, const std::string& emoji) override;

    // Phase 4: Attachments
    MessagingResult uploadAttachment(const std::string& sessionToken, int64_t conversationId,
                                     const std::string& fileName, const std::string& rawBytes, const std::string& mimeType) override;
    MessagingResult getAttachment(const std::string& sessionToken, int64_t attachmentId) override;
    MessagingResult deleteAttachment(const std::string& sessionToken, int64_t attachmentId) override;
    int cleanupOrphanedAttachments(int maxAgeHours = 24) override;

    // Phase 4: Preferences
    MessagingResult getUserPreferences(const std::string& sessionToken) override;
    MessagingResult updateUserPreferences(const std::string& sessionToken, const UserPreferences& prefs) override;

private:
    std::shared_ptr<IConversationRepository> convRepo_;
    std::shared_ptr<IMessageRepository> msgRepo_;
    std::shared_ptr<IUserRepository> userRepo_;
    std::shared_ptr<ISessionRepository> sessionRepo_;

    std::optional<int64_t> authenticate(const std::string& token);
    void enrichConversation(Conversation& conv, int64_t currentUserId);
};

} // namespace connectx
