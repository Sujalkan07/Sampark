#include "MessagingService.hpp"
#include "Validator.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/rand.h>

namespace connectx {

namespace fs = std::filesystem;

MessagingService::MessagingService(std::shared_ptr<IConversationRepository> convRepo,
                                   std::shared_ptr<IMessageRepository> msgRepo,
                                   std::shared_ptr<IUserRepository> userRepo,
                                   std::shared_ptr<ISessionRepository> sessionRepo)
    : convRepo_(std::move(convRepo)),
      msgRepo_(std::move(msgRepo)),
      userRepo_(std::move(userRepo)),
      sessionRepo_(std::move(sessionRepo)) {
    // Ensure uploads directory exists
    try {
        fs::create_directories("uploads");
    } catch (...) {}
}

std::optional<int64_t> MessagingService::authenticate(const std::string& token) {
    if (token.empty()) return std::nullopt;
    auto session = sessionRepo_->findSession(token);
    if (!session) return std::nullopt;
    return session->userId;
}

void MessagingService::enrichConversation(Conversation& conv, int64_t currentUserId) {
    if (conv.type == "direct") {
        int64_t otherId = (conv.userLowId == currentUserId) ? conv.userHighId : conv.userLowId;
        auto otherUser = userRepo_->findById(otherId);
        if (otherUser) {
            conv.otherParticipant = otherUser;
        }
    } else {
        conv.members = convRepo_->getGroupMembers(conv.id);
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
        res.message = "Access denied: you are not a participant in this conversation.";
        return res;
    }

    enrichConversation(*conv, *callerId);

    res.success = true;
    res.statusCode = 200;
    res.conversation = conv;
    return res;
}

MessagingResult MessagingService::getMessages(const std::string& sessionToken, int64_t conversationId, int limit, int64_t beforeId, int64_t afterId) {
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
        res.message = "Access denied: you are not a participant in this conversation.";
        return res;
    }

    enrichConversation(*conv, *callerId);

    auto msgs = msgRepo_->getMessages(conversationId, limit, beforeId, afterId, *callerId);

    // Read receipt privacy enforcement:
    // If either participant in a direct conversation has disabled read receipts,
    // suppress readAt timestamps so read status is not disclosed.
    if (conv->type == "direct") {
        int64_t otherId = (conv->userLowId == *callerId) ? conv->userHighId : conv->userLowId;
        auto myPrefs = msgRepo_->getUserPreferences(*callerId);
        auto otherPrefs = msgRepo_->getUserPreferences(otherId);
        if (!myPrefs.readReceiptsEnabled || !otherPrefs.readReceiptsEnabled) {
            for (auto& m : msgs) {
                m.readAt = std::nullopt;
            }
        }
    }

    res.success = true;
    res.statusCode = 200;
    res.conversation = conv;
    res.messages = std::move(msgs);
    return res;
}

