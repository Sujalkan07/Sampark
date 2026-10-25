#pragma once

#include "Models.hpp"
#include "UserRepository.hpp"
#include "SessionRepository.hpp"
#include <memory>
#include <string>
#include <optional>
#include <vector>

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

    virtual std::optional<std::vector<User>> searchUsers(const std::string& token,
                                                         const std::string& query,
                                                         int limit = 50) = 0;

    // Phase 3 extensions
    virtual AuthResult updateProfile(const std::string& token,
                                     const std::string& fullName,
                                     const std::string& bio) = 0;

    virtual AuthResult changePassword(const std::string& token,
                                      const std::string& currentPassword,
                                      const std::string& newPassword) = 0;

    virtual std::optional<User> getUserById(const std::string& token, int64_t targetUserId) = 0;
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

    std::optional<std::vector<User>> searchUsers(const std::string& token,
                                                 const std::string& query,
                                                 int limit = 50) override;

    // Phase 3 extensions
    AuthResult updateProfile(const std::string& token,
                             const std::string& fullName,
                             const std::string& bio) override;

    AuthResult changePassword(const std::string& token,
                              const std::string& currentPassword,
                              const std::string& newPassword) override;

    std::optional<User> getUserById(const std::string& token, int64_t targetUserId) override;

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
