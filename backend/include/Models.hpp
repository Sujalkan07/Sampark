#pragma once

#include <string>
#include <optional>
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

struct Message {
    int64_t id = 0;
    int64_t conversationId = 0;
    int64_t senderId = 0;
    std::string body;
    std::string createdAt;
    std::optional<std::string> readAt;
    std::optional<std::string> editedAt;
    bool isDeleted = false;

    json toJson() const {
        json j = {
            {"id", id},
            {"conversationId", conversationId},
            {"senderId", senderId},
            {"body", body},
            {"createdAt", createdAt},
            {"isDeleted", isDeleted}
        };
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
        return j;
    }
};

struct Conversation {
    int64_t id = 0;
    int64_t userLowId = 0;
    int64_t userHighId = 0;
    std::string createdAt;
    std::string updatedAt;

    // Associated context for current user view
    std::optional<User> otherParticipant;
    std::optional<Message> lastMessage;
    int unreadCount = 0;

    json toJson() const {
        json j = {
            {"id", id},
            {"userLowId", userLowId},
            {"userHighId", userHighId},
            {"createdAt", createdAt},
            {"updatedAt", updatedAt},
            {"unreadCount", unreadCount}
        };
        if (otherParticipant.has_value()) {
            j["participant"] = otherParticipant->toPublicJson();
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
