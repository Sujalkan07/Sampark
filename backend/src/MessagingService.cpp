#include "MessagingService.hpp"
#include "Validator.hpp"
#include <iostream>

namespace connectx {

MessagingService::MessagingService(std::shared_ptr<IConversationRepository> convRepo,
                                   std::shared_ptr<IMessageRepository> msgRepo,
                                   std::shared_ptr<IUserRepository> userRepo,
                                   std::shared_ptr<ISessionRepository> sessionRepo)
    : convRepo_(std::move(convRepo)),
      msgRepo_(std::move(msgRepo)),
      userRepo_(std::move(userRepo)),
      sessionRepo_(std::move(sessionRepo)) {}

std::optional<int64_t> MessagingService::authenticate(const std::string& token) {
    if (token.empty()) return std::nullopt;
    auto session = sessionRepo_->findSession(token);
    if (!session) return std::nullopt;
    return session->userId;
}

void MessagingService::enrichConversation(Conversation& conv, int64_t currentUserId) {
    int64_t otherId = (conv.userLowId == currentUserId) ? conv.userHighId : conv.userLowId;
    auto otherUser = userRepo_->findById(otherId);
    if (otherUser) {
        conv.otherParticipant = otherUser;
    }
    conv.lastMessage = msgRepo_->getLastMessage(conv.id);
    conv.unreadCount = msgRepo_->getUnreadCount(conv.id, currentUserId);
}

MessagingResult MessagingService::createOrGetConversation(const std::string& sessionToken, int64_t targetUserId) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    if (*callerId == targetUserId) {
        res.success = false;
        res.statusCode = 400;
        res.message = "You cannot start a conversation with yourself.";
        return res;
    }

    auto targetUser = userRepo_->findById(targetUserId);
    if (!targetUser) {
        res.success = false;
        res.statusCode = 404;
        res.message = "The selected user does not exist.";
        return res;
    }

    auto conv = convRepo_->getOrCreate(*callerId, targetUserId);
    if (!conv) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to create or retrieve conversation.";
        return res;
    }

    enrichConversation(*conv, *callerId);

    res.success = true;
    res.statusCode = 200;
    res.conversation = conv;
    return res;
}

MessagingResult MessagingService::listConversations(const std::string& sessionToken) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    auto convs = convRepo_->getUserConversations(*callerId);
    for (auto& c : convs) {
        enrichConversation(c, *callerId);
    }

    res.success = true;
    res.statusCode = 200;
    res.conversations = std::move(convs);
    return res;
}

MessagingResult MessagingService::getConversation(const std::string& sessionToken, int64_t conversationId) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    auto conv = convRepo_->findById(conversationId);
    if (!conv) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Conversation not found.";
        return res;
    }

    if (!convRepo_->isParticipant(conversationId, *callerId)) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Access denied. You are not a participant in this conversation.";
        return res;
    }

    enrichConversation(*conv, *callerId);

    res.success = true;
    res.statusCode = 200;
    res.conversation = conv;
    return res;
}

MessagingResult MessagingService::getMessages(const std::string& sessionToken,
                                              int64_t conversationId,
                                              int limit,
                                              int64_t beforeId,
                                              int64_t afterId) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    auto conv = convRepo_->findById(conversationId);
    if (!conv) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Conversation not found.";
        return res;
    }

    if (!convRepo_->isParticipant(conversationId, *callerId)) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Access denied. You are not a participant in this conversation.";
        return res;
    }

    enrichConversation(*conv, *callerId);
    auto msgs = msgRepo_->getMessages(conversationId, limit, beforeId, afterId);

    res.success = true;
    res.statusCode = 200;
    res.conversation = conv;
    res.messages = std::move(msgs);
    return res;
}

MessagingResult MessagingService::sendMessage(const std::string& sessionToken,
                                              int64_t conversationId,
                                              const std::string& body) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    std::string trimmedBody = Validator::trim(body);
    if (trimmedBody.empty()) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Message body cannot be empty.";
        return res;
    }

    if (trimmedBody.length() > 4000) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Message exceeds maximum length of 4000 characters.";
        return res;
    }

    auto conv = convRepo_->findById(conversationId);
    if (!conv) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Conversation not found.";
        return res;
    }

    if (!convRepo_->isParticipant(conversationId, *callerId)) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Access denied. You are not a participant in this conversation.";
        return res;
    }

    auto createdMsg = msgRepo_->createMessage(conversationId, *callerId, trimmedBody);
    if (!createdMsg) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to persist message.";
        return res;
    }

    res.success = true;
    res.statusCode = 201;
    res.chatMessage = createdMsg;
    return res;
}

MessagingResult MessagingService::markAsRead(const std::string& sessionToken, int64_t conversationId) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    auto conv = convRepo_->findById(conversationId);
    if (!conv) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Conversation not found.";
        return res;
    }

    if (!convRepo_->isParticipant(conversationId, *callerId)) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Access denied. You are not a participant in this conversation.";
        return res;
    }

    msgRepo_->markAsRead(conversationId, *callerId);

    res.success = true;
    res.statusCode = 200;
    res.message = "Messages marked as read.";
    return res;
}

} // namespace connectx