MessagingResult MessagingService::sendMessage(const std::string& sessionToken, int64_t conversationId, const std::string& body,
                                              std::optional<int64_t> replyToId,
                                              std::optional<int64_t> attachmentId) {
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
        res.message = "Access denied: you are not a participant in this conversation.";
        return res;
    }

    // Body validation: if no attachment, body must not be empty/whitespace
    std::string trimmedBody = body;
    size_t first = trimmedBody.find_first_not_of(" \t\r\n");
    size_t last = trimmedBody.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) {
        if (!attachmentId.has_value() || *attachmentId <= 0) {
            res.success = false;
            res.statusCode = 400;
            res.message = "Message body cannot be empty.";
            return res;
        }
        trimmedBody = ""; // Empty body is acceptable if attachment is sent
    } else {
        trimmedBody = trimmedBody.substr(first, last - first + 1);
    }

    if (trimmedBody.size() > 4000) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Message exceeds the maximum allowed length of 4000 characters.";
        return res;
    }

    // Verify reply reference if provided
    if (replyToId.has_value() && *replyToId > 0) {
        auto originalMsg = msgRepo_->getMessageById(*replyToId, *callerId);
        if (!originalMsg || originalMsg->conversationId != conversationId) {
            res.success = false;
            res.statusCode = 400;
            res.message = "Invalid reply reference: message does not belong to this conversation.";
            return res;
        }
    }

    // Validate attachment if provided
    if (attachmentId.has_value() && *attachmentId > 0) {
        auto att = msgRepo_->getAttachmentById(*attachmentId);
        if (!att || att->conversationId != conversationId || att->uploaderId != *callerId) {
            res.success = false;
            res.statusCode = 400;
            res.message = "Invalid attachment reference: attachment does not belong to this conversation or user.";
            return res;
        }
        if (att->messageId.has_value() && *att->messageId > 0) {
            res.success = false;
            res.statusCode = 400;
            res.message = "Attachment is already linked to another message.";
            return res;
        }
    }

    auto createdMsg = msgRepo_->createMessage(conversationId, *callerId, trimmedBody, replyToId);
    if (!createdMsg) {
        // If message creation failed and an unlinked attachment was provided, clean up the attachment to avoid orphans
        if (attachmentId.has_value() && *attachmentId > 0) {
            auto att = msgRepo_->getAttachmentById(*attachmentId);
            if (att && att->uploaderId == *callerId && !att->messageId.has_value()) {
                try {
                    if (fs::exists(att->filePath)) fs::remove(att->filePath);
                } catch (...) {}
                msgRepo_->deleteAttachment(*attachmentId);
            }
        }
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to persist message.";
        return res;
    }

    // Link attachment if provided
    if (attachmentId.has_value() && *attachmentId > 0) {
        auto att = msgRepo_->getAttachmentById(*attachmentId);
        if (att && att->conversationId == conversationId && att->uploaderId == *callerId && !att->messageId.has_value()) {
            msgRepo_->linkAttachmentToMessage(*attachmentId, createdMsg->id);
            att->messageId = createdMsg->id;
            createdMsg->attachment = att;
        }
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
    if (!conv || !convRepo_->isParticipant(conversationId, *callerId)) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Access denied: you are not a participant in this conversation.";
        return res;
    }

    if (conv->type == "group") {
        convRepo_->updateLastReadMessage(conversationId, *callerId);
    } else {
        auto myPrefs = msgRepo_->getUserPreferences(*callerId);
        int64_t otherId = (conv->userLowId == *callerId) ? conv->userHighId : conv->userLowId;
        auto otherPrefs = msgRepo_->getUserPreferences(otherId);
        bool suppress = (!myPrefs.readReceiptsEnabled || !otherPrefs.readReceiptsEnabled);
        msgRepo_->markAsRead(conversationId, *callerId, suppress);
    }

    res.success = true;
    res.statusCode = 200;
    res.message = "Messages marked as read.";
    return res;
}

MessagingResult MessagingService::editMessage(const std::string& sessionToken, int64_t messageId, const std::string& newBody) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    auto existingMsg = msgRepo_->getMessageById(messageId, *callerId);
    if (!existingMsg) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Message not found.";
        return res;
    }

    if (existingMsg->senderId != *callerId) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Forbidden: you can only edit messages you authored.";
        return res;
    }

    if (existingMsg->isDeleted) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Cannot edit a deleted message.";
        return res;
    }

    std::string trimmed = newBody;
    size_t first = trimmed.find_first_not_of(" \t\r\n");
    size_t last = trimmed.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Message body cannot be empty.";
        return res;
    }
    trimmed = trimmed.substr(first, last - first + 1);

    if (trimmed.size() > 4000) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Message exceeds the maximum allowed length of 4000 characters.";
        return res;
    }

    bool ok = msgRepo_->updateMessageBody(messageId, trimmed);
    if (!ok) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to update message.";
        return res;
    }

    res.success = true;
    res.statusCode = 200;
    res.chatMessage = msgRepo_->getMessageById(messageId, *callerId);
    return res;
}

MessagingResult MessagingService::deleteMessage(const std::string& sessionToken, int64_t messageId) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    auto existingMsg = msgRepo_->getMessageById(messageId, *callerId);
    if (!existingMsg) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Message not found.";
        return res;
    }

    if (existingMsg->senderId != *callerId) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Forbidden: you can only delete messages you authored.";
        return res;
    }

    if (existingMsg->isDeleted) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Message is already deleted.";
        return res;
    }

    bool ok = msgRepo_->deleteMessage(messageId);
    if (!ok) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to delete message.";
        return res;
    }

    res.success = true;
    res.statusCode = 200;
    res.message = "Message deleted successfully.";
    auto updated = msgRepo_->getMessageById(messageId, *callerId);
    if (updated) {
        res.chatMessage = *updated;
    }
    return res;
}

