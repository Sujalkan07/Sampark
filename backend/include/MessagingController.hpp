#pragma once

#include "MessagingService.hpp"
#include <httplib.h>
#include <memory>
#include <string>

namespace connectx {

class MessagingController {
public:
    explicit MessagingController(std::shared_ptr<IMessagingService> messagingService);

    void handleCreateOrGetConversation(const httplib::Request& req, httplib::Response& res);
    void handleListConversations(const httplib::Request& req, httplib::Response& res);
    void handleGetConversation(const httplib::Request& req, httplib::Response& res);
    void handleGetMessages(const httplib::Request& req, httplib::Response& res);
    void handleSendMessage(const httplib::Request& req, httplib::Response& res);
    void handleMarkAsRead(const httplib::Request& req, httplib::Response& res);
    void handleEditMessage(const httplib::Request& req, httplib::Response& res);
    void handleDeleteMessage(const httplib::Request& req, httplib::Response& res);
    void handleSearchMessages(const httplib::Request& req, httplib::Response& res);
    void handleSearchGlobalMessages(const httplib::Request& req, httplib::Response& res);

    // Phase 4: Group Chat Handlers
    void handleCreateGroup(const httplib::Request& req, httplib::Response& res);
    void handleGetGroupMembers(const httplib::Request& req, httplib::Response& res);
    void handleAddGroupMember(const httplib::Request& req, httplib::Response& res);
    void handleRemoveGroupMember(const httplib::Request& req, httplib::Response& res);
    void handleUpdateGroupMemberRole(const httplib::Request& req, httplib::Response& res);
    void handleUpdateGroup(const httplib::Request& req, httplib::Response& res);
    void handleDeleteGroup(const httplib::Request& req, httplib::Response& res);
    void handleLeaveGroup(const httplib::Request& req, httplib::Response& res);

    // Phase 4: Reaction Handlers
    void handleToggleReaction(const httplib::Request& req, httplib::Response& res);

    // Phase 4: Attachment Handlers
    void handleUploadAttachment(const httplib::Request& req, httplib::Response& res);
    void handleGetAttachment(const httplib::Request& req, httplib::Response& res);
    void handleDeleteAttachment(const httplib::Request& req, httplib::Response& res);

    // Phase 4: Preferences Handlers
    void handleGetPreferences(const httplib::Request& req, httplib::Response& res);
    void handleUpdatePreferences(const httplib::Request& req, httplib::Response& res);

    static std::string extractSessionToken(const httplib::Request& req);

private:
    std::shared_ptr<IMessagingService> messagingService_;
    static void sendError(httplib::Response& res, int statusCode, const std::string& message);
};

} // namespace connectx
