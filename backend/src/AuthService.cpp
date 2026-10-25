#include "AuthService.hpp"
#include "Validator.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <sstream>
#include <iomanip>
#include <vector>

namespace connectx {

namespace {

std::string bytesToHex(const unsigned char* data, size_t length) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::vector<unsigned char> hexToBytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i + 1 < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

} // namespace

AuthService::AuthService(std::shared_ptr<IUserRepository> userRepo,
                         std::shared_ptr<ISessionRepository> sessionRepo)
    : userRepo_(std::move(userRepo)), sessionRepo_(std::move(sessionRepo)) {}

std::string AuthService::generateSalt(size_t byteCount) {
    std::vector<unsigned char> buf(byteCount);
    if (RAND_bytes(buf.data(), static_cast<int>(byteCount)) != 1) {
        throw std::runtime_error("Cryptographic random generation failed for salt.");
    }
    return bytesToHex(buf.data(), byteCount);
}

std::string AuthService::generateSessionToken(size_t byteCount) {
    std::vector<unsigned char> buf(byteCount);
    if (RAND_bytes(buf.data(), static_cast<int>(byteCount)) != 1) {
        throw std::runtime_error("Cryptographic random generation failed for session token.");
    }
    return bytesToHex(buf.data(), byteCount);
}

std::string AuthService::hashPassword(const std::string& password, const std::string& saltHex) {
    std::vector<unsigned char> saltBytes = hexToBytes(saltHex);
    constexpr int iterations = 100000;
    constexpr int keyLength = 32; // 256 bits
    unsigned char outKey[keyLength];

    if (PKCS5_PBKDF2_HMAC(password.c_str(),
                           static_cast<int>(password.length()),
                           saltBytes.data(),
                           static_cast<int>(saltBytes.size()),
                           iterations,
                           EVP_sha256(),
                           keyLength,
                           outKey) != 1) {
        throw std::runtime_error("PBKDF2 password hashing failed.");
    }

    return bytesToHex(outKey, keyLength);
}

bool AuthService::verifyPassword(const std::string& password, const std::string& saltHex, const std::string& expectedHashHex) {
    std::string calculatedHash = hashPassword(password, saltHex);
    if (calculatedHash.length() != expectedHashHex.length()) {
        return false;
    }
    // Constant-time memory comparison to mitigate timing attacks
    return CRYPTO_memcmp(calculatedHash.data(), expectedHashHex.data(), calculatedHash.length()) == 0;
}

AuthResult AuthService::registerUser(const std::string& rawUsername,
                                     const std::string& rawEmail,
                                     const std::string& rawFullName,
                                     const std::string& rawPassword) {
    AuthResult res;

    std::string username = Validator::trim(rawUsername);
    std::string email = Validator::toLower(Validator::trim(rawEmail));
    std::string fullName = Validator::trim(rawFullName);
    std::string password = rawPassword;

    std::string validationError;
    if (!Validator::isValidFullName(fullName, validationError)) {
        res.success = false;
        res.statusCode = 400;
        res.message = validationError;
        return res;
    }
    if (!Validator::isValidUsername(username, validationError)) {
        res.success = false;
        res.statusCode = 400;
        res.message = validationError;
        return res;
    }
    if (!Validator::isValidEmail(email, validationError)) {
        res.success = false;
        res.statusCode = 400;
        res.message = validationError;
        return res;
    }
    if (!Validator::isValidPassword(password, validationError)) {
        res.success = false;
        res.statusCode = 400;
        res.message = validationError;
        return res;
    }

    // Check uniqueness
    if (userRepo_->usernameExists(username)) {
        res.success = false;
        res.statusCode = 409;
        res.message = "Username '" + username + "' is already registered.";
        return res;
    }

    if (userRepo_->emailExists(email)) {
        res.success = false;
        res.statusCode = 409;
        res.message = "Email address is already associated with another account.";
        return res;
    }

    std::string salt = generateSalt(16);
    std::string hash = hashPassword(password, salt);

    auto createdUser = userRepo_->create(username, email, fullName, hash, salt);
    if (!createdUser) {
        res.success = false;
        res.statusCode = 500;
        res.message = "An error occurred while creating your account. Please try again.";
        return res;
    }

    // Create authenticated session upon successful registration
    std::string sessionToken = generateSessionToken(32);
    if (!sessionRepo_->createSession(sessionToken, createdUser->id, 168)) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Account created, but failed to establish session. Please log in.";
        return res;
    }

    auto session = sessionRepo_->findSession(sessionToken);

    res.success = true;
    res.statusCode = 201;
    res.message = "Account registered successfully.";
    res.user = createdUser;
    res.session = session;
    return res;
}

AuthResult AuthService::login(const std::string& rawIdentifier,
                              const std::string& rawPassword) {
    AuthResult res;

    std::string identifier = Validator::trim(rawIdentifier);
    std::string password = rawPassword;

    if (identifier.empty() || password.empty()) {
        res.success = false;
        res.statusCode = 400;
        res.message = "Username/email and password are required.";
        return res;
    }

    auto user = userRepo_->findByUsernameOrEmail(identifier);
    if (!user) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Invalid username or password.";
        return res;
    }

    if (!verifyPassword(password, user->salt, user->passwordHash)) {
        res.success = false;
        res.statusCode = 401;
        res.message = "Invalid username or password.";
        return res;
    }

    std::string sessionToken = generateSessionToken(32);
    if (!sessionRepo_->createSession(sessionToken, user->id, 168)) {
        res.success = false;
        res.statusCode = 500;
        res.message = "Failed to establish a user session. Please try again.";
        return res;
    }

    auto session = sessionRepo_->findSession(sessionToken);

    res.success = true;
    res.statusCode = 200;
    res.message = "Login successful.";
    res.user = user;
    res.session = session;
    return res;
}

bool AuthService::logout(const std::string& token) {
    if (token.empty()) return false;
    return sessionRepo_->deleteSession(token);
}

std::optional<User> AuthService::validateSession(const std::string& token) {
    if (token.empty()) return std::nullopt;

    auto session = sessionRepo_->findSession(token);
    if (!session) {
        return std::nullopt;
    }

    return userRepo_->findById(session->userId);
}

} // namespace connectx
