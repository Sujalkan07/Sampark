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

    json toJson(bool includePrivate = false) const {
        json j = {
            {"id", id},
            {"username", username},
            {"email", email},
            {"fullName", fullName},
            {"createdAt", createdAt},
            {"updatedAt", updatedAt}
        };
        if (includePrivate) {
            j["passwordHash"] = passwordHash;
            j["salt"] = salt;
        }
        return j;
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

} // namespace connectx
