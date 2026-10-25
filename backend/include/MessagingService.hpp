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
};

class IMessagingService {
public:
    virtual ~IMessagingService() = default;

    virtual MessagingResult createOrGetConversation(const std::string& sessionToken, int64_t targetUserId) = 0;
    virtual MessagingResult listConversations(const std::string& sessionToken) = 0;
    virtual MessagingResult getConversation(const std::string& sessionToken, int64_t conversationId) = 0;
    virtual MessagingResult getMessages(const std::string& sessionToken, int64_t conversationId, int limit, int64_t beforeId, int64_t afterId) = 0;
    virtual MessagingResult sendMessage(const std::string& sessionToken, int64_t conversationId, const std::string& body) = 0;
    virtual MessagingResult markAsRead(const std::string& sessionToken, int64_t conversationId) = 0;
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
    MessagingResult sendMessage(const std::string& sessionToken, int64_t conversationId, const std::string& body) override;
    MessagingResult markAsRead(const std::string& sessionToken, int64_t conversationId) override;

private:
    std::shared_ptr<IConversationRepository> convRepo_;
    std::shared_ptr<IMessageRepository> msgRepo_;
    std::shared_ptr<IUserRepository> userRepo_;
    std::shared_ptr<ISessionRepository> sessionRepo_;

    std::optional<int64_t> authenticate(const std::string& token);
    void enrichConversation(Conversation& conv, int64_t currentUserId);
};

} // namespace connectx
