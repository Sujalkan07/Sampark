#include "Validator.hpp"
#include <algorithm>
#include <cctype>
#include <regex>

namespace connectx {

std::string Validator::trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();
    return (start < end) ? std::string(start, end) : std::string();
}

std::string Validator::toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return lower;
}

bool Validator::isValidUsername(const std::string& username, std::string& error) {
    if (username.length() < 3) {
        error = "Username must be at least 3 characters long.";
        return false;
    }
    if (username.length() > 30) {
        error = "Username cannot exceed 30 characters.";
        return false;
    }
    // Only letters, digits, underscores, dots, hyphens
    static const std::regex usernameRegex("^[a-zA-Z0-9][a-zA-Z0-9_.-]*[a-zA-Z0-9]$");
    if (!std::regex_match(username, usernameRegex)) {
        error = "Username must start and end with an alphanumeric character and contain only letters, numbers, underscores, dots, or hyphens.";
        return false;
    }
    return true;
}

bool Validator::isValidEmail(const std::string& email, std::string& error) {
    if (email.length() < 5) {
        error = "Email is too short.";
        return false;
    }
    if (email.length() > 100) {
        error = "Email cannot exceed 100 characters.";
        return false;
    }
    // Standard RFC-compliant relaxed email regex
    static const std::regex emailRegex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    if (!std::regex_match(email, emailRegex)) {
        error = "Please provide a valid email address.";
        return false;
    }
    return true;
}

bool Validator::isValidFullName(const std::string& fullName, std::string& error) {
    if (fullName.empty()) {
        error = "Full name is required.";
        return false;
    }
    if (fullName.length() > 100) {
        error = "Full name cannot exceed 100 characters.";
        return false;
    }
    return true;
}

bool Validator::isValidPassword(const std::string& password, std::string& error) {
    if (password.length() < 8) {
        error = "Password must be at least 8 characters long.";
        return false;
    }
    if (password.length() > 128) {
        error = "Password cannot exceed 128 characters.";
        return false;
    }
    return true;
}

bool Validator::isValidBio(const std::string& bio, std::string& error) {
    if (bio.length() > 300) {
        error = "Bio cannot exceed 300 characters.";
        return false;
    }
    return true;
}

} // namespace connectx
