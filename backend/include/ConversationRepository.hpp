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

    // Phase 4: Group Chat Extensions
    virtual std::optional<Conversation> createGroup(const std::string& name,
                                                    const std::string& description,
                                                    const std::string& avatarUrl,
                                                    int64_t createdBy,
                                                    const std::vector<int64_t>& memberIds) = 0;
    virtual std::vector<ConversationMember> getGroupMembers(int64_t conversationId) = 0;
    virtual bool addMember(int64_t conversationId, int64_t userId, const std::string& role = "member") = 0;
    virtual bool removeMember(int64_t conversationId, int64_t userId) = 0;
    virtual bool updateGroup(int64_t conversationId, const std::string& name, const std::string& description, const std::string& avatarUrl) = 0;
    virtual bool deleteGroup(int64_t conversationId) = 0;
    virtual std::optional<std::string> getMemberRole(int64_t conversationId, int64_t userId) = 0;
    virtual bool transferAdmin(int64_t conversationId, int64_t currentAdminId, int64_t newAdminId) = 0;
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

    // Phase 4: Group Chat Extensions
    std::optional<Conversation> createGroup(const std::string& name,
                                            const std::string& description,
                                            const std::string& avatarUrl,
                                            int64_t createdBy,
                                            const std::vector<int64_t>& memberIds) override;
    std::vector<ConversationMember> getGroupMembers(int64_t conversationId) override;
    bool addMember(int64_t conversationId, int64_t userId, const std::string& role = "member") override;
    bool removeMember(int64_t conversationId, int64_t userId) override;
    bool updateGroup(int64_t conversationId, const std::string& name, const std::string& description, const std::string& avatarUrl) override;
    bool deleteGroup(int64_t conversationId) override;
    std::optional<std::string> getMemberRole(int64_t conversationId, int64_t userId) override;
    bool transferAdmin(int64_t conversationId, int64_t currentAdminId, int64_t newAdminId) override;

private:
    Database& db_;
    static Conversation extractConversation(Statement& stmt);
};

} // namespace connectx