MessagingResult MessagingService::searchConversationMessages(const std::string& sessionToken, int64_t conversationId, const std::string& query, int limit) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    if (!convRepo_->isParticipant(conversationId, *callerId)) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Access denied: you are not a participant in this conversation.";
        return res;
    }

    res.messages = msgRepo_->searchMessages(conversationId, query, limit, *callerId);
    res.success = true;
    res.statusCode = 200;
    return res;
}

MessagingResult MessagingService::searchGlobalMessages(const std::string& sessionToken, const std::string& query, int limit) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    res.messages = msgRepo_->searchAllUserMessages(*callerId, query, limit);
    res.success = true;
    res.statusCode = 200;
    return res;
}

// Phase 4: Group Chat Extensions
MessagingResult MessagingService::createGroup(const std::string& sessionToken,
                                              const std::string& name,
                                              const std::string& description,
                                              const std::string& avatarUrl,
                                              const std::vector<int64_t>& memberIds) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    std::string trimmedName = name;
    size_t f = trimmedName.find_first_not_of(" \t\r\n");
    size_t l = trimmedName.find_last_not_of(" \t\r\n");
    if (f == std::string::npos) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Group name cannot be empty.";
        return res;
    }
    trimmedName = trimmedName.substr(f, l - f + 1);

    if (trimmedName.size() > 100) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Group name cannot exceed 100 characters.";
        return res;
    }

    if (description.size() > 500) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Group description cannot exceed 500 characters.";
        return res;
    }

    auto conv = convRepo_->createGroup(trimmedName, description, avatarUrl, *callerId, memberIds);
    if (!conv) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to create group.";
        return res;
    }

    enrichConversation(*conv, *callerId);

    res.success = true;
    res.statusCode = 201;
    res.conversation = conv;
    return res;
}

MessagingResult MessagingService::getGroupMembers(const std::string& sessionToken, int64_t conversationId) {
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
    if (conv->type != "group") {
        res.success = false;
        res.statusCode = 400;
        res.message = "This operation is only valid for group conversations.";
        return res;
    }

    if (!convRepo_->isParticipant(conversationId, *callerId)) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Access denied: you are not a member of this group.";
        return res;
    }

    res.members = convRepo_->getGroupMembers(conversationId);
    res.success = true;
    res.statusCode = 200;
    return res;
}

MessagingResult MessagingService::addGroupMember(const std::string& sessionToken, int64_t conversationId, int64_t targetUserId) {
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
    if (conv->type != "group") {
        res.success = false;
        res.statusCode = 400;
        res.message = "This operation is only valid for group conversations.";
        return res;
    }

    auto role = convRepo_->getMemberRole(conversationId, *callerId);
    if (!role || *role != "admin") {
        res.success = false;
        res.statusCode = 403;
        res.message = "Only group administrators can add new members.";
        return res;
    }

    auto targetUser = userRepo_->findById(targetUserId);
    if (!targetUser) {
        res.success = false;
        res.statusCode = 404;
        res.message = "User to add does not exist.";
        return res;
    }

    if (convRepo_->isParticipant(conversationId, targetUserId)) {
        res.success = false;
        res.statusCode = 400;
        res.message = "User is already a member of this group.";
        return res;
    }

    bool ok = convRepo_->addMember(conversationId, targetUserId, "member");
    if (!ok) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to add member to group.";
        return res;
    }

    res.members = convRepo_->getGroupMembers(conversationId);
    res.success = true;
    res.statusCode = 200;
    res.message = "Member added successfully.";
    return res;
}

