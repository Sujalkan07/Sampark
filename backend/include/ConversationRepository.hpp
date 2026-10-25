#pragma once

#include "Database.hpp"
#include "Models.hpp"
#include <optional>
#include <vector>
#include <string>

namespace connectx {

class IConversationRepository {
public:
    virtual ~IConversationRepository() = default;

    virtual std::optional<Conversation> getOrCreate(int64_t user1Id, int64_t user2Id) = 0;
    virtual std::optional<Conversation> findById(int64_t conversationId) = 0;
    virtual std::optional<Conversation> findByParticipants(int64_t user1Id, int64_t user2Id) = 0;
    virtual std::vector<Conversation> getUserConversations(int64_t userId) = 0;
    virtual bool isParticipant(int64_t conversationId, int64_t userId) = 0;
    virtual int64_t getOtherParticipantId(int64_t conversationId, int64_t userId) = 0;
    virtual bool updateTimestamp(int64_t conversationId) = 0;
};

class SqliteConversationRepository : public IConversationRepository {
public:
    explicit SqliteConversationRepository(Database& db);

    std::optional<Conversation> getOrCreate(int64_t user1Id, int64_t user2Id) override;
    std::optional<Conversation> findById(int64_t conversationId) override;
    std::optional<Conversation> findByParticipants(int64_t user1Id, int64_t user2Id) override;
    std::vector<Conversation> getUserConversations(int64_t userId) override;
    bool isParticipant(int64_t conversationId, int64_t userId) override;
    int64_t getOtherParticipantId(int64_t conversationId, int64_t userId) override;
    bool updateTimestamp(int64_t conversationId) override;

private:
    Database& db_;
    static Conversation extractConversation(Statement& stmt);
};

} // namespace connectx
