#pragma once

#include "Models.hpp"
#include "UserRepository.hpp"
#include "SessionRepository.hpp"
#include <memory>
#include <string>
#include <optional>

namespace connectx {

class IAuthService {
public:
    virtual ~IAuthService() = default;

    virtual AuthResult registerUser(const std::string& username,
                                    const std::string& email,
                                    const std::string& fullName,
                                    const std::string& password) = 0;

    virtual AuthResult login(const std::string& identifier,
                             const std::string& password) = 0;

    virtual bool logout(const std::string& token) = 0;

    virtual std::optional<User> validateSession(const std::string& token) = 0;
};

class AuthService : public IAuthService {
public:
    AuthService(std::shared_ptr<IUserRepository> userRepo,
                std::shared_ptr<ISessionRepository> sessionRepo);
    ~AuthService() override = default;

    AuthResult registerUser(const std::string& username,
                            const std::string& email,
                            const std::string& fullName,
                            const std::string& password) override;

    AuthResult login(const std::string& identifier,
                     const std::string& password) override;

    bool logout(const std::string& token) override;

    std::optional<User> validateSession(const std::string& token) override;

    // Cryptographic helper methods
    static std::string generateSalt(size_t byteCount = 16);
    static std::string generateSessionToken(size_t byteCount = 32);
    static std::string hashPassword(const std::string& password, const std::string& saltHex);
    static bool verifyPassword(const std::string& password, const std::string& saltHex, const std::string& expectedHashHex);

private:
    std::shared_ptr<IUserRepository> userRepo_;
    std::shared_ptr<ISessionRepository> sessionRepo_;
};

} // namespace connectx
