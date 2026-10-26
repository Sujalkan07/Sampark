#include "Validator.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <regex>
#include <unordered_set>

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

AttachmentValidationResult Validator::validateAttachment(const std::string& fileName, const std::string& rawBytes) {
    AttachmentValidationResult res;

    // 1. Check for empty filename or null bytes
    if (fileName.empty()) {
        res.error = "File name is required.";
        return res;
    }
    for (char c : fileName) {
        if (c == '\0') {
            res.error = "File name contains invalid characters.";
            return res;
        }
    }

    // 2. Size checks: empty or exceeding 10MB
    if (rawBytes.empty()) {
        res.error = "Attachment file cannot be empty.";
        return res;
    }
    if (rawBytes.size() > 10 * 1024 * 1024) {
        res.error = "Attachment size exceeds the 10MB limit.";
        return res;
    }

    // 3. Extract basename to defeat directory path traversal
    size_t lastSlash = fileName.find_last_of("/\\");
    std::string baseName = (lastSlash != std::string::npos) ? fileName.substr(lastSlash + 1) : fileName;

    // Sanitize filename (strip dangerous characters, trim)
    std::string safe;
    for (char c : baseName) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|' || uc < 32) {
            continue;
        }
        safe.push_back(c);
    }
    // Trim leading/trailing dots and spaces
    size_t first = safe.find_first_not_of(" .");
    size_t last = safe.find_last_not_of(" .");
    if (first == std::string::npos || last == std::string::npos) {
        safe = "attachment.dat";
    } else {
        safe = safe.substr(first, last - first + 1);
    }
    if (safe.empty() || safe == "." || safe == "..") {
        safe = "attachment.dat";
    }
    if (safe.length() > 255) {
        safe = safe.substr(safe.length() - 255);
    }

    // Extract extension
    size_t dotPos = safe.find_last_of('.');
    if (dotPos == std::string::npos || dotPos == safe.length() - 1) {
        res.error = "File must have a valid extension.";
        return res;
    }
    std::string ext = toLower(safe.substr(dotPos));

    // Strict allowlist
    static const std::unordered_set<std::string> allowedExts = {
        ".jpg", ".jpeg", ".png", ".webp", ".gif",
        ".pdf", ".txt", ".docx", ".doc"
    };
    if (allowedExts.find(ext) == allowedExts.end()) {
        res.error = "File type not allowed. Supported formats: images (JPEG, PNG, WebP, GIF) and documents (PDF, TXT, DOCX, DOC).";
        return res;
    }

    // 4. Magic byte sniffing & content validation
    std::string detectedFormat;
    std::string detectedMime;

    const size_t len = rawBytes.size();
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(rawBytes.data());

    if (len >= 8 && memcmp(bytes, "\x89PNG\r\n\x1a\n", 8) == 0) {
        detectedFormat = "png";
        detectedMime = "image/png";
    } else if (len >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
        detectedFormat = "jpeg";
        detectedMime = "image/jpeg";
    } else if (len >= 6 && (memcmp(bytes, "GIF87a", 6) == 0 || memcmp(bytes, "GIF89a", 6) == 0)) {
        detectedFormat = "gif";
        detectedMime = "image/gif";
    } else if (len >= 12 && memcmp(bytes, "RIFF", 4) == 0 && memcmp(bytes + 8, "WEBP", 4) == 0) {
        detectedFormat = "webp";
        detectedMime = "image/webp";
    } else if (len >= 5 && memcmp(bytes, "%PDF-", 5) == 0) {
        detectedFormat = "pdf";
        detectedMime = "application/pdf";
    } else if (len >= 4 && bytes[0] == 0x50 && bytes[1] == 0x4B &&
               ((bytes[2] == 0x03 && bytes[3] == 0x04) || (bytes[2] == 0x05 && bytes[3] == 0x06))) {
        detectedFormat = "docx";
        detectedMime = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    } else if (len >= 8 && memcmp(bytes, "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8) == 0) {
        detectedFormat = "doc";
        detectedMime = "application/msword";
    } else if (ext == ".txt") {
        bool validText = true;
        for (size_t i = 0; i < len; ++i) {
            unsigned char b = bytes[i];
            if (b == 0) {
                validText = false;
                break;
            }
            if ((b < 9) || (b >= 11 && b <= 12) || (b >= 14 && b <= 31)) {
                validText = false;
                break;
            }
        }
        if (validText) {
            detectedFormat = "txt";
            detectedMime = "text/plain";
        }
    }

    if (detectedFormat.empty()) {
        if (ext == ".txt") {
            res.error = "File content is not valid plain text.";
        } else {
            res.error = "File content could not be verified or is corrupted.";
        }
        return res;
    }

    // 5. Verify format matches extension
    bool matches = false;
    if (detectedFormat == "png" && ext == ".png") matches = true;
    else if (detectedFormat == "jpeg" && (ext == ".jpg" || ext == ".jpeg")) matches = true;
    else if (detectedFormat == "gif" && ext == ".gif") matches = true;
    else if (detectedFormat == "webp" && ext == ".webp") matches = true;
    else if (detectedFormat == "pdf" && ext == ".pdf") matches = true;
    else if (detectedFormat == "docx" && ext == ".docx") matches = true;
    else if (detectedFormat == "doc" && ext == ".doc") matches = true;
    else if (detectedFormat == "txt" && ext == ".txt") matches = true;

    if (!matches) {
        res.error = "File content does not match the file extension.";
        return res;
    }

    res.isValid = true;
    res.detectedMime = detectedMime;
    res.sanitizedFileName = safe;
    res.safeStorageExt = ext;
    return res;
}

} // namespace connectx