MessagingResult MessagingService::removeGroupMember(const std::string& sessionToken, int64_t conversationId, int64_t targetUserId) {
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
    if (conv->type != "group") {
        res.success = false;
        res.statusCode = 400;
        res.message = "This operation is only valid for group conversations.";
        return res;
    }

    if (!convRepo_->isParticipant(conversationId, targetUserId)) {
        res.success = false;
        res.statusCode = 404;
        res.message = "User is not a member of this group.";
        return res;
    }

    if (*callerId == targetUserId) {
        return leaveGroup(sessionToken, conversationId);
    }

    auto role = convRepo_->getMemberRole(conversationId, *callerId);
    if (!role || *role != "admin") {
        res.success = false;
        res.statusCode = 403;
        res.message = "Only group administrators can remove other members.";
        return res;
    }

    bool ok = convRepo_->removeMember(conversationId, targetUserId);
    if (!ok) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to remove member.";
        return res;
    }

    res.success = true;
    res.statusCode = 200;
    res.message = "Member removed from group.";
    return res;
}

MessagingResult MessagingService::updateGroupMemberRole(const std::string& sessionToken, int64_t conversationId, int64_t targetUserId, const std::string& role) {
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
    if (conv->type != "group") {
        res.success = false;
        res.statusCode = 400;
        res.message = "This operation is only valid for group conversations.";
        return res;
    }

    auto myRole = convRepo_->getMemberRole(conversationId, *callerId);
    if (!myRole || *myRole != "admin") {
        res.success = false;
        res.statusCode = 403;
        res.message = "Only group administrators can change member roles.";
        return res;
    }

    if (role != "admin" && role != "member") {
        res.success = false;
        res.statusCode = 400;
        res.message = "Invalid role. Allowed roles: 'admin', 'member'.";
        return res;
    }

    if (!convRepo_->isParticipant(conversationId, targetUserId)) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Target user is not a member of this group.";
        return res;
    }

    if (role == "member") {
        auto targetRole = convRepo_->getMemberRole(conversationId, targetUserId);
        if (targetRole && *targetRole == "admin") {
            auto members = convRepo_->getGroupMembers(conversationId);
            int adminCount = 0;
            for (const auto& m : members) {
                if (m.role == "admin") adminCount++;
            }
            if (adminCount <= 1) {
                res.success = false;
                res.statusCode = 400;
                res.message = "Cannot demote the sole group administrator. Promote another admin first.";
                return res;
            }
        }
    }

    bool ok = convRepo_->updateMemberRole(conversationId, targetUserId, role);
    if (!ok) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to update member role.";
        return res;
    }

    res.success = true;
    res.statusCode = 200;
    res.message = "Member role updated successfully.";
    res.members = convRepo_->getGroupMembers(conversationId);
    return res;
}

MessagingResult MessagingService::updateGroup(const std::string& sessionToken, int64_t conversationId,
                                              const std::string& name, const std::string& description, const std::string& avatarUrl) {
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
    if (conv->type != "group") {
        res.success = false;
        res.statusCode = 400;
        res.message = "This operation is only valid for group conversations.";
        return res;
    }

    auto role = convRepo_->getMemberRole(conversationId, *callerId);
    if (!role || *role != "admin") {
        res.success = false;
        res.statusCode = 403;
        res.message = "Only group administrators can update group details.";
        return res;
    }

    std::string trimmedName = name;
    size_t f = trimmedName.find_first_not_of(" \t\r\n");
    size_t l = trimmedName.find_last_not_of(" \t\r\n");
    if (f == std::string::npos) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Group name cannot be empty.";
        return res;
    }
    trimmedName = trimmedName.substr(f, l - f + 1);

    bool ok = convRepo_->updateGroup(conversationId, trimmedName, description, avatarUrl);
    if (!ok) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to update group.";
        return res;
    }

    auto updatedConv = convRepo_->findById(conversationId);
    if (updatedConv) enrichConversation(*updatedConv, *callerId);
    res.success = true;
    res.statusCode = 200;
    res.conversation = updatedConv;
    return res;
}

MessagingResult MessagingService::deleteGroup(const std::string& sessionToken, int64_t conversationId) {
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
    if (conv->type != "group") {
        res.success = false;
        res.statusCode = 400;
        res.message = "This operation is only valid for group conversations.";
        return res;
    }

    auto role = convRepo_->getMemberRole(conversationId, *callerId);
    if (!role || *role != "admin") {
        res.success = false;
        res.statusCode = 403;
        res.message = "Only group administrators can delete the group.";
        return res;
    }

    bool ok = convRepo_->deleteGroup(conversationId);
    if (!ok) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to delete group.";
        return res;
    }

    res.success = true;
    res.statusCode = 200;
    res.message = "Group deleted successfully.";
    return res;
}

