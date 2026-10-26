#pragma once

#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace connectx {

using json = nlohmann::json;

struct User {
    int64_t id = 0;
    std::string username;
    std::string email;
    std::string fullName;
    std::string passwordHash;
    std::string salt;
    std::string createdAt;
    std::string updatedAt;
    std::string bio;
    std::string lastActiveAt;

    json toJson(bool includePrivate = false) const {
        json j = {
            {"id", id},
            {"username", username},
            {"email", email},
            {"fullName", fullName},
            {"createdAt", createdAt},
            {"updatedAt", updatedAt},
            {"bio", bio},
            {"lastActiveAt", lastActiveAt}
        };
        if (includePrivate) {
            j["passwordHash"] = passwordHash;
            j["salt"] = salt;
        }
        return j;
    }

    json toPublicJson() const {
        return {
            {"id", id},
            {"username", username},
            {"fullName", fullName},
            {"createdAt", createdAt},
            {"bio", bio},
            {"lastActiveAt", lastActiveAt}
        };
    }
};

struct Session {
    std::string token;
    int64_t userId = 0;
    std::string createdAt;
    std::string expiresAt;
};

struct AuthResult {
    bool success = false;
    int statusCode = 200;
    std::string message;
    std::optional<User> user;
    std::optional<Session> session;
};

// Phase 4: Conversation Member (for group and participant management)
struct ConversationMember {
    int64_t id = 0;
    int64_t conversationId = 0;
    int64_t userId = 0;
    std::string role = "member"; // "admin", "member"
    std::string joinedAt;
    int64_t lastReadMessageId = 0;
    std::optional<User> user;

    json toJson() const {
        json j = {
            {"id", id},
            {"conversationId", conversationId},
            {"userId", userId},
            {"role", role},
            {"joinedAt", joinedAt},
            {"lastReadMessageId", lastReadMessageId}
        };
        if (user.has_value()) {
            j["user"] = user->toPublicJson();
        }
        return j;
    }
};

// Phase 4: Reply Preview
struct ReplyPreview {
    int64_t id = 0;
    int64_t senderId = 0;
    std::string senderUsername;
    std::string senderName;
    std::string body;
    bool isDeleted = false;

    json toJson() const {
        return {
            {"id", id},
            {"senderId", senderId},
            {"senderUsername", senderUsername},
            {"senderName", senderName},
            {"body", body},
            {"isDeleted", isDeleted}
        };
    }
};

// Phase 4: Reaction Summary
struct ReactionSummary {
    std::string emoji;
    int count = 0;
    bool userReacted = false;
    std::vector<std::string> users;

    json toJson() const {
        return {
            {"emoji", emoji},
            {"count", count},
            {"userReacted", userReacted},
            {"users", users}
        };
    }
};

// Phase 4: Attachment
struct Attachment {
    int64_t id = 0;
    std::optional<int64_t> messageId;
    int64_t uploaderId = 0;
    int64_t conversationId = 0;
    std::string fileName;
    std::string filePath;
    int64_t fileSize = 0;
    std::string mimeType;
    std::string createdAt;

    json toJson() const {
        return {
            {"id", id},
            {"fileName", fileName},
            {"fileSize", fileSize},
            {"mimeType", mimeType},
            {"url", "/api/attachments/" + std::to_string(id)},
            {"createdAt", createdAt}
        };
    }
};

// Phase 4: User Preferences
struct UserPreferences {
    int64_t userId = 0;
    std::string theme = "system"; // "light", "dark", "system"
    bool soundNotifications = true;
    bool desktopNotifications = true;
    bool readReceiptsEnabled = true;
    std::string updatedAt;

    json toJson() const {
        return {
            {"userId", userId},
            {"theme", theme},
            {"soundNotifications", soundNotifications},
            {"sound_enabled", soundNotifications},
            {"desktopNotifications", desktopNotifications},
            {"desktop_notifications_enabled", desktopNotifications},
            {"readReceiptsEnabled", readReceiptsEnabled},
            {"read_receipts_enabled", readReceiptsEnabled},
            {"updatedAt", updatedAt}
        };
    }
};

struct Message {
    int64_t id = 0;
    int64_t conversationId = 0;
    int64_t senderId = 0;
    std::string senderUsername;
    std::string senderName;
    std::string body;
    std::string createdAt;
    std::optional<std::string> readAt;
    std::optional<std::string> editedAt;
    bool isDeleted = false;

    // Phase 4 extensions
    std::optional<int64_t> replyToId;
    std::optional<ReplyPreview> replyTo;
    std::vector<ReactionSummary> reactions;
    std::optional<Attachment> attachment;

    json toJson() const {
        json j = {
            {"id", id},
            {"conversationId", conversationId},
            {"senderId", senderId},
            {"body", body},
            {"createdAt", createdAt},
            {"isDeleted", isDeleted}
        };
        if (!senderUsername.empty()) {
            j["senderUsername"] = senderUsername;
        }
        if (!senderName.empty()) {
            j["senderName"] = senderName;
        }
        if (readAt.has_value()) {
            j["readAt"] = *readAt;
        } else {
            j["readAt"] = nullptr;
        }
        if (editedAt.has_value()) {
            j["editedAt"] = *editedAt;
        } else {
            j["editedAt"] = nullptr;
        }
        if (replyToId.has_value()) {
            j["replyToId"] = *replyToId;
        } else {
            j["replyToId"] = nullptr;
        }
        if (replyTo.has_value()) {
            j["replyTo"] = replyTo->toJson();
        } else {
            j["replyTo"] = nullptr;
        }

        json reactionsArr = json::array();
        for (const auto& r : reactions) {
            reactionsArr.push_back(r.toJson());
        }
        j["reactions"] = reactionsArr;

        if (attachment.has_value()) {
            j["attachment"] = attachment->toJson();
        } else {
            j["attachment"] = nullptr;
        }

        return j;
    }
};

struct Conversation {
    int64_t id = 0;
    std::string type = "direct"; // "direct", "group"
    std::string name;
    std::string description;
    std::string avatarUrl;
    std::optional<int64_t> createdBy;
    int64_t userLowId = 0;
    int64_t userHighId = 0;
    std::string createdAt;
    std::string updatedAt;

    // Associated context for current user view
    std::optional<User> otherParticipant;
    std::vector<ConversationMember> members;
    std::optional<Message> lastMessage;
    int unreadCount = 0;

    json toJson() const {
        json j = {
            {"id", id},
            {"type", type},
            {"name", name},
            {"description", description},
            {"avatarUrl", avatarUrl},
            {"createdAt", createdAt},
            {"updatedAt", updatedAt},
            {"unreadCount", unreadCount}
        };
        if (createdBy.has_value()) {
            j["createdBy"] = *createdBy;
        } else {
            j["createdBy"] = nullptr;
        }
        if (type == "direct") {
            j["userLowId"] = userLowId;
            j["userHighId"] = userHighId;
            if (otherParticipant.has_value()) {
                j["participant"] = otherParticipant->toPublicJson();
            }
        } else {
            json membersArr = json::array();
            for (const auto& m : members) {
                membersArr.push_back(m.toJson());
            }
            j["members"] = membersArr;
        }
        if (lastMessage.has_value()) {
            j["lastMessage"] = lastMessage->toJson();
        } else {
            j["lastMessage"] = nullptr;
        }
        return j;
    }
};

} // namespace connectx
