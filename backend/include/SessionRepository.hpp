#pragma once

#include "Models.hpp"
#include "Database.hpp"
#include <optional>
#include <string>

namespace connectx {

class ISessionRepository {
public:
    virtual ~ISessionRepository() = default;

    virtual bool createSession(const std::string& token, int64_t userId, int durationHours = 168) = 0;
    virtual std::optional<Session> findSession(const std::string& token) = 0;
    virtual bool deleteSession(const std::string& token) = 0;
    virtual bool deleteUserSessions(int64_t userId) = 0;
    virtual void cleanExpiredSessions() = 0;
};

class SqliteSessionRepository : public ISessionRepository {
public:
    explicit SqliteSessionRepository(Database& db);
    ~SqliteSessionRepository() override = default;

    bool createSession(const std::string& token, int64_t userId, int durationHours = 168) override;
    std::optional<Session> findSession(const std::string& token) override;
    bool deleteSession(const std::string& token) override;
    bool deleteUserSessions(int64_t userId) override;
    void cleanExpiredSessions() override;

private:
    Database& db_;
};

} // namespace connectx