MessagingResult MessagingService::leaveGroup(const std::string& sessionToken, int64_t conversationId) {
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
    if (conv->type != "group") {
        res.success = false;
        res.statusCode = 400;
        res.message = "This operation is only valid for group conversations.";
        return res;
    }

    auto role = convRepo_->getMemberRole(conversationId, *callerId);
    if (!role) {
        res.success = false;
        res.statusCode = 403;
        res.message = "You are not a member of this group.";
        return res;
    }

    auto members = convRepo_->getGroupMembers(conversationId);
    if (members.size() <= 1) {
        // Last member leaving - disband the group cleanly
        bool delOk = convRepo_->deleteGroup(conversationId);
        if (!delOk) {
            res.success = false;
            res.statusCode = 500;
            res.message = "Failed to disband group upon departure.";
            return res;
        }
        res.success = true;
        res.statusCode = 200;
        res.message = "You left and the group was disbanded.";
        return res;
    }

    if (*role == "admin") {
        // Count other admins
        int otherAdmins = 0;
        int64_t nextMemberId = 0;
        for (const auto& m : members) {
            if (m.userId == *callerId) continue;
            if (m.role == "admin") otherAdmins++;
            else if (nextMemberId == 0) nextMemberId = m.userId;
        }

        if (otherAdmins == 0 && nextMemberId > 0) {
            // Transfer admin to the next member
            bool transOk = convRepo_->transferAdmin(conversationId, *callerId, nextMemberId);
            if (!transOk) {
                res.success = false;
                res.statusCode = 500;
                res.message = "Failed to transfer administration to a remaining member.";
                return res;
            }
        }
    }

    bool remOk = convRepo_->removeMember(conversationId, *callerId);
    if (!remOk) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to leave group.";
        return res;
    }

    res.success = true;
    res.statusCode = 200;
    res.message = "You left the group.";
    return res;
}

// Phase 4: Reactions
MessagingResult MessagingService::toggleReaction(const std::string& sessionToken, int64_t messageId, const std::string& emoji) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    if (emoji.empty() || emoji.size() > 16) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Invalid emoji specified.";
        return res;
    }

    auto msg = msgRepo_->getMessageById(messageId, *callerId);
    if (!msg) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Message not found.";
        return res;
    }

    if (!convRepo_->isParticipant(msg->conversationId, *callerId)) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Access denied: you cannot react to messages in conversations you are not part of.";
        return res;
    }

    if (msg->isDeleted) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Cannot react to a deleted message.";
        return res;
    }

    auto [ok, action] = msgRepo_->toggleReaction(messageId, *callerId, emoji);
    if (!ok) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to update reaction.";
        return res;
    }

    res.success = true;
    res.statusCode = 200;
    res.reactionAction = action;
    res.reactions = msgRepo_->getMessageReactions(messageId, *callerId);
    return res;
}

// Phase 4: Attachments
MessagingResult MessagingService::uploadAttachment(const std::string& sessionToken, int64_t conversationId,
                                                   const std::string& fileName, const std::string& rawBytes, const std::string& mimeType) {
    (void)mimeType; // Unused - server determines MIME strictly from file signatures
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    if (!convRepo_->isParticipant(conversationId, *callerId)) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Access denied: you are not a participant in this conversation.";
        return res;
    }

    auto valRes = Validator::validateAttachment(fileName, rawBytes);
    if (!valRes.isValid) {
        res.success = false;
        res.statusCode = 400;
        res.message = valRes.error;
        return res;
    }

    // Generate safe, server-controlled unique disk filename with zero user input in filename
    unsigned char randBytes[16];
    if (RAND_bytes(randBytes, sizeof(randBytes)) != 1) {
        std::random_device rd;
        for (int i = 0; i < 16; ++i) randBytes[i] = rd() & 0xFF;
    }
    std::stringstream hexStream;
    for (int i = 0; i < 16; ++i) {
        hexStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(randBytes[i]);
    }
    int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string diskName = std::to_string(timestamp) + "_" + std::to_string(*callerId) + "_" + hexStream.str() + valRes.safeStorageExt;
    std::string fullPath = "uploads/" + diskName;

    std::ofstream out(fullPath, std::ios::binary);
    if (!out.is_open()) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to store attachment on server.";
        return res;
    }
    out.write(rawBytes.data(), rawBytes.size());
    out.close();

    auto att = msgRepo_->createAttachment(*callerId, conversationId, valRes.sanitizedFileName, fullPath, static_cast<int64_t>(rawBytes.size()), valRes.detectedMime);
    if (!att) {
        try { fs::remove(fullPath); } catch (...) {}
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to record attachment.";
        return res;
    }

    res.success = true;
    res.statusCode = 201;
    res.attachment = att;
    return res;
}

