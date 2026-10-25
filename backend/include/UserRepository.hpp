#pragma once

#include "Models.hpp"
#include "Database.hpp"
#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace connectx {

class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    virtual std::optional<User> findById(int64_t id) = 0;
    virtual std::optional<User> findByUsername(const std::string& username) = 0;
    virtual std::optional<User> findByEmail(const std::string& email) = 0;
    virtual std::optional<User> findByUsernameOrEmail(const std::string& identifier) = 0;
    virtual bool usernameExists(const std::string& username) = 0;
    virtual bool emailExists(const std::string& email) = 0;
    virtual std::optional<User> create(const std::string& username,
                                         const std::string& email,
                                         const std::string& fullName,
                                         const std::string& passwordHash,
                                         const std::string& salt) = 0;
    virtual std::vector<User> searchUsers(const std::string& query,
                                          int64_t excludeUserId,
                                          int limit = 50) = 0;
    virtual bool updateProfile(int64_t userId, const std::string& fullName, const std::string& bio) = 0;
    virtual bool updatePassword(int64_t userId, const std::string& newHash, const std::string& newSalt) = 0;
    virtual bool updateLastActive(int64_t userId) = 0;
};

class SqliteUserRepository : public IUserRepository {
public:
    explicit SqliteUserRepository(Database& db);
    ~SqliteUserRepository() override = default;

    std::optional<User> findById(int64_t id) override;
    std::optional<User> findByUsername(const std::string& username) override;
    std::optional<User> findByEmail(const std::string& email) override;
    std::optional<User> findByUsernameOrEmail(const std::string& identifier) override;
    bool usernameExists(const std::string& username) override;
    bool emailExists(const std::string& email) override;
    std::optional<User> create(const std::string& username,
                                const std::string& email,
                                const std::string& fullName,
                                const std::string& passwordHash,
                                const std::string& salt) override;
    std::vector<User> searchUsers(const std::string& query,
                                  int64_t excludeUserId,
                                  int limit = 50) override;
    bool updateProfile(int64_t userId, const std::string& fullName, const std::string& bio) override;
    bool updatePassword(int64_t userId, const std::string& newHash, const std::string& newSalt) override;
    bool updateLastActive(int64_t userId) override;

private:
    Database& db_;
    User extractUser(Statement& stmt);
};

} // namespace connectx