MessagingResult MessagingService::getAttachment(const std::string& sessionToken, int64_t attachmentId) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    auto att = msgRepo_->getAttachmentById(attachmentId);
    if (!att) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Attachment not found.";
        return res;
    }

    if (!convRepo_->isParticipant(att->conversationId, *callerId)) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Access denied: you do not have permission to access this attachment.";
        return res;
    }

    // Check if the associated message was deleted
    if (att->messageId.has_value() && *att->messageId > 0) {
        auto msg = msgRepo_->getMessageById(*att->messageId, *callerId);
        if (msg && msg->isDeleted) {
            res.success = false;
            res.statusCode = 404;
            res.message = "Attachment is no longer available.";
            return res;
        }
    }

    try {
        if (!fs::exists(att->filePath)) {
            res.success = false;
            res.statusCode = 404;
            res.message = "Attachment file not found on disk.";
            return res;
        }
    } catch (...) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Attachment file not found on disk.";
        return res;
    }

    std::ifstream in(att->filePath, std::ios::binary);
    if (!in.is_open()) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Attachment file not found on disk.";
        return res;
    }

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    res.success = true;
    res.statusCode = 200;
    res.attachment = att;
    res.fileContent = std::move(content);
    res.fileMimeType = att->mimeType;
    res.fileName = att->fileName;
    return res;
}

MessagingResult MessagingService::deleteAttachment(const std::string& sessionToken, int64_t attachmentId) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    auto att = msgRepo_->getAttachmentById(attachmentId);
    if (!att) {
        res.success = false;
        res.statusCode = 404;
        res.message = "Attachment not found.";
        return res;
    }

    if (att->uploaderId != *callerId) {
        res.success = false;
        res.statusCode = 403;
        res.message = "Forbidden: you can only delete your own attachments.";
        return res;
    }

    if (att->messageId.has_value() && *att->messageId > 0) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Cannot delete an attachment that is already linked to a message.";
        return res;
    }

    try {
        if (fs::exists(att->filePath)) {
            fs::remove(att->filePath);
        }
    } catch (...) {}

    msgRepo_->deleteAttachment(attachmentId);

    res.success = true;
    res.statusCode = 200;
    res.message = "Attachment deleted.";
    return res;
}

int MessagingService::cleanupOrphanedAttachments(int maxAgeHours) {
    return msgRepo_->cleanupOrphanedAttachments(maxAgeHours);
}

// Phase 4: Preferences
MessagingResult MessagingService::getUserPreferences(const std::string& sessionToken) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    res.preferences = msgRepo_->getUserPreferences(*callerId);
    res.success = true;
    res.statusCode = 200;
    return res;
}

MessagingResult MessagingService::updateUserPreferences(const std::string& sessionToken, const UserPreferences& prefs) {
    MessagingResult res;
    auto callerId = authenticate(sessionToken);
    if (!callerId) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Authentication required. Please log in.";
        return res;
    }

    UserPreferences toSave = prefs;
    toSave.userId = *callerId;
    bool ok = msgRepo_->updateUserPreferences(toSave);
    if (!ok) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to save user preferences.";
        return res;
    }

    res.preferences = msgRepo_->getUserPreferences(*callerId);
    res.success = true;
    res.statusCode = 200;
    return res;
}

} // namespace connectx
